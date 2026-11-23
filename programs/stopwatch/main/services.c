/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Board services behind the device bridge: settings and alarms in NVS with
 * the factory keys and blob layout (M5StopWatch-UserDemo hal.cpp,
 * hal_alarm.cpp), the RTC wall clock, battery, keys and IMU publication,
 * the factory tone generator (apps/common/audio/audio.cpp) and boot sound,
 * and the microphone spectrum task.
 */
#include "services.h"
#include "badge.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "m5_pocket/app.h"
#include "m5_stopwatch/bsp.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "spectrum.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "services";

#define BUTTON_A_BIT 0x0100u /* BTN.LTRIGGER */
#define BUTTON_B_BIT 0x0200u /* BTN.RTRIGGER */
#define MAX_ALARMS 16
#define SAMPLE_RATE M5_STOPWATCH_AUDIO_SAMPLE_RATE

extern const uint8_t boot_sfx_start[] asm("_binary_boot_sfx_bin_start");
extern const uint8_t boot_sfx_end[] asm("_binary_boot_sfx_bin_end");

/* ------------------------------- settings -------------------------------- */

typedef struct {
  int brightness, volume;
  bool sfx, vibrate, wifi;
  int launch_count;
} settings_t;

static settings_t settings = {80, 80, true, true, false, 0};

static int32_t nvs_get_int(nvs_handle_t nvs, const char *key,
                           int32_t fallback) {
  int32_t value = fallback;
  if (nvs_get_i32(nvs, key, &value) != ESP_OK) {
    return fallback;
  }
  return value;
}

static bool nvs_get_flag(nvs_handle_t nvs, const char *key, bool fallback) {
  uint8_t value = fallback;
  if (nvs_get_u8(nvs, key, &value) != ESP_OK) {
    return fallback;
  }
  return value != 0;
}

static void settings_load(void) {
  nvs_handle_t nvs;
  if (nvs_open("system", NVS_READONLY, &nvs) != ESP_OK) {
    return;
  }
  int brightness = nvs_get_int(nvs, "bl_lev", 80);
  settings.brightness = brightness < 10    ? 10
                        : brightness > 100 ? 100
                                           : brightness;
  int volume = nvs_get_int(nvs, "spk_vol", 80);
  settings.volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
  settings.sfx = nvs_get_flag(nvs, "btn_sfx", true);
  settings.vibrate = nvs_get_flag(nvs, "btn_vibrate", true);
  settings.wifi = nvs_get_flag(nvs, "wifi_enabled", false);
  settings.launch_count = nvs_get_int(nvs, "launch_count", 0);
  nvs_close(nvs);
}

static void settings_save_int(const char *key, int32_t value) {
  nvs_handle_t nvs;
  if (nvs_open("system", NVS_READWRITE, &nvs) != ESP_OK) {
    return;
  }
  nvs_set_i32(nvs, key, value);
  nvs_commit(nvs);
  nvs_close(nvs);
}

static void settings_save_flag(const char *key, bool value) {
  nvs_handle_t nvs;
  if (nvs_open("system", NVS_READWRITE, &nvs) != ESP_OK) {
    return;
  }
  nvs_set_u8(nvs, key, value ? 1 : 0);
  nvs_commit(nvs);
  nvs_close(nvs);
}

/* -------------------------------- alarms --------------------------------- */

/* Factory AlarmStorageSnapshot: version, count, reserved, 16 entries of
 * {hour, minute, enabled, reserved}; 68 bytes in namespace "alarm". */
typedef struct {
  uint8_t version, count;
  uint16_t reserved;
  uint8_t entries[MAX_ALARMS][4];
} alarm_blob_t;

static alarm_blob_t alarms_saved, alarms_pending;

static void alarms_load(void) {
  memset(&alarms_saved, 0, sizeof(alarms_saved));
  nvs_handle_t nvs;
  if (nvs_open("alarm", NVS_READONLY, &nvs) != ESP_OK) {
    return;
  }
  size_t size = sizeof(alarms_saved);
  if (nvs_get_blob(nvs, "storage", &alarms_saved, &size) != ESP_OK ||
      size != sizeof(alarms_saved) || alarms_saved.version != 1 ||
      alarms_saved.count > MAX_ALARMS) {
    memset(&alarms_saved, 0, sizeof(alarms_saved));
  }
  nvs_close(nvs);
}

static void alarms_publish(void) {
  float hours[MAX_ALARMS], minutes[MAX_ALARMS], enabled[MAX_ALARMS];
  size_t count = 0;
  for (size_t i = 0; i < alarms_saved.count; i++) {
    const uint8_t *e = alarms_saved.entries[i];
    if (e[0] < 24 && e[1] < 60 && e[2] <= 1) {
      hours[count] = e[0];
      minutes[count] = e[1];
      enabled[count] = e[2];
      count++;
    }
  }
  m5_pocket_bridge_set_array("alarmHours", hours, count);
  m5_pocket_bridge_set_array("alarmMinutes", minutes, count);
  m5_pocket_bridge_set_array("alarmEnabled", enabled, count);
}

