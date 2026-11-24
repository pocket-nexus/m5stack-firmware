/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* REC window, board side, after ui_function_rec_window.cpp of
 * M5CoreP4X-UserDemo: the session state machine (start_rec_action,
 * end_rec_to_initial_state, play_button_event_cb, build_content,
 * rec_content_delete_event_cb), the workers (monitor_task, rec_task,
 * rec_play_task, restore_record_file_from_storage) and the 50 ms
 * waveform_timer_cb, whose charts and spectrograms go into the texture
 * "assets/rec-canvas.png" (audio_draw.c).
 *
 * Published: recState (0 idle, 1 recording, 2 recorded, 3 playing),
 * recStopping, recPlayStopping, recBusy (a recording task runs or the
 * saved file is being read), recPlayBusy, recSpectrum (the view mode; the
 * board switches to Waveform when a recording is saved and when playback
 * starts), recDb0..2 and recTexture.
 *
 * Recording: 10 s of raw I2S frames (16 kHz, 8 bytes per frame, see
 * m5_corep4x/audio.h) into PSRAM, then /webspiffs/REC.PCM on the SPIFFS
 * partition "storage". */
#include "audio_draw.h"
#include "audio_player.h"
#include "audio_svc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/semphr.h"
#include "m5_corep4x/audio.h"
#include "m5_pocket/bridge.h"
#include "svc.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "audio_rec";

#define REC_PARTITION "storage"
#define REC_BASE_PATH "/webspiffs"
#define REC_FILE REC_BASE_PATH "/REC.PCM"
#define REC_SECONDS 10u
#define REC_BYTES                                                              \
  (M5_COREP4X_AUDIO_SAMPLE_RATE * REC_SECONDS * AUDIO_FRAME_BYTES)
/* STORAGE_RESERVE_BYTES */
#define REC_RESERVE (32u * 1024u)
/* rec_task: 4096-byte reads (512 frames, 32 ms); monitor_task: 40 ms. */
#define REC_CHUNK 4096u
#define MONITOR_FRAMES 640u
#define REC_TEXTURE "assets/rec-canvas.png"
#define REC_TASK_STACK 8192
#define PLAY_TASK_STACK 6144
#define MONITOR_TASK_STACK 6144

enum {
  REC_IDLE = 0,
  REC_RECORDING = 1,
  REC_RECORDED = 2,
  REC_PLAYING = 3,
};

/* The session; guarded by audio_lock(). */
static struct {
  bool open;
  bool spectrum;
  bool show_ref;
  bool spectrum_active;
  int state;
  bool rec_running;
  bool restoring;
  bool stop_requested;
  bool discard_requested;
  bool play_running;
  bool stop_play_requested;
  bool monitor_running;
  size_t record_bytes;
  uint32_t playback_duration_ms;
  uint32_t playback_start_ms;
  bool playback_wave_active;
  /* The recorded preview; the serial changes with it. */
  bool preview_ready;
  int16_t preview_y[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS];
  int16_t preview_peak[AUDIO_REC_CHANNELS];
  uint32_t preview_serial;
  /* The newest chunk's values. */
  int16_t latest_peak[AUDIO_REC_CHANNELS];
  int16_t latest_rms[AUDIO_REC_CHANNELS];
  int16_t latest_wave_y[AUDIO_REC_CHANNELS][AUDIO_WAVE_PER_TICK];
  uint8_t latest_spectrum[AUDIO_REC_CHANNELS][AUDIO_SPEC_BINS];
  /* Requests to the UI task: reset the charts / the spectrograms. */
  bool clear_waves;
  bool clear_spectrogram;
} s_rec;

static struct {
  int state;
  int stopping;
  int play_stopping;
  int busy;
  int play_busy;
  int spectrum;
} s_published = {-1, -1, -1, -1, -1, -1};

/* The UI task's charts, spectrograms and card pictures (PSRAM). */
typedef struct {
  int16_t wave[AUDIO_REC_CHANNELS][AUDIO_WAVE_POINTS];
  int16_t rms[AUDIO_REC_CHANNELS][AUDIO_WAVE_POINTS];
  /* lv_chart_set_x_start_point: the oldest point, per chart. */
  int start[AUDIO_REC_CHANNELS];
  rec_spectrogram_t spectrogram[AUDIO_REC_CHANNELS];
  rec_card_t model;
  rec_card_t drawn;
  bool drawn_valid;
  bool recorded_view;
  int rendered_shift;
  uint32_t rendered_serial;
  char db[AUDIO_REC_CHANNELS][8];
} rec_ui_t;

