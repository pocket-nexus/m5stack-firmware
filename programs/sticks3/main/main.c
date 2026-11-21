// SPDX-License-Identifier: MIT
//
// StickS3 firmware: brings the board up through m5_bsp_sticks3 and runs the
// sticks3-factory application through m5_pocket_app. The device bridge
// carries battery, charge and Wi-Fi state to apps/sticks3-factory/device.ts
// and its requests (tone, volume, brightness, setup access point) back.

#include "m5_pocket/app.h"
#include "m5_sticks3/board.h"
#include "m5_sticks3/display.h"
#include "m5_sticks3/power.h"
#include "m5_sticks3/speaker.h"
#include "m5_sticks3_net.h"

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pocket_spec.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "sticks3";

// Two DMA strips of 40 rows: 135 * 40 * 2 bytes each.
#define STRIP_ROWS 40
#define STRIP_BUFFERS 2
#define STRIP_BYTES (M5_STICKS3_LCD_WIDTH * STRIP_ROWS * 2)

// UIFlow2 startup: brightness 90 % of 255, speaker volume 100.
#define BOOT_BRIGHTNESS 230
#define BOOT_VOLUME 100

// The PMIC is read twice a second (LauncherApp.on_run polls every 500 ms).
#define POWER_POLL_US (500 * 1000)

static QueueHandle_t s_setup_queue;
static int64_t s_power_polled_us = -POWER_POLL_US;

static uint32_t read_keys(void *context) {
  (void)context;
  uint32_t buttons = 0;
  if (m5_sticks3_key_pressed(M5_STICKS3_KEY_1)) {
    buttons |= POCKET_BTN_CIRCLE; // front key, UIFlow BtnA
  }
  if (m5_sticks3_key_pressed(M5_STICKS3_KEY_2)) {
    buttons |= POCKET_BTN_DOWN; // side key, UIFlow BtnB
  }
  return buttons;
}

static void publish(void *context) {
  (void)context;
  const int64_t now = esp_timer_get_time();
  if (now - s_power_polled_us >= POWER_POLL_US) {
    s_power_polled_us = now;
    m5_pocket_bridge_set_number("batteryLevel", m5_sticks3_battery_level());
    m5_pocket_bridge_set_bool("charging", m5_sticks3_is_charging());
  }
  m5_sticks3_net_state_t net;
  m5_sticks3_net_get_state(&net);
  m5_pocket_bridge_set_bool("wifiConfigured", net.configured);
  m5_pocket_bridge_set_number("wifiStatus", net.status);
  m5_pocket_bridge_set_number("apStations", net.ap_stations);
  m5_pocket_bridge_set_number("portalClients", net.portal_clients);
  m5_pocket_bridge_set_text("apSsid", net.ap_ssid);
}

static bool request_is(const m5_pocket_request *request, const char *name) {
  return request->name_length == strlen(name) &&
         memcmp(request->name, name, request->name_length) == 0;
}

static uint8_t clamp_byte(double value) {
  return value <= 0 ? 0 : value >= 255 ? 255 : (uint8_t)value;
}

static void on_request(void *context, const m5_pocket_request *request) {
  (void)context;
  if (request_is(request, "tone") && request->argc >= 2) {
    m5_sticks3_speaker_tone((uint32_t)request->args[0],
                            (uint32_t)request->args[1]);
  } else if (request_is(request, "volume") && request->argc >= 1) {
    m5_sticks3_speaker_set_volume(clamp_byte(request->args[0]));
  } else if (request_is(request, "brightness") && request->argc >= 1) {
    m5_sticks3_backlight_set(clamp_byte(request->args[0]));
  } else if (request_is(request, "setup") && request->argc >= 1) {
    // Starting or stopping the access point and HTTP server blocks for tens
    // of milliseconds; the setup task does it off the app task.
    const bool active = request->args[0] != 0;
    xQueueSend(s_setup_queue, &active, 0);
  } else {
    ESP_LOGW(TAG, "unknown request %.*s", (int)request->text_length,
             request->text);
  }
}

static void setup_task(void *arg) {
  (void)arg;
  bool active;
  for (;;) {
    if (xQueueReceive(s_setup_queue, &active, portMAX_DELAY) == pdTRUE) {
      const esp_err_t err =
          active ? m5_sticks3_net_setup_start() : m5_sticks3_net_setup_stop();
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "setup %s: %s", active ? "start" : "stop",
                 esp_err_to_name(err));
      }
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
    ESP_LOGW(TAG, "speaker unavailable");
  }

  // UIFlow Startup: connect to the saved network before the menu starts.
  if (m5_sticks3_net_init() != ESP_OK) {
    ESP_LOGW(TAG, "Wi-Fi unavailable");
  }
  s_setup_queue = xQueueCreate(4, sizeof(bool));
  if (!s_setup_queue ||
      xTaskCreate(setup_task, "sticks3_setup", 4096, NULL, 4, NULL) != pdPASS) {
    ESP_LOGE(TAG, "setup task");
  }

  // Values that do not change while the firmware runs.
  char version[40];
  snprintf(version, sizeof(version), "v%s", esp_app_get_description()->version);
  m5_pocket_bridge_set_text("firmwareVersion", version);
  m5_pocket_bridge_set_number("cloudStatus", -1); // no UIFlow cloud client
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
  config.input.buttons = read_keys;
  config.heap_limit = 2 * 1024 * 1024;
  config.js_heap_limit = 4 * 1024 * 1024;
  config.js_stack_limit = 96 * 1024;
  config.js_prefer_psram = true;
  config.task_stack_bytes = 128 * 1024;
  config.task_stack_caps = MALLOC_CAP_SPIRAM;
  config.before_frame = publish;
  config.on_request = on_request;

  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
  ESP_LOGI(TAG, "sticks3-factory running, free internal %u, PSRAM %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
