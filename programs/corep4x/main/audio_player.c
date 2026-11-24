/*
 * SPDX-FileCopyrightText: 2025 exia
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* The asynchronous player (audio_player.h) after play_audio_file_impl,
 * play_audio_buffer_impl, playback_task_entry, buffer_playback_task_entry,
 * stop_async_playback and clear_audio_pipeline of M5CoreP4X-UserDemo
 * (components/otool_esp_component/otool_audio/audio_playback.cpp). */
#include "audio_player.h"

#include "audio_svc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "m5_corep4x/audio.h"

static const char *TAG = "audio_player";

#define PLAYER_STACK 4096
/* write_stream_converted_i16: 512 output frames per write. */
#define STREAM_FRAMES 512u
/* play_audio_buffer_impl: 4096-byte writes. */
#define RAW_CHUNK 4096u
#define SILENCE_CHUNK 1024u
#define STOP_TIMEOUT_MS 3000u

typedef enum {
  RESULT_DONE,
  RESULT_STOPPED,
  RESULT_ERROR,
} result_t;

static struct {
  volatile bool running;
  volatile bool stop;
  volatile uint32_t generation;
  volatile bool progress_valid;
  volatile uint32_t elapsed_ms;
  volatile uint32_t duration_ms;
  const audio_wav_t *wav;
  uint8_t *frames;
  size_t bytes;
} s_player;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static const uint8_t s_silence[SILENCE_CHUNK];

static uint32_t frames_to_ms(uint64_t frames) {
  return (uint32_t)(frames * 1000u / M5_COREP4X_AUDIO_SAMPLE_RATE);
}

static void set_elapsed(uint64_t frames) {
  const uint32_t ms = frames_to_ms(frames);
  s_player.elapsed_ms = ms > s_player.duration_ms ? s_player.duration_ms : ms;
}

static result_t stream_wav(const audio_wav_t *wav) {
  const uint64_t total = audio_convert_length(wav);
  s_player.duration_ms = frames_to_ms(total);
  s_player.elapsed_ms = 0;
  s_player.progress_valid = true;
  int32_t *chunk = heap_caps_malloc(STREAM_FRAMES * AUDIO_FRAME_BYTES,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!chunk) {
    chunk =
        heap_caps_malloc(STREAM_FRAMES * AUDIO_FRAME_BYTES, MALLOC_CAP_8BIT);
  }
  if (!chunk) {
    ESP_LOGE(TAG, "no memory for the stream chunk");
    return RESULT_ERROR;
  }
  result_t result = RESULT_DONE;
  for (uint64_t frame = 0; frame < total;) {
    if (s_player.stop) {
      result = RESULT_STOPPED;
      break;
    }
    const size_t count =
        total - frame < STREAM_FRAMES ? (size_t)(total - frame) : STREAM_FRAMES;
    audio_convert(wav, frame, count, chunk);
    const esp_err_t err =
        m5_corep4x_audio_write(chunk, count * AUDIO_FRAME_BYTES);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "write at frame %llu: %s", (unsigned long long)frame,
               esp_err_to_name(err));
      result = RESULT_ERROR;
      break;
    }
    frame += count;
    set_elapsed(frame);
  }
  heap_caps_free(chunk);
  return result;
}

static result_t stream_frames(const uint8_t *frames, size_t bytes) {
  s_player.duration_ms = frames_to_ms(bytes / AUDIO_FRAME_BYTES);
  s_player.elapsed_ms = 0;
  s_player.progress_valid = s_player.duration_ms > 0;
  for (size_t done = 0; done < bytes;) {
    if (s_player.stop) {
      return RESULT_STOPPED;
    }
    const size_t count = bytes - done < RAW_CHUNK ? bytes - done : RAW_CHUNK;
    const esp_err_t err = m5_corep4x_audio_write(frames + done, count);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "write at byte %u: %s", (unsigned)done,
               esp_err_to_name(err));
      return RESULT_ERROR;
    }
    done += count;
    set_elapsed(done / AUDIO_FRAME_BYTES);
  }
  return RESULT_DONE;
}

/* clear_audio_pipeline: `ms` of silence, then ms + 50 ms. */
static esp_err_t clear_pipeline(uint32_t ms) {
  size_t remaining =
      (size_t)M5_COREP4X_AUDIO_SAMPLE_RATE * AUDIO_FRAME_BYTES * ms / 1000u;
  while (remaining > 0) {
    const size_t count =
        remaining < SILENCE_CHUNK ? remaining : (size_t)SILENCE_CHUNK;
    const esp_err_t err = m5_corep4x_audio_write(s_silence, count);
    if (err != ESP_OK) {
      return err;
    }
    remaining -= count;
  }
  vTaskDelay(pdMS_TO_TICKS(ms + 50));
  return ESP_OK;
}