static rec_ui_t *s_ui;
/* front: the newest complete picture; back: the one being drawn. */
static uint16_t *s_front;
static uint16_t *s_back;
static volatile bool s_fresh;
static uint32_t s_last_upload_ms;
static SemaphoreHandle_t s_texture_lock;
static StaticSemaphore_t s_texture_lock_buffer;
/* REC.PCM and the SPIFFS mount. */
static SemaphoreHandle_t s_file_lock;
static StaticSemaphore_t s_file_lock_buffer;
/* DFT tables, built on first use by the monitor or the recording. */
static audio_dft_t *volatile s_dft;
static portMUX_TYPE s_init_mux = portMUX_INITIALIZER_UNLOCKED;

static void create_locks(void) {
  taskENTER_CRITICAL(&s_init_mux);
  if (!s_file_lock) {
    s_file_lock = xSemaphoreCreateMutexStatic(&s_file_lock_buffer);
    s_texture_lock = xSemaphoreCreateMutexStatic(&s_texture_lock_buffer);
  }
  taskEXIT_CRITICAL(&s_init_mux);
}

static void publish_flag(const char *name, int value, int *last) {
  if (value != *last) {
    *last = value;
    m5_pocket_bridge_set_number(name, value);
  }
}

/* Called with audio_lock() held. */
static void publish_locked(void) {
  publish_flag("recState", s_rec.state, &s_published.state);
  publish_flag("recStopping", s_rec.stop_requested, &s_published.stopping);
  publish_flag("recPlayStopping", s_rec.stop_play_requested,
               &s_published.play_stopping);
  publish_flag("recBusy", s_rec.rec_running || s_rec.restoring,
               &s_published.busy);
  publish_flag("recPlayBusy", s_rec.play_running, &s_published.play_busy);
  publish_flag("recSpectrum", s_rec.spectrum, &s_published.spectrum);
}

/* clear_rec_waveforms / clear_rec_spectrogram: the values here, the
 * pictures on the next tick. */
static void clear_waves_locked(void) {
  s_rec.clear_waves = true;
  memset(s_rec.latest_peak, 0, sizeof(s_rec.latest_peak));
  memset(s_rec.latest_rms, 0, sizeof(s_rec.latest_rms));
  for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
    for (int pt = 0; pt < AUDIO_WAVE_PER_TICK; pt++) {
      s_rec.latest_wave_y[ch][pt] = AUDIO_WAVE_ZERO;
    }
  }
}

static void clear_spectrogram_locked(bool activate) {
  s_rec.spectrum_active = activate;
  s_rec.clear_spectrogram = true;
  memset(s_rec.latest_spectrum, 0, sizeof(s_rec.latest_spectrum));
}

/* clear_record_file_state: the recording is gone (the file is deleted by
 * the caller). */
static void forget_record_locked(void) {
  s_rec.record_bytes = 0;
  s_rec.playback_duration_ms = 0;
  s_rec.preview_ready = false;
  s_rec.preview_serial++;
}

static uint32_t record_duration_ms(size_t bytes) {
  return (uint32_t)((uint64_t)bytes * 1000u /
                    (M5_COREP4X_AUDIO_SAMPLE_RATE * AUDIO_FRAME_BYTES));
}

/* ---- storage ----------------------------------------------------------- */

/* ensure_rec_storage_mounted; call with s_file_lock held. The factory
 * mounts without formatting (its image carries a SPIFFS of web assets);
 * this firmware formats a partition that holds no SPIFFS yet. */
static esp_err_t mount_locked(void) {
  if (esp_spiffs_mounted(REC_PARTITION)) {
    return ESP_OK;
  }
  const esp_vfs_spiffs_conf_t conf = {
      .base_path = REC_BASE_PATH,
      .partition_label = REC_PARTITION,
      .max_files = 8,
      .format_if_mount_failed = true,
  };
  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err == ESP_ERR_INVALID_STATE && esp_spiffs_mounted(REC_PARTITION)) {
    err = ESP_OK;
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mount %s: %s", REC_BASE_PATH, esp_err_to_name(err));
    return err;
  }
  size_t total = 0;
  size_t used = 0;
  if (esp_spiffs_info(REC_PARTITION, &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "%s mounted: %u of %u bytes used", REC_BASE_PATH,
             (unsigned)used, (unsigned)total);
  }
  return ESP_OK;
}

static void delete_file_locked(void) {
  if (esp_spiffs_mounted(REC_PARTITION)) {
    (void)remove(REC_FILE);
  }
}

static void delete_file(void) {
  xSemaphoreTake(s_file_lock, portMAX_DELAY);
  delete_file_locked();
  xSemaphoreGive(s_file_lock);
}

static void set_preview(const audio_preview_acc_t *acc, size_t bytes) {
  int16_t wave_y[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS];
  int16_t peak[AUDIO_REC_CHANNELS];
  audio_preview_finish(acc, wave_y, peak);
  audio_lock();
  memcpy(s_rec.preview_y, wave_y, sizeof(wave_y));
  memcpy(s_rec.preview_peak, peak, sizeof(peak));
  s_rec.preview_ready = acc->offset > 0;
  s_rec.preview_serial++;
  s_rec.record_bytes = bytes;
  s_rec.playback_duration_ms = record_duration_ms(bytes);
  audio_unlock();
}

