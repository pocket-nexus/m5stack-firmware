// SPDX-License-Identifier: MIT
//
// StickS3 firmware: brings the board up through m5_bsp_sticks3 and runs the
// sticks3-demos application through m5_pocket_app. Every frame the device
// bridge carries the clock, keys (with interrupt timestamps), IMU, microphone
// level, battery and power-key clicks to apps/sticks3-demos/device.ts; the
// application's requests (tones, brightness, volume, microphone, best
// scores) come back to on_request.

#include "m5_pocket/app.h"
#include "m5_sticks3/board.h"
#include "m5_sticks3/display.h"
#include "m5_sticks3/imu.h"
#include "m5_sticks3/mic.h"
#include "m5_sticks3/pm1.h"
#include "m5_sticks3/power.h"
#include "m5_sticks3/speaker.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "sticks3";

// Two DMA strips of 40 rows: 135 * 40 * 2 bytes each.
#define STRIP_ROWS 40
#define STRIP_BUFFERS 2
#define STRIP_BYTES (M5_STICKS3_LCD_WIDTH * STRIP_ROWS * 2)

// Until the application's first requests arrive.
#define BOOT_BRIGHTNESS 200
#define BOOT_VOLUME 110

// The PMIC is read twice a second for the battery and ten times a second for
// the power key; the IMU and the microphone level every frame.
#define POWER_POLL_US (500 * 1000)
#define POWER_KEY_POLL_US (100 * 1000)

// Best scores: slots of device.ts BEST_*, stored as one blob.
#define BEST_SLOTS 7
#define BEST_NAMESPACE "demos"
#define BEST_KEY "best"

typedef struct {
  uint32_t slot;
  int32_t value;
} best_write_t;

static int64_t s_presented_us;
static int64_t s_power_polled_us = -POWER_POLL_US;
static int64_t s_key_polled_us = -POWER_KEY_POLL_US;
static bool s_imu_ok;
static int32_t s_best[BEST_SLOTS];
static QueueHandle_t s_store_queue;

static double ms(int64_t us) { return (double)us / 1000.0; }

static void publish_best(void) {
  float values[BEST_SLOTS];
  for (int i = 0; i < BEST_SLOTS; ++i) {
    values[i] = (float)s_best[i];
  }
  m5_pocket_bridge_set_array("best", values, BEST_SLOTS);
}

static void publish(void *context) {
  (void)context;
  const int64_t now = esp_timer_get_time();
  m5_pocket_bridge_set_number("uptimeMs", ms(now));
  m5_pocket_bridge_set_number("presentedMs", ms(s_presented_us));

  m5_pocket_bridge_set_bool("keyFront",
                            m5_sticks3_key_pressed(M5_STICKS3_KEY_1));
  m5_pocket_bridge_set_bool("keySide",
                            m5_sticks3_key_pressed(M5_STICKS3_KEY_2));
  m5_pocket_bridge_set_number(
      "frontPressMs", ms(m5_sticks3_key_press_time_us(M5_STICKS3_KEY_1)));
  m5_pocket_bridge_set_number(
      "sidePressMs", ms(m5_sticks3_key_press_time_us(M5_STICKS3_KEY_2)));

  if (now - s_power_polled_us >= POWER_POLL_US) {
    s_power_polled_us = now;
    m5_pocket_bridge_set_number("batteryLevel", m5_sticks3_battery_level());
    m5_pocket_bridge_set_bool("charging", m5_sticks3_is_charging());
  }
  if (now - s_key_polled_us >= POWER_KEY_POLL_US) {
    s_key_polled_us = now;
    // A double click is the PMIC's power-off gesture; only clicks reach the
    // application.
    if (m5_sticks3_pm1_take_key_event() == M5_STICKS3_PM1_KEY_CLICK) {
      m5_pocket_bridge_post_event("powerKey", 1);
    }
  }

  if (s_imu_ok) {
    m5_sticks3_imu_sample_t imu;
    if (m5_sticks3_imu_read(&imu) == ESP_OK) {
      m5_pocket_bridge_set_number("accelX", imu.accel_x);
      m5_pocket_bridge_set_number("accelY", imu.accel_y);
      m5_pocket_bridge_set_number("accelZ", imu.accel_z);
      m5_pocket_bridge_set_number("gyroX", imu.gyro_x);
      m5_pocket_bridge_set_number("gyroY", imu.gyro_y);
      m5_pocket_bridge_set_number("gyroZ", imu.gyro_z);
    }
  }

  m5_sticks3_mic_level_t level;
  if (m5_sticks3_mic_take_level(&level) == ESP_OK) {
    m5_pocket_bridge_set_number("micDb", level.rms_dbfs);
    m5_pocket_bridge_set_number("micPeakDb", level.peak_dbfs);
  }
}

static void after_frame(void *context, int32_t status) {
  (void)context;
  (void)status;
  // The tick that rendered this frame has handed it to the panel.
  s_presented_us = esp_timer_get_time();
}

static bool request_is(const m5_pocket_request *request, const char *name) {
  return request->name_length == strlen(name) &&
         memcmp(request->name, name, request->name_length) == 0;
}

static uint8_t clamp_byte(double value) {
  return value <= 0 ? 0 : value >= 255 ? 255 : (uint8_t)value;
}

static void set_mic(bool on) {
  if (on) {
    const esp_err_t err = m5_sticks3_mic_start();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "microphone: %s", esp_err_to_name(err));
    }
  } else {
    m5_sticks3_mic_stop();
  }
  m5_pocket_bridge_set_bool("micOn", m5_sticks3_mic_running());
}

