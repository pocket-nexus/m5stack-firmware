/*
 * SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * M5Stack StopWatch firmware. The BSP powers the board and returns the
 * CO5300 panel and CST820 touch handles; m5_pocket_app runs the
 * stopwatch-factory application on them (JS mode by default, AOT with
 * CONFIG_M5_POCKET_APP_MODE_AOT) and services.c connects the board to the
 * application's device bridge.
 */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "m5_pocket/app.h"
#include "m5_stopwatch/bsp.h"
#include "nvs_flash.h"
#include "services.h"

static const char *TAG = "stopwatch";

/* 32 full-width RGB565 rows per strip keep one QSPI transfer under the
 * ESP32-S3 SPI DMA limit of 32 KiB. */
#define STRIP_ROWS 32

static m5_stopwatch_handles_t board;

static uint32_t buttons(void *context) {
  (void)context;
  return stopwatch_services_buttons();
}

static bool touch(void *context, float *x, float *y) {
  (void)context;
  if (!board.touch || esp_lcd_touch_read_data(board.touch) != ESP_OK) {
    return false;
  }
  esp_lcd_touch_point_data_t point;
  uint8_t count = 0;
  if (esp_lcd_touch_get_data(board.touch, &point, &count, 1) != ESP_OK ||
      count == 0) {
    return false;
  }
  *x = point.x;
  *y = point.y;
  return true;
}

static void before_frame(void *context) {
  (void)context;
  stopwatch_services_before_frame();
}

static void on_request(void *context, const m5_pocket_request *request) {
  (void)context;
  stopwatch_services_request(request);
}

static void init_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

void app_main(void) {
  init_nvs();
  const m5_stopwatch_config_t board_config = M5_STOPWATCH_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(m5_stopwatch_init(&board_config, &board));
  ESP_ERROR_CHECK(stopwatch_services_start());

  m5_pocket_app_config config;
  m5_pocket_app_config_defaults(&config);
  config.display.kind = M5_POCKET_DISPLAY_ESP_LCD;
  config.display.lcd = (pocket_display_esp_lcd_config){
      .panel = board.lcd,
      .io = board.lcd_io,
      .width = M5_STOPWATCH_LCD_H_RES,
      .height = M5_STOPWATCH_LCD_V_RES,
      .strip_rows = STRIP_ROWS,
      .buffer_count = 2,
      .frame_buffer_budget = 2 * M5_STOPWATCH_LCD_H_RES * STRIP_ROWS * 2,
      .byte_swap = true,
  };
  config.input.buttons = buttons;
  config.input.touch = touch;
  config.before_frame = before_frame;
  config.on_request = on_request;
  config.heap_limit = 4 * 1024 * 1024;
  config.js_heap_limit = 6 * 1024 * 1024;
  config.js_stack_limit = 48 * 1024;
  config.js_prefer_psram = true;
  /* Internal-RAM stack: the request hook writes NVS (flash). */
  config.task_stack_bytes = 64 * 1024;
  config.task_core = 1;

  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
  ESP_LOGI(TAG, "running, free internal=%u psram=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