static void alarms_save(void) {
  alarms_pending.version = 1;
  alarms_saved = alarms_pending;
  nvs_handle_t nvs;
  if (nvs_open("alarm", NVS_READWRITE, &nvs) != ESP_OK) {
    return;
  }
  nvs_set_blob(nvs, "storage", &alarms_saved, sizeof(alarms_saved));
  nvs_commit(nvs);
  nvs_close(nvs);
}

/* --------------------------------- audio --------------------------------- */

typedef struct {
  int kind; /* 0 tone, 1 boot sound */
  float frequency;
  float seconds;
} audio_job_t;

static QueueHandle_t audio_queue;
static TaskHandle_t mic_task_handle;
static volatile bool mic_on;
static bool audio_ready;

/* Factory audio::play_tone: a sine at 32767 / 5 × 0.5 with a 200-sample
 * linear fade-out; a new job cuts the current one. */
static void play_tone(const audio_job_t *job) {
  static int16_t chunk[256];
  const size_t total = (size_t)(job->seconds * SAMPLE_RATE);
  const float amplitude = 32767.0f / 5.0f * 0.5f;
  for (size_t done = 0; done < total;) {
    const size_t n = total - done > 256 ? 256 : total - done;
    for (size_t i = 0; i < n; i++) {
      const size_t index = done + i;
      float gain = 1.0f;
      if (total - index <= 200) {
        gain = (float)(total - index) / 200.0f;
      }
      chunk[i] = (int16_t)(amplitude * gain *
                           sinf(2.0f * (float)M_PI * job->frequency *
                                (float)index / SAMPLE_RATE));
    }
    m5_stopwatch_audio_write(chunk, n);
    done += n;
    if (uxQueueMessagesWaiting(audio_queue) > 0) {
      return;
    }
  }
}

static void play_boot(void) {
  const int16_t *samples = (const int16_t *)boot_sfx_start;
  const size_t total = (size_t)(boot_sfx_end - boot_sfx_start) / 2;
  for (size_t done = 0; done < total; done += 512) {
    const size_t n = total - done > 512 ? 512 : total - done;
    m5_stopwatch_audio_write(samples + done, n);
    if (uxQueueMessagesWaiting(audio_queue) > 0) {
      return;
    }
  }
}