/* ---- measurements ------------------------------------------------------ */

static const audio_dft_t *dft_tables(void) {
  audio_dft_t *tables = s_dft;
  if (tables) {
    return tables;
  }
  tables = heap_caps_malloc(sizeof(*tables), MALLOC_CAP_SPIRAM);
  if (!tables) {
    ESP_LOGE(TAG, "no memory for the spectrum tables");
    return NULL;
  }
  audio_dft_init(tables);
  taskENTER_CRITICAL(&s_init_mux);
  if (!s_dft) {
    s_dft = tables;
    tables = NULL;
  }
  taskEXIT_CRITICAL(&s_init_mux);
  if (tables) {
    heap_caps_free(tables);
  }
  return s_dft;
}

/* update_latest_audio_metrics_from_raw: the latest values of one chunk;
 * spectra only in Spectrum mode, REF's only when shown. */
static void measure(const uint8_t *frames, size_t count, audio_chunk_t *chunk) {
  audio_lock();
  const bool spectrum = s_rec.spectrum;
  const bool ref = s_rec.show_ref;
  audio_unlock();
  audio_chunk_measure(frames, count, spectrum, ref, chunk);
  uint8_t levels[AUDIO_REC_CHANNELS][AUDIO_SPEC_BINS];
  memset(levels, 0, sizeof(levels));
  if (spectrum) {
    const audio_dft_t *tables = dft_tables();
    for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
      if (ch < 2 || ref) {
        audio_spectrum_levels(tables, chunk->spectrum_samples[ch],
                              chunk->spectrum_count[ch], levels[ch]);
      }
    }
  }
  audio_lock();
  memcpy(s_rec.latest_peak, chunk->peak, sizeof(s_rec.latest_peak));
  memcpy(s_rec.latest_rms, chunk->rms, sizeof(s_rec.latest_rms));
  memcpy(s_rec.latest_wave_y, chunk->wave_y, sizeof(s_rec.latest_wave_y));
  if (spectrum) {
    memcpy(s_rec.latest_spectrum, levels, sizeof(levels));
  }
  audio_unlock();
}

/* ---- tasks ------------------------------------------------------------- */

