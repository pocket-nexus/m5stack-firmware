/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Audio service (svc.h): the codec bring-up of app_main, the amplifier
 * 500 ms later (enable_pa_after_delay), the startup sound 1000 ms after
 * the bring-up when NVS corep4_prefs/boot_sound is set
 * (play_startup_prompt_sound, main/tools.cpp), the output volume, and the
 * REC (audio_rec.c) and MUSIC (audio_music.c) windows. Tasks and locking:
 * audio_svc.h.
 *
 * Requests: rec 0|1, recView <spectrum> <ref>, recStart, recEnd, recPlay,
 * recPlayStop, music 0|1, musicPlay, musicStop, volume <0..100>,
 * bootSound 0|1. Values: volume, bootSound, railAudioPA (when the
 * amplifier is switched on here) and the window values of the modules. */
#include "audio_player.h"
#include "audio_svc.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "m5_corep4x/audio.h"
#include "m5_corep4x/bsp.h"
#include "svc.h"

static const char *TAG = "audio_svc";

#define CTL_STACK 6144
#define UI_STACK 6144
#define CTL_QUEUE_LENGTH 8
#define DEFAULT_VOLUME 80
#define AMPLIFIER_DELAY_MS 500u
#define BOOT_SOUND_DELAY_MS 1000u
/* A codec supply switched back on settles before the bring-up. */
#define RAIL_SETTLE_MS 50u
#define HALT_TIMEOUT_MS 3000u

/* programs/corep4x/main/CMakeLists.txt: the factory's embedded PCM. */
extern const uint8_t _binary_candy_wind_wav_start[];
extern const uint8_t _binary_candy_wind_wav_end[];
extern const uint8_t _binary_startup_wav_start[];
extern const uint8_t _binary_startup_wav_end[];

static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buffer;
static QueueHandle_t s_queue;
static TaskHandle_t s_ui_task;
static volatile int s_volume = DEFAULT_VOLUME;
static volatile bool s_sleeping;
static bool s_boot_sound = true;
static audio_wav_t s_candy_wind;
static audio_wav_t s_startup;

void audio_lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }

void audio_unlock(void) { xSemaphoreGive(s_lock); }

uint32_t audio_now_ms(void) { return pdTICKS_TO_MS(xTaskGetTickCount()); }

bool audio_ready(void) { return !s_sleeping && m5_corep4x_audio_ready(); }

int audio_volume(void) { return s_volume; }

void audio_amplifier_on(void) {
  const esp_err_t err = m5_corep4x_set_switch(M5_COREP4X_SWITCH_SPEAKER, true);
  if (err == ESP_OK) {
    m5_pocket_bridge_set_bool("railAudioPA", true);
  } else {
    ESP_LOGW(TAG, "amplifier: %s", esp_err_to_name(err));
  }
}

const audio_wav_t *audio_track_candy_wind(void) { return &s_candy_wind; }

const audio_wav_t *audio_track_startup(void) { return &s_startup; }

void audio_ui_wake(void) {
  if (s_ui_task) {
    xTaskNotifyGive(s_ui_task);
  }
}

static bool post(const audio_cmd_t *cmd) {
  return s_queue && xQueueSend(s_queue, cmd, 0) == pdTRUE;
}

bool audio_post(audio_cmd_kind_t kind) {
  const audio_cmd_t cmd = {.kind = kind};
  if (!post(&cmd)) {
    ESP_LOGW(TAG, "command %d dropped", (int)kind);
    return false;
  }
  return true;
}

/* ---- audio_ctl --------------------------------------------------------- */

static void bring_up(void) {
  const esp_err_t err = m5_corep4x_audio_init(s_volume);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "audio not ready: %s", esp_err_to_name(err));
  }
}

/* The power service switched AudioIC/AudioPA. The factory leaves its audio
 * pipeline running on an unpowered codec and asks for a restart; here the
 * work stops and the codecs and I2S0 are released (pins high impedance)
 * when the codec loses power, and brought up again when it returns. The
 * amplifier needs nothing: it only gates the speaker. */
static void rails(bool codec_on) {
  if (!codec_on) {
    if (m5_corep4x_audio_ready()) {
      (void)audio_player_stop();
      audio_rec_halt(HALT_TIMEOUT_MS);
      (void)m5_corep4x_audio_deinit(false);
      ESP_LOGI(TAG, "codec supply off: audio released");
    }
  } else if (!m5_corep4x_audio_ready() && !s_sleeping) {
    vTaskDelay(pdMS_TO_TICKS(RAIL_SETTLE_MS));
    bring_up();
  }
}