static void set_volume(int volume) {
  (void)m5_corep4x_audio_set_volume(volume);
}

static void player_task(void *arg) {
  (void)arg;
  const result_t result = s_player.wav
                              ? stream_wav(s_player.wav)
                              : stream_frames(s_player.frames, s_player.bytes);
  if (result == RESULT_DONE) {
    set_volume(10);
    vTaskDelay(pdMS_TO_TICKS(20));
    (void)clear_pipeline(200);
    set_volume(audio_volume());
  } else if (result == RESULT_STOPPED) {
    /* The stop path of the *_impl function, then the one of the task
     * entry. */
    set_volume(0);
    vTaskDelay(pdMS_TO_TICKS(20));
    (void)clear_pipeline(150);
    set_volume(audio_volume());
    set_volume(0);
    vTaskDelay(pdMS_TO_TICKS(30));
    if (clear_pipeline(200) == ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(30));
      (void)clear_pipeline(100);
    }
    set_volume(audio_volume());
  }
  if (s_player.frames) {
    heap_caps_free(s_player.frames);
  }
  s_player.frames = NULL;
  s_player.wav = NULL;
  s_player.running = false;
  vTaskDelete(NULL);
}

static esp_err_t start(const audio_wav_t *wav, uint8_t *frames, size_t bytes,
                       uint32_t *generation) {
  taskENTER_CRITICAL(&s_mux);
  const bool busy = s_player.running;
  s_player.running = true;
  taskEXIT_CRITICAL(&s_mux);
  if (busy) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!audio_ready()) {
    s_player.running = false;
    return ESP_ERR_INVALID_STATE;
  }
  s_player.stop = false;
  s_player.generation++;
  s_player.progress_valid = false;
  s_player.elapsed_ms = 0;
  s_player.duration_ms = 0;
  s_player.wav = wav;
  s_player.frames = frames;
  s_player.bytes = bytes;
  if (xTaskCreatePinnedToCore(player_task, "audio_play", PLAYER_STACK, NULL,
                              AUDIO_PRIORITY, NULL, AUDIO_CORE) != pdPASS) {
    s_player.frames = NULL;
    s_player.wav = NULL;
    s_player.running = false;
    return ESP_ERR_NO_MEM;
  }
  if (generation) {
    *generation = s_player.generation;
  }
  return ESP_OK;
}

esp_err_t audio_player_play_wav(const audio_wav_t *wav, uint32_t *generation) {
  if (!wav || !wav->samples) {
    return ESP_ERR_NOT_FOUND;
  }
  return start(wav, NULL, 0, generation);
}

esp_err_t audio_player_play_frames(uint8_t *frames, size_t bytes,
                                   uint32_t *generation) {
  if (!frames || bytes < AUDIO_FRAME_BYTES) {
    return ESP_ERR_INVALID_ARG;
  }
  return start(NULL, frames, bytes - bytes % AUDIO_FRAME_BYTES, generation);
}

esp_err_t audio_player_stop(void) {
  if (!s_player.running) {
    return ESP_OK;
  }
  s_player.stop = true;
  const uint32_t begin = audio_now_ms();
  while (s_player.running) {
    if (audio_now_ms() - begin > STOP_TIMEOUT_MS) {
      ESP_LOGW(TAG, "playback did not stop within %u ms",
               (unsigned)STOP_TIMEOUT_MS);
      return ESP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(AUDIO_TICK_MS));
  }
  return ESP_OK;
}

bool audio_player_running(void) { return s_player.running; }

bool audio_player_running_id(uint32_t generation) {
  return s_player.running && s_player.generation == generation;
}

bool audio_player_progress(uint32_t *elapsed_ms, uint32_t *duration_ms) {
  if (!s_player.progress_valid || s_player.duration_ms == 0) {
    *elapsed_ms = 0;
    *duration_ms = 0;
    return false;
  }
  *duration_ms = s_player.duration_ms;
  const uint32_t elapsed = s_player.elapsed_ms;
  *elapsed_ms = elapsed > *duration_ms ? *duration_ms : elapsed;
  return true;
}