static void monitor_task(void *arg) {
  (void)arg;
  uint8_t *frames = heap_caps_malloc(MONITOR_FRAMES * AUDIO_FRAME_BYTES,
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  audio_chunk_t *chunk = heap_caps_malloc(sizeof(*chunk), MALLOC_CAP_8BIT);
  uint32_t last_error_ms = 0;
  for (;;) {
    /* The check and the flag under one lock: a reopen either finds this
     * task still running or starts a new one. */
    audio_lock();
    const bool busy = s_rec.rec_running || s_rec.play_running;
    if (!s_rec.open || !frames || !chunk) {
      s_rec.monitor_running = false;
      audio_unlock();
      break;
    }
    audio_unlock();
    if (busy || !audio_ready()) {
      vTaskDelay(pdMS_TO_TICKS(AUDIO_TICK_MS));
      continue;
    }
    const esp_err_t err =
        m5_corep4x_audio_read(frames, MONITOR_FRAMES * AUDIO_FRAME_BYTES);
    if (err != ESP_OK) {
      if (audio_now_ms() - last_error_ms > 1000u) {
        last_error_ms = audio_now_ms();
        ESP_LOGW(TAG, "monitor read: %s", esp_err_to_name(err));
      }
      vTaskDelay(pdMS_TO_TICKS(AUDIO_TICK_MS));
      continue;
    }
    measure(frames, MONITOR_FRAMES, chunk);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  heap_caps_free(chunk);
  heap_caps_free(frames);
  vTaskDelete(NULL);
}

static void start_monitor(void) {
  audio_lock();
  const bool start = !s_rec.monitor_running;
  s_rec.monitor_running = true;
  audio_unlock();
  if (start && xTaskCreatePinnedToCore(
                   monitor_task, "rec_mon", MONITOR_TASK_STACK, NULL,
                   AUDIO_MONITOR_PRIORITY, NULL, AUDIO_CORE) != pdPASS) {
    ESP_LOGW(TAG, "no monitor task");
    audio_lock();
    s_rec.monitor_running = false;
    audio_unlock();
  }
}

/* rec_task: record into PSRAM until 10 s, a stop or a close; save unless
 * END REC discarded it. */
static esp_err_t record_and_save(void) {
  xSemaphoreTake(s_file_lock, portMAX_DELAY);
  esp_err_t err = mount_locked();
  if (err == ESP_OK) {
    delete_file_locked();
  }
  xSemaphoreGive(s_file_lock);
  audio_lock();
  forget_record_locked();
  audio_unlock();
  if (err != ESP_OK) {
    return err;
  }
  if (!audio_ready()) {
    return ESP_ERR_INVALID_STATE;
  }
  (void)m5_corep4x_audio_set_mic_gain(0, M5_COREP4X_MIC_GAIN_27DB);
  (void)m5_corep4x_audio_set_mic_gain(1, M5_COREP4X_MIC_GAIN_27DB);
  uint8_t *buffer = heap_caps_malloc(REC_BYTES, MALLOC_CAP_SPIRAM);
  uint8_t *chunk = heap_caps_malloc(REC_CHUNK, MALLOC_CAP_INTERNAL);
  audio_chunk_t *metrics = heap_caps_malloc(sizeof(*metrics), MALLOC_CAP_8BIT);
  audio_preview_acc_t *preview =
      heap_caps_malloc(sizeof(*preview), MALLOC_CAP_8BIT);
  size_t recorded = 0;
  if (!buffer || !chunk || !metrics || !preview) {
    ESP_LOGE(TAG, "no memory for %u bytes", (unsigned)REC_BYTES);
    err = ESP_ERR_NO_MEM;
  }
  while (err == ESP_OK && recorded < REC_BYTES) {
    audio_lock();
    const bool go = s_rec.open && s_rec.rec_running && !s_rec.stop_requested;
    audio_unlock();
    if (!go) {
      break;
    }
    const size_t count =
        REC_BYTES - recorded < REC_CHUNK ? REC_BYTES - recorded : REC_CHUNK;
    err = m5_corep4x_audio_read(chunk, count);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "read: %s", esp_err_to_name(err));
      break;
    }
    memcpy(buffer + recorded, chunk, count);
    recorded += count;
    measure(chunk, count / AUDIO_FRAME_BYTES, metrics);
  }
  audio_lock();
  const bool discard = s_rec.discard_requested;
  const bool stopped = s_rec.stop_requested;
  audio_unlock();
  if (err == ESP_OK && discard) {
    ESP_LOGI(TAG, "REC cleared");
  } else if (err == ESP_OK && recorded > 0 &&
             (recorded >= REC_BYTES || stopped)) {
    size_t total = 0;
    size_t used = 0;
    err = esp_spiffs_info(REC_PARTITION, &total, &used);
    const size_t free_bytes = err == ESP_OK && total > used ? total - used : 0;
    if (err == ESP_OK && free_bytes < recorded + REC_RESERVE) {
      ESP_LOGW(TAG, "storage full: need %u, free %u", (unsigned)recorded,
               (unsigned)free_bytes);
      err = ESP_ERR_NO_MEM;
    }
    if (err == ESP_OK) {
      xSemaphoreTake(s_file_lock, portMAX_DELAY);
      FILE *file = fopen(REC_FILE, "wb");
      size_t saved = 0;
      if (file) {
        while (saved < recorded) {
          const size_t count =
              recorded - saved < REC_CHUNK ? recorded - saved : REC_CHUNK;
          if (fwrite(buffer + saved, 1, count, file) != count) {
            break;
          }
          saved += count;
        }
        fflush(file);
        fclose(file);
      }
      xSemaphoreGive(s_file_lock);
      if (!file || saved != recorded) {
        ESP_LOGW(TAG, "save %u of %u bytes", (unsigned)saved,
                 (unsigned)recorded);
        err = ESP_FAIL;
      }
    }
    if (err == ESP_OK) {
      audio_preview_begin(preview, recorded / AUDIO_FRAME_BYTES);
      audio_preview_feed(preview, buffer, recorded / AUDIO_FRAME_BYTES);
      set_preview(preview, recorded);
      ESP_LOGI(TAG, "REC done | %u B | %s", (unsigned)recorded, REC_FILE);
    }
  } else if (err == ESP_OK) {
    ESP_LOGW(TAG, "REC stopped: no data saved");
    err = ESP_ERR_INVALID_SIZE;
  }
  heap_caps_free(preview);
  heap_caps_free(metrics);
  heap_caps_free(chunk);
  heap_caps_free(buffer);
  return err;
}

static void rec_task(void *arg) {
  (void)arg;
  const esp_err_t err = record_and_save();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "REC failed: %s", esp_err_to_name(err));
    delete_file();
  }
  audio_lock();
  if (err != ESP_OK) {
    forget_record_locked();
  }
  s_rec.state = s_rec.record_bytes > 0 ? REC_RECORDED : REC_IDLE;
  s_rec.rec_running = false;
  s_rec.stop_requested = false;
  s_rec.discard_requested = false;
  /* update_rec_ui_from_task */
  if (s_rec.open) {
    if (s_rec.state == REC_RECORDED) {
      s_rec.spectrum = false;
      clear_spectrogram_locked(false);
    } else {
      clear_waves_locked();
      clear_spectrogram_locked(false);
    }
  }
  publish_locked();
  audio_unlock();
  audio_ui_wake();
  vTaskDelete(NULL);
}

