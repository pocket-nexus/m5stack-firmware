/*
 * SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * M5Stack StopWatch firmware, JS mode: the stopwatch-factory bundle runs in
 * the QuickJS guest. The BSP powers the board and returns the panel and touch
 * handles; this file wraps them in the PocketJS display and input drivers and
 * starts the runtime on the runner task.
 */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "m5_stopwatch/bsp.h"
#include "nvs_flash.h"
#include "pocket/display_esp_lcd.h"
#include "pocket/guest_idf.h"
#include "pocket/input_gpio.h"
#include "pocket/input_touch.h"
#include "pocket/package_source.h"
#include "pocket/runner.h"
#include "pocket_profile.h"
#include "pocketjs_bundle_stopwatch.h"
#include <string.h>

static const char *TAG = "stopwatch";

/* Front keys as PocketJS button bits: KEYA (yellow) and KEYB (blue). The
 * trigger bits keep the keys out of directional focus navigation. */
#define STOPWATCH_BUTTON_A 0x0100u /* BTN.LTRIGGER */
#define STOPWATCH_BUTTON_B 0x0200u /* BTN.RTRIGGER */

/* 32 full-width RGB565 rows per strip keep one QSPI transfer under the
 * ESP32-S3 SPI DMA limit of 32 KiB. */
#define STOPWATCH_STRIP_ROWS 32
#define STOPWATCH_STRIP_BUFFERS 2

static void interrupt_guest(void *context) {
  pocket_guest_idf_interrupt((pocket_guest_idf *)context);
}

static void after_tick(int32_t status, void *context) {
  if (status != POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "tick status=%d: %s", (int)status,
             pocket_guest_idf_error((pocket_guest_idf *)context));
  }
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
  m5_stopwatch_handles_t board;
  ESP_ERROR_CHECK(m5_stopwatch_init(&board_config, &board));

  const pocketjs_bundle_host_contract_t *host =
      &pocketjs_bundle_stopwatch_contract;
  const size_t strip_bytes =
      (size_t)host->physical_width * STOPWATCH_STRIP_ROWS * 2;
  const pocket_display_esp_lcd_config display_config = {
      .struct_size = sizeof(display_config),
      .panel = board.lcd,
      .io = board.lcd_io,
      .width = host->physical_width,
      .height = host->physical_height,
      .strip_rows = STOPWATCH_STRIP_ROWS,
      .buffer_count = STOPWATCH_STRIP_BUFFERS,
      .frame_buffer_budget = strip_bytes * STOPWATCH_STRIP_BUFFERS,
      .byte_swap = true,
  };
  pocket_display_esp_lcd *display =
      pocket_display_esp_lcd_create(&display_config);
  if (!display) {
    ESP_LOGE(TAG, "display driver creation failed");
    abort();
  }

  pocket_runtime_config runtime_config = {
      .struct_size = sizeof(runtime_config),
      .host_abi = pocket_profile.host_abi,
      .tick_hz = pocket_profile.tick_hz,
      .heap_limit = 4 * 1024 * 1024,
      .frame_buffer_budget = pocket_display_esp_lcd_buffer_bytes(display),
      .profile = pocket_profile.facts,
  };
  memcpy(runtime_config.profile_hash, pocket_profile.profile_hash, 32);
  pocket_runtime *runtime = pocket_runtime_create(&runtime_config);
  if (!runtime) {
    ESP_LOGE(TAG, "runtime creation failed");
    abort();
  }

  const pocket_guest_idf_config guest_config = {
      .heap_limit = 6 * 1024 * 1024,
      .stack_limit = 256 * 1024,
      .prefer_psram = true,
  };
  pocket_guest_idf *guest = pocket_guest_idf_create(&guest_config);
  if (!guest) {
    ESP_LOGE(TAG, "guest creation failed");
    abort();
  }
  ESP_ERROR_CHECK(pocket_runtime_set_guest(runtime, pocket_guest_idf_vtable(),
                                           guest) == POCKET_STATUS_OK
                      ? ESP_OK
                      : ESP_FAIL);

  static pocket_memory_package package;
  package.data = pocketjs_bundle_stopwatch.data;
  package.size = pocketjs_bundle_stopwatch.size;
  ESP_ERROR_CHECK(
      pocket_runtime_set_package_source(runtime, pocket_memory_package_vtable(),
                                        &package) == POCKET_STATUS_OK
          ? ESP_OK
          : ESP_FAIL);

  if (board.touch) {
    const pocket_input_touch_config touch_config = {
        .struct_size = sizeof(touch_config),
        .touch = board.touch,
        .surface = 0,
        .scale_x = (float)host->logical_width / M5_STOPWATCH_LCD_H_RES,
        .scale_y = (float)host->logical_height / M5_STOPWATCH_LCD_V_RES,
    };
    pocket_input_touch *touch = pocket_input_touch_create(&touch_config);
    if (!touch ||
        pocket_runtime_add_input_source(runtime, pocket_input_touch_vtable(),
                                        touch) != POCKET_STATUS_OK) {
      ESP_LOGE(TAG, "touch input registration failed");
      abort();
    }
  } else {
    ESP_LOGW(TAG, "touch controller absent");
  }

  static const pocket_gpio_button keys[] = {
      {.pin = M5_STOPWATCH_PIN_BUTTON_A,
       .buttons = STOPWATCH_BUTTON_A,
       .active_low = true},
      {.pin = M5_STOPWATCH_PIN_BUTTON_B,
       .buttons = STOPWATCH_BUTTON_B,
       .active_low = true},
  };
  const pocket_input_gpio_config keys_config = {
      .struct_size = sizeof(keys_config),
      .gpio = keys,
      .gpio_count = sizeof(keys) / sizeof(keys[0]),
      .debounce_us = 10000,
  };
  pocket_input_gpio *buttons = pocket_input_gpio_create(&keys_config);
  if (!buttons ||
      pocket_runtime_add_input_source(runtime, pocket_input_gpio_vtable(),
                                      buttons) != POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "button input registration failed");
    abort();
  }

  if (pocket_runtime_add_display(runtime, pocket_display_esp_lcd_vtable(),
                                 display, POCKET_RENDERER_SOFTWARE) !=
      POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "display registration failed");
    abort();
  }

  const int32_t boot = pocket_runtime_boot(runtime);
  if (boot != POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "boot status=%d: %s", (int)boot,
             pocket_guest_idf_error(guest));
    abort();
  }

  pocket_runner_config_t runner_config;
  pocket_runner_config_defaults(&runner_config);
  runner_config.task_name = "pocket";
  runner_config.task_stack_bytes = 64 * 1024;
  runner_config.task_stack_caps = MALLOC_CAP_SPIRAM;
  runner_config.task_core = 1;
  runner_config.interrupt = interrupt_guest;
  runner_config.after_tick = after_tick;
  runner_config.user_data = guest;
  pocket_runner_t *runner = NULL;
  ESP_ERROR_CHECK(pocket_runner_start(runtime, &runner_config, &runner));
  ESP_LOGI(TAG, "running %ux%u, free internal=%u psram=%u",
           (unsigned)host->logical_width, (unsigned)host->logical_height,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