static void ctl_task(void *arg) {
  (void)arg;
  bring_up();
  const uint32_t up = audio_now_ms();
  uint32_t amplifier_at = audio_ready() ? up + AMPLIFIER_DELAY_MS : 0;
  uint32_t boot_sound_at =
      audio_ready() && s_boot_sound ? up + BOOT_SOUND_DELAY_MS : 0;
  if (!s_boot_sound) {
    ESP_LOGI(TAG, "startup sound disabled");
  }
  for (;;) {
    TickType_t wait = portMAX_DELAY;
    const uint32_t now = audio_now_ms();
    if (amplifier_at || boot_sound_at) {
      const uint32_t next =
          amplifier_at ? amplifier_at : boot_sound_at; /* amplifier first */
      wait = (int32_t)(next - now) > 0 ? pdMS_TO_TICKS(next - now) : 0;
    }
    audio_cmd_t cmd;
    if (xQueueReceive(s_queue, &cmd, wait) == pdTRUE) {
      switch (cmd.kind) {
      case AUDIO_CMD_MUSIC_PLAY:
        audio_music_ctl_play();
        break;
      case AUDIO_CMD_MUSIC_STOP:
        audio_music_ctl_stop();
        break;
      case AUDIO_CMD_REC_RESTORE:
        audio_rec_ctl_restore();
        break;
      case AUDIO_CMD_REC_DELETE:
        audio_rec_ctl_delete();
        break;
      case AUDIO_CMD_RAILS:
        if (!cmd.codec_on) {
          amplifier_at = 0;
          boot_sound_at = 0;
        }
        rails(cmd.codec_on);
        audio_ui_wake();
        break;
      }
    }
    const uint32_t later = audio_now_ms();
    if (amplifier_at && (int32_t)(later - amplifier_at) >= 0) {
      amplifier_at = 0;
      if (audio_ready()) {
        audio_amplifier_on();
      }
    }
    if (!amplifier_at && boot_sound_at &&
        (int32_t)(later - boot_sound_at) >= 0) {
      boot_sound_at = 0;
      const esp_err_t err = audio_player_play_wav(&s_startup, NULL);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "startup sound skipped: %s", esp_err_to_name(err));
      }
    }
  }
}

/* ---- audio_ui ---------------------------------------------------------- */

static void ui_task(void *arg) {
  (void)arg;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    if (!audio_rec_is_open() && !audio_music_is_open()) {
      (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      last = xTaskGetTickCount();
    } else {
      vTaskDelayUntil(&last, pdMS_TO_TICKS(AUDIO_TICK_MS));
    }
    audio_rec_tick();
    audio_music_tick();
  }
}

/* ---- service ----------------------------------------------------------- */

static void parse_track(const char *name, const uint8_t *start,
                        const uint8_t *end, audio_wav_t *out) {
  if (!audio_wav_parse(start, (size_t)(end - start), out)) {
    ESP_LOGE(TAG, "%s: not a 16-bit PCM WAV", name);
  }
}

void audio_svc_start(void) {
  s_lock = xSemaphoreCreateMutexStatic(&s_lock_buffer);
  s_queue = xQueueCreate(CTL_QUEUE_LENGTH, sizeof(audio_cmd_t));
  parse_track("CandyWind", _binary_candy_wind_wav_start,
              _binary_candy_wind_wav_end, &s_candy_wind);
  parse_track("startup", _binary_startup_wav_start, _binary_startup_wav_end,
              &s_startup);
  s_boot_sound = svc_pref_get("boot_sound", true);
  m5_pocket_bridge_set_bool("bootSound", s_boot_sound);
  m5_pocket_bridge_set_number("volume", s_volume);
  if (!s_queue ||
      xTaskCreatePinnedToCore(ui_task, "audio_ui", UI_STACK, NULL,
                              AUDIO_UI_PRIORITY, &s_ui_task,
                              AUDIO_CORE) != pdPASS ||
      xTaskCreatePinnedToCore(ctl_task, "audio_ctl", CTL_STACK, NULL,
                              AUDIO_PRIORITY, NULL, AUDIO_CORE) != pdPASS) {
    ESP_LOGE(TAG, "no audio tasks");
  }
}

void audio_svc_frame(void) { audio_rec_upload(); }

bool audio_svc_request(const m5_pocket_request *request) {
  if (!s_lock) {
    return false;
  }
  if (svc_is(request, "rec")) {
    audio_rec_open(svc_arg(request, 0) != 0);
  } else if (svc_is(request, "recView")) {
    audio_rec_view(svc_arg(request, 0) != 0, svc_arg(request, 1) != 0);
  } else if (svc_is(request, "recStart")) {
    audio_rec_start();
  } else if (svc_is(request, "recEnd")) {
    audio_rec_end();
  } else if (svc_is(request, "recPlay")) {
    audio_rec_play();
  } else if (svc_is(request, "recPlayStop")) {
    audio_rec_play_stop();
  } else if (svc_is(request, "music")) {
    audio_music_open(svc_arg(request, 0) != 0);
  } else if (svc_is(request, "musicPlay")) {
    audio_music_play();
  } else if (svc_is(request, "musicStop")) {
    audio_music_stop();
  } else if (svc_is(request, "volume")) {
    /* set_volume: 0..100, not saved. */
    int volume = svc_arg(request, 0);
    volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    s_volume = volume;
    if (audio_ready()) {
      (void)m5_corep4x_audio_set_volume(volume);
    }
    m5_pocket_bridge_set_number("volume", volume);
  } else if (svc_is(request, "bootSound")) {
    /* set_startup_prompt_audio_enabled: NVS, next boot. */
    s_boot_sound = svc_arg(request, 0) != 0;
    svc_pref_set("boot_sound", s_boot_sound);
    m5_pocket_bridge_set_bool("bootSound", s_boot_sound);
  } else {
    return false;
  }
  return true;
}

void audio_svc_rails_changed(bool codec_on, bool amplifier_on) {
  const audio_cmd_t cmd = {
      .kind = AUDIO_CMD_RAILS,
      .codec_on = codec_on,
      .amplifier_on = amplifier_on,
  };
  if (!post(&cmd)) {
    ESP_LOGW(TAG, "rail change dropped");
  }
}

/* sleep_before_work step 5: audio_system_deinit(true), then the I2S pins
 * GPIO 6, 2, 3, 5, 4 to high impedance (m5_corep4x_audio_deinit). */
void audio_svc_sleep(void) {
  s_sleeping = true;
  if (s_lock) {
    (void)audio_player_stop();
    audio_rec_halt(HALT_TIMEOUT_MS);
  }
  (void)m5_corep4x_audio_deinit(true);
}