/* rec_play_task: the file into PSRAM, handed to the player; polled every
 * 50 ms until it ends, the window closes or END PLAY. */
static void play_task(void *arg) {
  (void)arg;
  esp_err_t err = audio_ready() ? ESP_OK : ESP_ERR_INVALID_STATE;
  audio_lock();
  const size_t bytes = s_rec.record_bytes;
  audio_unlock();
  if (err == ESP_OK && bytes == 0) {
    err = ESP_ERR_NOT_FOUND;
  }
  uint8_t *buffer = NULL;
  size_t length = 0;
  if (err == ESP_OK) {
    xSemaphoreTake(s_file_lock, portMAX_DELAY);
    FILE *file = fopen(REC_FILE, "rb");
    const bool opened = file != NULL;
    if (file) {
      buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
      if (buffer) {
        length = fread(buffer, 1, bytes, file);
      }
      fclose(file);
    }
    xSemaphoreGive(s_file_lock);
    err = !opened       ? ESP_ERR_NOT_FOUND
          : !buffer     ? ESP_ERR_NO_MEM
          : length == 0 ? ESP_ERR_INVALID_SIZE
                        : ESP_OK;
  }
  audio_lock();
  const bool stop_early = s_rec.stop_play_requested;
  audio_unlock();
  if (err == ESP_OK && !stop_early) {
    uint32_t generation = 0;
    err = audio_player_play_frames(buffer, length, &generation);
    if (err == ESP_OK) {
      buffer = NULL;
      audio_lock();
      s_rec.playback_start_ms = audio_now_ms();
      s_rec.playback_wave_active = true;
      audio_unlock();
      while (audio_player_running_id(generation)) {
        audio_lock();
        const bool stop = !s_rec.open || s_rec.stop_play_requested;
        audio_unlock();
        if (stop) {
          (void)audio_player_stop();
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(AUDIO_TICK_MS));
      }
    }
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "PLAY failed: %s", esp_err_to_name(err));
  }
  heap_caps_free(buffer);
  audio_lock();
  s_rec.play_running = false;
  s_rec.stop_play_requested = false;
  s_rec.playback_wave_active = false;
  /* update_rec_ui_from_play_task */
  if (s_rec.record_bytes > 0) {
    s_rec.state = REC_RECORDED;
  } else {
    s_rec.state = REC_IDLE;
    clear_waves_locked();
    clear_spectrogram_locked(false);
  }
  publish_locked();
  audio_unlock();
  audio_ui_wake();
  vTaskDelete(NULL);
}

/* ---- requests ---------------------------------------------------------- */

static bool alloc_ui(void) {
  if (s_ui && s_front && s_back) {
    return true;
  }
  const size_t picture = (size_t)REC_TEXTURE_SIZE * REC_TEXTURE_SIZE * 2;
  if (!s_ui) {
    s_ui = heap_caps_calloc(1, sizeof(*s_ui), MALLOC_CAP_SPIRAM);
    if (s_ui) {
      s_ui->rendered_shift = -1;
      for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
        rec_spectrogram_clear(&s_ui->spectrogram[ch]);
        strcpy(s_ui->db[ch], "-inf");
      }
    }
  }
  if (!s_front) {
    s_front = heap_caps_malloc(picture, MALLOC_CAP_SPIRAM);
    if (s_front) {
      memset(s_front, 0xFF, picture);
    }
  }
  if (!s_back) {
    s_back = heap_caps_malloc(picture, MALLOC_CAP_SPIRAM);
    if (s_back) {
      memset(s_back, 0xFF, picture);
    }
  }
  if (!s_ui || !s_front || !s_back) {
    ESP_LOGE(TAG, "no memory for the REC card pictures");
    return false;
  }
  return true;
}

void audio_rec_open(bool open) {
  create_locks();
  if (open) {
    (void)alloc_ui();
  }
  bool restore = false;
  audio_lock();
  s_rec.open = open;
  if (open) {
    s_rec.spectrum = false;
    s_rec.show_ref = false;
    s_rec.stop_requested = false;
    s_rec.stop_play_requested = false;
    clear_spectrogram_locked(false);
    if (s_rec.rec_running) {
      s_rec.state = REC_RECORDING;
    } else if (s_rec.play_running) {
      s_rec.state = REC_PLAYING;
    } else {
      /* restore_record_file_from_storage runs on audio_ctl; until it
       * answers the window is idle and busy. */
      restore = true;
      s_rec.restoring = true;
      s_rec.state = REC_IDLE;
      forget_record_locked();
    }
    if (s_rec.state != REC_RECORDED && s_rec.state != REC_PLAYING) {
      clear_waves_locked();
    }
  } else {
    /* rec_content_delete_event_cb: a recording stops and is saved, a
     * playback stops, the monitor ends. */
    s_rec.stop_requested = s_rec.rec_running;
    s_rec.stop_play_requested = s_rec.play_running;
    s_rec.spectrum_active = false;
  }
  publish_locked();
  audio_unlock();
  if (restore && !audio_post(AUDIO_CMD_REC_RESTORE)) {
    audio_lock();
    s_rec.restoring = false;
    publish_locked();
    audio_unlock();
  }
  if (open) {
    start_monitor();
    audio_ui_wake();
  }
}