static void audio_task(void *arg) {
  (void)arg;
  audio_job_t job;
  for (;;) {
    if (xQueueReceive(audio_queue, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (settings.volume <= 0) {
      continue;
    }
    if (job.kind == 0) {
      play_tone(&job);
    } else {
      play_boot();
    }
  }
}

static void mic_task(void *arg) {
  (void)arg;
  static int16_t hop[STOPWATCH_SPECTRUM_HOP];
  stopwatch_spectrum_frame frame;
  for (;;) {
    if (!mic_on) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      stopwatch_spectrum_reset();
      continue;
    }
    if (m5_stopwatch_audio_read(hop, STOPWATCH_SPECTRUM_HOP) != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    stopwatch_spectrum_push(hop, &frame);
    m5_pocket_bridge_set_array("spectrum", frame.bands,
                               STOPWATCH_SPECTRUM_BANDS);
    m5_pocket_bridge_set_number("peakHz", frame.peak_hz);
  }
}

static void audio_post(int kind, float frequency, float seconds) {
  if (!audio_ready) {
    return;
  }
  const audio_job_t job = {kind, frequency, seconds};
  xQueueReset(audio_queue);
  xQueueSend(audio_queue, &job, 0);
}

/* --------------------------------- clock --------------------------------- */

static void clock_from_rtc(void) {
  struct tm now;
  if (m5_stopwatch_rtc_get(&now) != ESP_OK) {
    ESP_LOGW(TAG, "RTC read failed");
    return;
  }
  setenv("TZ", "GMT0", 1);
  tzset();
  const struct timeval tv = {.tv_sec = mktime(&now), .tv_usec = 0};
  settimeofday(&tv, NULL);
}

/* ------------------------------ publication ------------------------------ */

static int64_t last_battery_us = -2000000;

esp_err_t stopwatch_services_start(void) {
  settings_load();
  alarms_load();
  clock_from_rtc();
  if (m5_stopwatch_imu_init() != ESP_OK) {
    ESP_LOGW(TAG, "IMU init failed");
  }
  m5_stopwatch_display_set_brightness((uint8_t)settings.brightness);
  if (m5_stopwatch_audio_init() == ESP_OK) {
    m5_stopwatch_audio_set_volume((uint8_t)settings.volume);
    m5_stopwatch_audio_set_mic_gain(30.0f);
    m5_stopwatch_audio_speaker_enable(true);
    audio_queue = xQueueCreate(1, sizeof(audio_job_t));
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(mic_task, "mic", 6144, NULL, 4, &mic_task_handle,
                            0);
    audio_ready = true;
  } else {
    ESP_LOGW(TAG, "audio init failed");
  }

  m5_pocket_bridge_set_number("brightness", settings.brightness);
  m5_pocket_bridge_set_number("volume", settings.volume);
  m5_pocket_bridge_set_bool("buttonSfx", settings.sfx);
  m5_pocket_bridge_set_bool("buttonVibrate", settings.vibrate);
  m5_pocket_bridge_set_bool("wifiEnabled", settings.wifi);
  m5_pocket_bridge_set_number("guideLaunchCount", settings.launch_count);
  alarms_publish();
  if (stopwatch_badge_init() != ESP_OK) {
    ESP_LOGW(TAG, "badge storage unavailable");
  }
  stopwatch_services_before_frame();
  return ESP_OK;
}

uint32_t stopwatch_services_buttons(void) {
  uint32_t bits = 0;
  if (!gpio_get_level(M5_STOPWATCH_PIN_BUTTON_A)) {
    bits |= BUTTON_A_BIT;
  }
  if (!gpio_get_level(M5_STOPWATCH_PIN_BUTTON_B)) {
    bits |= BUTTON_B_BIT;
  }
  return bits;
}

static m5_pocket_app *running_app;

void stopwatch_services_set_app(m5_pocket_app *app) { running_app = app; }

typedef struct {
  stopwatch_badge_image image;
  bool changed;
} badge_take;

static void badge_take_image(void *argument) {
  badge_take *take = argument;
  take->changed = stopwatch_badge_take_image(&take->image);
}

/* The one place that hands the badge picture to PocketJS, in both modes:
 * m5_pocket_app_set_texture uploads a newly decoded picture and binds it to
 * the image name the badge view draws; the published handle makes the view
 * remount it. */
static void badge_texture_update(void) {
  if (!running_app || !stopwatch_badge_image_pending()) {
    return;
  }
  /* The decode reads the FAT partition: on an internal stack. */
  badge_take take = {.changed = false};
  if (m5_pocket_app_run_internal(badge_take_image, &take) != ESP_OK ||
      !take.changed) {
    return;
  }
  stopwatch_badge_image image = take.image;
  int32_t handle = -1;
  if (m5_pocket_app_set_texture(running_app, "images/badge_slot.png",
                                image.pixels, image.canvas_w, image.canvas_h,
                                (size_t)image.canvas_w * sizeof(uint16_t),
                                &handle) == ESP_OK &&
      image.pixels) {
    m5_pocket_bridge_set_number("badgeCanvas", image.canvas_w);
  }
  heap_caps_free(image.pixels);
  m5_pocket_bridge_set_number("badgeTexture", handle);
}

void stopwatch_services_before_frame(void) {
  badge_texture_update();
  const int64_t now_us = esp_timer_get_time();
  m5_pocket_bridge_set_number("uptimeMs", (double)now_us / 1000.0);

  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  m5_pocket_bridge_set_number("year", local.tm_year + 1900);
  m5_pocket_bridge_set_number("month", local.tm_mon + 1);
  m5_pocket_bridge_set_number("day", local.tm_mday);
  m5_pocket_bridge_set_number("weekday", local.tm_wday);
  m5_pocket_bridge_set_number("hour", local.tm_hour);
  m5_pocket_bridge_set_number("minute", local.tm_min);
  m5_pocket_bridge_set_number("second", local.tm_sec);

  const uint32_t keys = stopwatch_services_buttons();
  m5_pocket_bridge_set_bool("keyA", keys & BUTTON_A_BIT);
  m5_pocket_bridge_set_bool("keyB", keys & BUTTON_B_BIT);

  if (now_us - last_battery_us >= 1000000) {
    last_battery_us = now_us;
    m5_stopwatch_battery_t battery;
    if (m5_stopwatch_battery_read(&battery) == ESP_OK) {
      m5_pocket_bridge_set_number("batteryLevel", battery.level);
      m5_pocket_bridge_set_bool("charging", battery.external_power);
    }
  }

  m5_stopwatch_imu_sample_t imu;
  if (m5_stopwatch_imu_read(&imu) == ESP_OK) {
    m5_pocket_bridge_set_number("accelX", imu.accel_x);
    m5_pocket_bridge_set_number("accelY", imu.accel_y);
    m5_pocket_bridge_set_number("accelZ", imu.accel_z);
    m5_pocket_bridge_set_number("gyroX", imu.gyro_x);
    m5_pocket_bridge_set_number("gyroY", imu.gyro_y);
    m5_pocket_bridge_set_number("gyroZ", imu.gyro_z);
  }
}

/* -------------------------------- requests ------------------------------- */

static bool is(const m5_pocket_request *request, const char *name) {
  return request->name_length == strlen(name) &&
         memcmp(request->name, name, request->name_length) == 0;
}

static double arg(const m5_pocket_request *request, size_t index) {
  return index < request->argc ? request->args[index] : 0.0;
}

static void set_rtc(const m5_pocket_request *request) {
  struct tm time = {
      .tm_year = (int)arg(request, 0) - 1900,
      .tm_mon = (int)arg(request, 1) - 1,
      .tm_mday = (int)arg(request, 2),
      .tm_hour = (int)arg(request, 3),
      .tm_min = (int)arg(request, 4),
      .tm_sec = (int)arg(request, 5),
      .tm_isdst = 0,
  };
  const time_t seconds = mktime(&time);
  localtime_r(&seconds, &time); /* fills tm_wday */
  if (m5_stopwatch_rtc_set(&time) != ESP_OK) {
    ESP_LOGW(TAG, "RTC write failed");
  }
  const struct timeval tv = {.tv_sec = seconds, .tv_usec = 0};
  settimeofday(&tv, NULL);
}

void stopwatch_services_request(const m5_pocket_request *request) {
  const bool save = arg(request, 1) != 0.0;
  if (is(request, "tone")) {
    const float midi = (float)arg(request, 0);
    const int frequency = (int)(440.0f * powf(2.0f, (midi - 69.0f) / 12.0f));
    audio_post(0, (float)frequency, (float)arg(request, 1));
  } else if (is(request, "vibrate")) {
    m5_stopwatch_vibrate((uint16_t)arg(request, 0), (uint8_t)arg(request, 1));
  } else if (is(request, "brightness")) {
    settings.brightness = (int)arg(request, 0);
    m5_stopwatch_display_set_brightness((uint8_t)settings.brightness);
    if (save) {
      settings_save_int("bl_lev", settings.brightness);
    }
  } else if (is(request, "volume")) {
    settings.volume = (int)arg(request, 0);
    if (audio_ready) {
      m5_stopwatch_audio_set_volume((uint8_t)settings.volume);
    }
    if (save) {
      settings_save_int("spk_vol", settings.volume);
    }
  } else if (is(request, "button_sfx")) {
    settings.sfx = arg(request, 0) != 0.0;
    if (save) {
      settings_save_flag("btn_sfx", settings.sfx);
    }
  } else if (is(request, "button_vibrate")) {
    settings.vibrate = arg(request, 0) != 0.0;
    if (save) {
      settings_save_flag("btn_vibrate", settings.vibrate);
    }
  } else if (is(request, "wifi")) {
    settings.wifi = arg(request, 0) != 0.0;
    settings_save_flag("wifi_enabled", settings.wifi);
    m5_pocket_bridge_set_text(
        "apMessage",
        settings.wifi ? "Wi-Fi is not available\nin this firmware" : "");
  } else if (is(request, "rtc")) {
    set_rtc(request);
  } else if (is(request, "guide_count")) {
    settings.launch_count++;
    settings_save_int("launch_count", settings.launch_count);
  } else if (is(request, "boot_sfx")) {
    audio_post(1, 0.0f, 0.0f);
  } else if (is(request, "mic")) {
    mic_on = arg(request, 0) != 0.0;
    if (mic_on && mic_task_handle) {
      xTaskNotifyGive(mic_task_handle);
    }
  } else if (is(request, "alarm_clear")) {
    memset(&alarms_pending, 0, sizeof(alarms_pending));
  } else if (is(request, "alarm")) {
    if (alarms_pending.count < MAX_ALARMS) {
      uint8_t *e = alarms_pending.entries[alarms_pending.count++];
      e[0] = (uint8_t)arg(request, 0);
      e[1] = (uint8_t)arg(request, 1);
      e[2] = arg(request, 2) != 0.0;
    }
  } else if (is(request, "alarm_save")) {
    alarms_save();
  } else if (is(request, "badge_edit")) {
    stopwatch_badge_edit(arg(request, 0) != 0.0);
  } else if (is(request, "badge_slot")) {
    stopwatch_badge_select((int)arg(request, 0));
  } else if (is(request, "alarm_ring")) {
    /* The app drives the alarm beeps itself. */
  } else {
    ESP_LOGW(TAG, "unknown request %.*s", (int)request->name_length,
             request->name);
  }
}
