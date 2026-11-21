// SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
// SPDX-License-Identifier: MIT
//
// M5Stack AtomS3 program: the GC9107 panel, the button and the factory
// functions' hardware, running apps/atoms3-factory through m5_pocket_app.
// The ESP32-S3FN8 has no PSRAM: every buffer below is internal RAM, and the
// heap is logged at each stage (programs/atoms3/README.md, memory budget).
#include "atoms3_device.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "m5_atoms3/board.h"
#include "m5_atoms3/display.h"
#include "m5_pocket/app.h"

static const char *TAG = "atoms3";

// Two 16-row RGB565 strips: 2 x 128 x 16 x 2 = 8 KiB of DMA memory.
#define STRIP_ROWS 16
#define STRIP_COUNT 2
// Runtime heap limit: UI tree, layout, DrawList and decoded textures (one
// 128x128 RGB565 artwork is 32 KiB).
#define RUNTIME_HEAP_LIMIT (160 * 1024)
// JS mode only: QuickJS heap and stack limits.
#define JS_HEAP_LIMIT (256 * 1024)
#define JS_STACK_LIMIT (16 * 1024)

static void log_heap(const char *stage) {
  ESP_LOGI(TAG, "heap %-8s free %6u largest %6u minimum %6u", stage,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}

static void after_frame(void *context, int32_t status) {
  static uint32_t frames;
  (void)context;
  if (status != POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "frame status %d", (int)status);
  }
  if (++frames % 300 == 0) {
    log_heap("running");
  }
}

void app_main(void) {
  static atoms3_device_t device;
  static m5_atoms3_display_t display;
  log_heap("boot");

  ESP_ERROR_CHECK(m5_atoms3_display_init(
      &(m5_atoms3_display_config_t){.max_transfer_bytes =
                                        M5_ATOMS3_LCD_WIDTH * STRIP_ROWS * 2},
      &display));
  ESP_ERROR_CHECK(m5_atoms3_backlight_init(127));
  ESP_ERROR_CHECK(atoms3_device_init(&device));
  log_heap("board");

  m5_pocket_app_config config;
  m5_pocket_app_config_defaults(&config);
  config.display.kind = M5_POCKET_DISPLAY_ESP_LCD;
  config.display.lcd = (pocket_display_esp_lcd_config){
      .panel = display.panel,
      .io = display.io,
      .width = M5_ATOMS3_LCD_WIDTH,
      .height = M5_ATOMS3_LCD_HEIGHT,
      .strip_rows = STRIP_ROWS,
      .buffer_count = STRIP_COUNT,
      .frame_buffer_budget = M5_ATOMS3_LCD_WIDTH * STRIP_ROWS * 2 * STRIP_COUNT,
      .byte_swap = true,
  };
  config.input.context = &device;
  config.input.buttons = atoms3_device_buttons;
  config.heap_limit = RUNTIME_HEAP_LIMIT;
  config.js_heap_limit = JS_HEAP_LIMIT;
  config.js_stack_limit = JS_STACK_LIMIT;
  config.js_prefer_psram = false;
  config.before_frame = atoms3_device_before_frame;
  config.on_request = atoms3_device_request;
  config.after_frame = after_frame;
  config.hook_context = &device;

  m5_pocket_app *app = NULL;
  esp_err_t err = m5_pocket_app_start(&config, &app);
  log_heap(err == ESP_OK ? "started" : "failed");
  ESP_ERROR_CHECK(err);
}