bool audio_rec_is_open(void) { return s_rec.open; }

void audio_rec_view(bool spectrum, bool show_ref) {
  audio_lock();
  /* A request with the mode and REF unchanged is a mode button pressed
   * again: apply_rec_display_mode runs again. */
  const bool mode_button =
      spectrum != s_rec.spectrum || show_ref == s_rec.show_ref;
  s_rec.spectrum = spectrum;
  s_rec.show_ref = show_ref;
  if (mode_button) {
    if (spectrum) {
      clear_spectrogram_locked(true);
    } else {
      s_rec.spectrum_active = false;
    }
  }
  publish_locked();
  audio_unlock();
  audio_ui_wake();
}

/* end_rec_to_initial_state */
void audio_rec_end(void) {
  audio_lock();
  if (s_rec.rec_running) {
    s_rec.discard_requested = true;
    s_rec.stop_requested = true;
    publish_locked();
    audio_unlock();
    return;
  }
  if (s_rec.play_running) {
    s_rec.stop_play_requested = true;
    s_rec.playback_wave_active = false;
  }
  forget_record_locked();
  s_rec.state = REC_IDLE;
  clear_waves_locked();
  clear_spectrogram_locked(false);
  publish_locked();
  audio_unlock();
  (void)audio_post(AUDIO_CMD_REC_DELETE);
  audio_ui_wake();
}

/* start_rec_action */
void audio_rec_start(void) {
  audio_lock();
  if (!s_rec.open || s_rec.restoring) {
    audio_unlock();
    return;
  }
  if (s_rec.rec_running || s_rec.play_running || s_rec.state != REC_IDLE ||
      s_rec.record_bytes > 0) {
    audio_unlock();
    audio_rec_end();
    return;
  }
  s_rec.rec_running = true;
  s_rec.stop_requested = false;
  s_rec.discard_requested = false;
  s_rec.state = REC_RECORDING;
  clear_spectrogram_locked(true);
  clear_waves_locked();
  publish_locked();
  audio_unlock();
  if (xTaskCreatePinnedToCore(rec_task, "func_rec", REC_TASK_STACK, NULL,
                              AUDIO_PRIORITY, NULL, AUDIO_CORE) != pdPASS) {
    ESP_LOGW(TAG, "REC failed: cannot create worker task");
    audio_lock();
    s_rec.rec_running = false;
    s_rec.state = REC_IDLE;
    publish_locked();
    audio_unlock();
  }
  audio_ui_wake();
}

/* play_button_event_cb */
void audio_rec_play(void) {
  audio_lock();
  if (s_rec.play_running) {
    audio_unlock();
    audio_rec_play_stop();
    return;
  }
  if (!s_rec.open || s_rec.rec_running || s_rec.restoring ||
      s_rec.state != REC_RECORDED || s_rec.record_bytes == 0) {
    audio_unlock();
    return;
  }
  s_rec.play_running = true;
  s_rec.stop_play_requested = false;
  s_rec.state = REC_PLAYING;
  s_rec.playback_duration_ms = record_duration_ms(s_rec.record_bytes);
  s_rec.playback_start_ms = audio_now_ms();
  s_rec.playback_wave_active = true;
  /* display mode forced to Waveform */
  s_rec.spectrum = false;
  s_rec.spectrum_active = false;
  publish_locked();
  audio_unlock();
  if (xTaskCreatePinnedToCore(play_task, "rec_play", PLAY_TASK_STACK, NULL,
                              AUDIO_PRIORITY, NULL, AUDIO_CORE) != pdPASS) {
    ESP_LOGW(TAG, "PLAY failed: cannot create worker task");
    audio_lock();
    s_rec.play_running = false;
    s_rec.playback_wave_active = false;
    s_rec.state = REC_RECORDED;
    publish_locked();
    audio_unlock();
  }
  audio_ui_wake();
}

void audio_rec_play_stop(void) {
  audio_lock();
  if (s_rec.play_running) {
    s_rec.stop_play_requested = true;
    s_rec.playback_wave_active = false;
    publish_locked();
  }
  audio_unlock();
  audio_ui_wake();
}