static void on_request(void *context, const m5_pocket_request *request) {
  (void)context;
  if (request_is(request, "tone") && request->argc >= 2) {
    const uint32_t hz = (uint32_t)request->args[0];
    const uint32_t duration = (uint32_t)request->args[1];
    if (request->argc >= 3) {
      // Start time on the uptimeMs clock (esp_timer).
      m5_sticks3_speaker_tone_at(hz, duration,
                                 (int64_t)(request->args[2] * 1000.0));
    } else {
      m5_sticks3_speaker_tone(hz, duration);
    }
  } else if (request_is(request, "brightness") && request->argc >= 1) {
    m5_sticks3_backlight_set(clamp_byte(request->args[0]));
  } else if (request_is(request, "volume") && request->argc >= 1) {
    m5_sticks3_speaker_set_volume(clamp_byte(request->args[0]));
  } else if (request_is(request, "mic") && request->argc >= 1) {
    set_mic(request->args[0] != 0);
  } else if (request_is(request, "best") && request->argc >= 2 &&
             request->args[0] >= 0 && request->args[0] < BEST_SLOTS) {
    // The app task's stack is in PSRAM: the store task writes the flash.
    const best_write_t write = {(uint32_t)request->args[0],
                                (int32_t)request->args[1]};
    s_best[write.slot] = write.value;
    if (!s_store_queue || xQueueSend(s_store_queue, &write, 0) != pdTRUE) {
      ESP_LOGW(TAG, "best score not stored");
    }
  } else {
    ESP_LOGW(TAG, "unknown request %.*s", (int)request->text_length,
             request->text);
  }
}

static esp_err_t nvs_start(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase");
    err = nvs_flash_init();
  }
  return err;
}

static void load_best(void) {
  nvs_handle_t handle;
  if (nvs_open(BEST_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
    return; // nothing stored yet
  }
  int32_t stored[BEST_SLOTS] = {0};
  size_t size = sizeof(stored);
  const esp_err_t err = nvs_get_blob(handle, BEST_KEY, stored, &size);
  nvs_close(handle);
  if (err == ESP_OK) {
    // A shorter blob from an older slot table leaves the new slots at 0.
    memcpy(s_best, stored, size < sizeof(s_best) ? size : sizeof(s_best));
  }
}

static void store_task(void *arg) {
  (void)arg;
  best_write_t write;
  int32_t values[BEST_SLOTS] = {0};
  memcpy(values, s_best, sizeof(values));
  for (;;) {
    if (xQueueReceive(s_store_queue, &write, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    values[write.slot] = write.value;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(BEST_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
      err = nvs_set_blob(handle, BEST_KEY, values, sizeof(values));
      if (err == ESP_OK) {
        err = nvs_commit(handle);
      }
      nvs_close(handle);
    }
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "store best %" PRIu32 ": %s", write.slot,
               esp_err_to_name(err));
    }
  }
}

void app_main(void) {
  ESP_ERROR_CHECK(m5_sticks3_power_init());
  ESP_ERROR_CHECK(m5_sticks3_keys_init());
  ESP_ERROR_CHECK(m5_sticks3_backlight_init());

  m5_sticks3_display_t lcd;
  ESP_ERROR_CHECK(m5_sticks3_display_init(STRIP_BYTES, &lcd));
  ESP_ERROR_CHECK(m5_sticks3_display_clear(&lcd, 0x0000));
  ESP_ERROR_CHECK(m5_sticks3_backlight_set(BOOT_BRIGHTNESS));

  if (m5_sticks3_speaker_init() == ESP_OK) {
    m5_sticks3_speaker_set_volume(BOOT_VOLUME);
  } else {
    ESP_LOGW(TAG, "speaker and microphone unavailable");
  }
  s_imu_ok = m5_sticks3_imu_init() == ESP_OK;
  if (!s_imu_ok) {
    ESP_LOGW(TAG, "IMU unavailable");
  }

  if (nvs_start() == ESP_OK) {
    load_best();
    s_store_queue = xQueueCreate(8, sizeof(best_write_t));
    if (!s_store_queue || xTaskCreate(store_task, "sticks3_store", 3072, NULL,
                                      3, NULL) != pdPASS) {
      ESP_LOGE(TAG, "store task");
    }
  } else {
    ESP_LOGW(TAG, "NVS unavailable: best scores are not kept");
  }

  // Values before the first frame.
  m5_pocket_bridge_set_bool("bridged", true);
  m5_pocket_bridge_set_bool("imuOk", s_imu_ok);
  m5_pocket_bridge_set_bool("micOn", false);
  m5_pocket_bridge_set_number("seed", (double)esp_random());
  publish_best();
  s_presented_us = esp_timer_get_time();
  publish(NULL);

  m5_pocket_app_config config;
  m5_pocket_app_config_defaults(&config);
  config.display.kind = M5_POCKET_DISPLAY_ESP_LCD;
  config.display.lcd = (pocket_display_esp_lcd_config){
      .panel = lcd.panel,
      .io = lcd.io,
      .width = lcd.width,
      .height = lcd.height,
      .strip_rows = STRIP_ROWS,
      .buffer_count = STRIP_BUFFERS,
      .frame_buffer_budget = STRIP_BYTES * STRIP_BUFFERS,
      .byte_swap = true, // the panel takes big-endian RGB565
  };
  // The keys reach the application as bridge values (keyFront/keySide and
  // their interrupt timestamps); no button callback is registered.
  config.heap_limit = 2 * 1024 * 1024;
  config.js_heap_limit = 4 * 1024 * 1024;
  config.js_stack_limit = 96 * 1024;
  config.js_prefer_psram = true;
  config.task_stack_bytes = 128 * 1024;
  config.task_stack_caps = MALLOC_CAP_SPIRAM;
  config.before_frame = publish;
  config.on_request = on_request;
  config.after_frame = after_frame;

  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
  ESP_LOGI(TAG, "sticks3-demos running, free internal %u, PSRAM %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