/* restore_record_file_from_storage, on audio_ctl. */
void audio_rec_ctl_restore(void) {
  create_locks();
  if (audio_ready()) {
    (void)m5_corep4x_audio_set_mic_gain(0, M5_COREP4X_MIC_GAIN_27DB);
    (void)m5_corep4x_audio_set_mic_gain(1, M5_COREP4X_MIC_GAIN_27DB);
  }
  size_t bytes = 0;
  audio_preview_acc_t *preview =
      heap_caps_malloc(sizeof(*preview), MALLOC_CAP_8BIT);
  uint8_t *chunk = heap_caps_malloc(REC_CHUNK, MALLOC_CAP_INTERNAL);
  xSemaphoreTake(s_file_lock, portMAX_DELAY);
  struct stat st;
  if (preview && chunk && mount_locked() == ESP_OK &&
      stat(REC_FILE, &st) == 0 && st.st_size > 0) {
    FILE *file = fopen(REC_FILE, "rb");
    if (file) {
      bytes = (size_t)st.st_size;
      audio_preview_begin(preview, bytes / AUDIO_FRAME_BYTES);
      size_t got;
      while ((got = fread(chunk, 1, REC_CHUNK, file)) > 0) {
        audio_preview_feed(preview, chunk, got / AUDIO_FRAME_BYTES);
      }
      fclose(file);
    }
  }
  xSemaphoreGive(s_file_lock);
  if (bytes > 0) {
    set_preview(preview, bytes);
  }
  heap_caps_free(chunk);
  heap_caps_free(preview);
  audio_lock();
  s_rec.restoring = false;
  if (!s_rec.rec_running && !s_rec.play_running) {
    s_rec.state = bytes > 0 ? REC_RECORDED : REC_IDLE;
  }
  publish_locked();
  audio_unlock();
  audio_ui_wake();
}

void audio_rec_ctl_delete(void) {
  create_locks();
  delete_file();
}

void audio_rec_halt(uint32_t timeout_ms) {
  audio_lock();
  if (s_rec.rec_running) {
    s_rec.discard_requested = true;
    s_rec.stop_requested = true;
  }
  if (s_rec.play_running) {
    s_rec.stop_play_requested = true;
  }
  publish_locked();
  audio_unlock();
  const uint32_t begin = audio_now_ms();
  for (;;) {
    audio_lock();
    const bool busy = s_rec.rec_running || s_rec.play_running;
    audio_unlock();
    if (!busy || audio_now_ms() - begin > timeout_ms) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/* ---- UI tick ----------------------------------------------------------- */

static void fill_charts(int16_t value) {
  for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
    for (int i = 0; i < AUDIO_WAVE_POINTS; i++) {
      s_ui->wave[ch][i] = value;
      s_ui->rms[ch][i] = value;
    }
    s_ui->start[ch] = 0;
  }
}

static void publish_db(int ch, const char *text) {
  static const char *const names[AUDIO_REC_CHANNELS] = {"recDb0", "recDb1",
                                                        "recDb2"};
  if (strcmp(s_ui->db[ch], text) != 0) {
    strcpy(s_ui->db[ch], text);
    m5_pocket_bridge_set_text(names[ch], text);
  }
}

/* waveform_timer_cb */
void audio_rec_tick(void) {
  if (!s_ui || !s_front || !s_back) {
    return;
  }
  /* The session as one snapshot. */
  audio_lock();
  if (!s_rec.open) {
    audio_unlock();
    return;
  }
  const int state = s_rec.state;
  const bool spectrum = s_rec.spectrum;
  const bool show_ref = s_rec.show_ref;
  const bool spectrum_live = s_rec.spectrum && s_rec.spectrum_active;
  const bool clear_waves = s_rec.clear_waves;
  const bool clear_spectrogram = s_rec.clear_spectrogram;
  s_rec.clear_waves = false;
  s_rec.clear_spectrogram = false;
  int16_t latest_peak[AUDIO_REC_CHANNELS];
  int16_t latest_rms[AUDIO_REC_CHANNELS];
  int16_t latest_wave_y[AUDIO_REC_CHANNELS][AUDIO_WAVE_PER_TICK];
  uint8_t latest_spectrum[AUDIO_REC_CHANNELS][AUDIO_SPEC_BINS];
  memcpy(latest_peak, s_rec.latest_peak, sizeof(latest_peak));
  memcpy(latest_rms, s_rec.latest_rms, sizeof(latest_rms));
  memcpy(latest_wave_y, s_rec.latest_wave_y, sizeof(latest_wave_y));
  memcpy(latest_spectrum, s_rec.latest_spectrum, sizeof(latest_spectrum));
  const bool recorded_view = state == REC_RECORDED || state == REC_PLAYING;
  int shift = 0;
  if (state == REC_PLAYING && s_rec.playback_wave_active &&
      s_rec.playback_duration_ms > 0) {
    const uint32_t elapsed = audio_now_ms() - s_rec.playback_start_ms;
    shift = (int)((uint64_t)elapsed * AUDIO_PREVIEW_POINTS /
                  s_rec.playback_duration_ms);
    shift = shift > AUDIO_PREVIEW_POINTS ? AUDIO_PREVIEW_POINTS : shift;
  }
  const uint32_t serial = s_rec.preview_serial;
  const bool preview_ready = s_rec.preview_ready;
  int16_t preview_y[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS];
  int16_t preview_peak[AUDIO_REC_CHANNELS];
  if (recorded_view) {
    memcpy(preview_y, s_rec.preview_y, sizeof(preview_y));
    memcpy(preview_peak, s_rec.preview_peak, sizeof(preview_peak));
  }
  audio_unlock();

  bool spectrogram_changed = false;
  if (clear_waves) {
    fill_charts(AUDIO_WAVE_ZERO);
    s_ui->rendered_shift = -1;
  }
  if (clear_spectrogram) {
    for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
      rec_spectrogram_clear(&s_ui->spectrogram[ch]);
    }
    spectrogram_changed = true;
  }
  if (recorded_view) {
    /* render_recorded_waveform_view: the preview from point 67 on,
     * shifted left during playback. */
    if (!s_ui->recorded_view || s_ui->rendered_serial != serial ||
        s_ui->rendered_shift != shift) {
      fill_charts(AUDIO_WAVE_ZERO);
      for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
        for (int src = 0; preview_ready && src < AUDIO_PREVIEW_POINTS; src++) {
          const int dst = AUDIO_PREVIEW_ANCHOR + 1 + src - shift;
          if (dst >= 0 && dst < AUDIO_WAVE_POINTS) {
            s_ui->wave[ch][dst] = preview_y[ch][src];
          }
        }
        char text[8];
        audio_db_text(preview_ready ? preview_peak[ch] : 0, false, text);
        publish_db(ch, text);
      }
      s_ui->rendered_serial = serial;
      s_ui->rendered_shift = shift;
    }
  } else {
    /* Live: 4 points and the RMS line per visible chart, the dB label
     * with its -60 dBFS floor. */
    for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
      if (spectrum || (ch == 2 && !show_ref)) {
        continue;
      }
      const int16_t rms_y = audio_wave_y(latest_rms[ch]);
      for (int pt = 0; pt < AUDIO_WAVE_PER_TICK; pt++) {
        const int index = (s_ui->start[ch] + pt) % AUDIO_WAVE_POINTS;
        s_ui->wave[ch][index] = latest_wave_y[ch][pt];
        s_ui->rms[ch][index] = rms_y;
      }
      s_ui->start[ch] =
          (s_ui->start[ch] + AUDIO_WAVE_PER_TICK) % AUDIO_WAVE_POINTS;
      char text[8];
      audio_db_text(latest_peak[ch], true, text);
      publish_db(ch, text);
    }
    s_ui->rendered_shift = -1;
  }
  s_ui->recorded_view = recorded_view;
  if (spectrum_live) {
    for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
      if (ch == 2 && !show_ref) {
        continue;
      }
      rec_spectrogram_push(&s_ui->spectrogram[ch], latest_spectrum[ch]);
      spectrogram_changed = true;
    }
  }

  rec_card_t *model = &s_ui->model;
  model->spectrum = spectrum;
  model->show_ref = show_ref;
  model->markers = recorded_view;
  for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
    for (int i = 0; i < AUDIO_WAVE_POINTS; i++) {
      const int index = (s_ui->start[ch] + i) % AUDIO_WAVE_POINTS;
      model->wave[ch][i] = s_ui->wave[ch][index];
      model->rms[ch][i] = s_ui->rms[ch][index];
    }
    model->spectrogram[ch] = &s_ui->spectrogram[ch];
  }
  const bool changed =
      !s_ui->drawn_valid ||
      (spectrum
           ? spectrogram_changed || model->show_ref != s_ui->drawn.show_ref ||
                 !s_ui->drawn.spectrum
           : memcmp(model, &s_ui->drawn, sizeof(*model)) != 0);
  if (!changed) {
    return;
  }
  rec_card_draw(s_back, model);
  s_ui->drawn = *model;
  s_ui->drawn_valid = true;
  xSemaphoreTake(s_texture_lock, portMAX_DELAY);
  uint16_t *done = s_back;
  s_back = s_front;
  s_front = done;
  s_fresh = true;
  xSemaphoreGive(s_texture_lock);
}

void audio_rec_upload(void) {
  if (!s_fresh || !s_texture_lock) {
    return;
  }
  const uint32_t now = audio_now_ms();
  if (now - s_last_upload_ms < AUDIO_TICK_MS) {
    return;
  }
  xSemaphoreTake(s_texture_lock, portMAX_DELAY);
  if (s_fresh) {
    const esp_err_t err = svc_texture(REC_TEXTURE, s_front, REC_TEXTURE_SIZE,
                                      REC_TEXTURE_SIZE, "recTexture");
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "texture: %s", esp_err_to_name(err));
    }
    s_fresh = false;
    s_last_upload_ms = now;
  }
  xSemaphoreGive(s_texture_lock);
}
