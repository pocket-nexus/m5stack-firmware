// SPDX-License-Identifier: MIT
//
// StickS3 firmware, JS mode: the sticks3-factory bundle runs in the QuickJS
// guest. The board comes up through m5_bsp_sticks3; the panel is wrapped in
// pocket_display_esp_lcd and the two front/side keys in pocket_input_gpio.

#include "m5_sticks3/board.h"
#include "m5_sticks3/display.h"
#include "m5_sticks3/power.h"
#include "m5_sticks3/speaker.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pocket/display_esp_lcd.h"
#include "pocket/guest_idf.h"
#include "pocket/input_gpio.h"
#include "pocket/package_source.h"
#include "pocket/runner.h"
#include "pocket_profile.h"
#include "pocketjs_bundle_sticks3_factory.h"
#include <string.h>

static const char *TAG = "sticks3";

// Two DMA strips of 40 rows: 135 * 40 * 2 bytes each.
#define STRIP_ROWS 40
#define STRIP_BUFFERS 2
#define STRIP_BYTES (M5_STICKS3_LCD_WIDTH * STRIP_ROWS * 2)

// UIFlow2 startup brightness: 90 % of 255.
#define BOOT_BRIGHTNESS 230

#define GUEST_HEAP_BYTES (4 * 1024 * 1024)
#define GUEST_STACK_BYTES (96 * 1024)
#define RUNNER_STACK_BYTES (128 * 1024)

static void interrupt_guest(void *guest) { pocket_guest_idf_interrupt(guest); }

static void halt(const char *what, int32_t status) {
  ESP_LOGE(TAG, "%s failed: %d", what, (int)status);
  for (;;) {
    vTaskDelay(portMAX_DELAY);
  }
}

static void check(const char *what, int32_t status) {
  if (status != POCKET_STATUS_OK) {
    halt(what, status);
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
    m5_sticks3_speaker_set_volume(100);
  } else {
    ESP_LOGW(TAG, "speaker unavailable");
  }
  ESP_LOGI(TAG, "battery %d %% (%u mV)%s", m5_sticks3_battery_level(),
           (unsigned)m5_sticks3_battery_mv(),
           m5_sticks3_is_charging() ? ", charging" : "");

  const pocket_display_esp_lcd_config display_config = {
      .struct_size = sizeof(display_config),
      .panel = lcd.panel,
      .io = lcd.io,
      .width = lcd.width,
      .height = lcd.height,
      .strip_rows = STRIP_ROWS,
      .buffer_count = STRIP_BUFFERS,
      .frame_buffer_budget = STRIP_BYTES * STRIP_BUFFERS,
      // The panel takes big-endian RGB565.
      .byte_swap = true,
  };
  pocket_display_esp_lcd *display =
      pocket_display_esp_lcd_create(&display_config);
  if (!display) {
    halt("display", POCKET_STATUS_FAILED);
  }

  // KEY1 (front) is the UIFlow BtnA, KEY2 (side) BtnB.
  const pocket_gpio_button keys[] = {
      {M5_STICKS3_KEY1, POCKET_BUTTON_CIRCLE, true},
      {M5_STICKS3_KEY2, POCKET_BUTTON_DOWN, true},
  };
  const pocket_input_gpio_config input_config = {
      .struct_size = sizeof(input_config),
      .gpio = keys,
      .gpio_count = sizeof(keys) / sizeof(keys[0]),
      .debounce_us = 10000, // M5Unified Button_Class debounce
  };
  pocket_input_gpio *input = pocket_input_gpio_create(&input_config);
  if (!input) {
    halt("input", POCKET_STATUS_FAILED);
  }

  pocket_runtime_config config = {
      .struct_size = sizeof(config),
      .host_abi = pocket_profile.host_abi,
      .tick_hz = pocket_profile.tick_hz,
      .heap_limit = GUEST_HEAP_BYTES,
      .frame_buffer_budget = pocket_display_esp_lcd_buffer_bytes(display),
      .profile = pocket_profile.facts,
  };
  memcpy(config.profile_hash, pocket_profile.profile_hash,
         sizeof(config.profile_hash));
  pocket_runtime *runtime = pocket_runtime_create(&config);
  if (!runtime) {
    halt("runtime", POCKET_STATUS_FAILED);
  }

  const pocket_guest_idf_config guest_config = {
      .heap_limit = GUEST_HEAP_BYTES,
      .stack_limit = GUEST_STACK_BYTES,
      .prefer_psram = true,
  };
  pocket_guest_idf *guest = pocket_guest_idf_create(&guest_config);
  if (!guest) {
    halt("guest", POCKET_STATUS_FAILED);
  }
  check("set guest",
        pocket_runtime_set_guest(runtime, pocket_guest_idf_vtable(), guest));
  static pocket_memory_package package;
  package.data = pocketjs_bundle_sticks3_factory.data;
  package.size = pocketjs_bundle_sticks3_factory.size;
  check("package", pocket_runtime_set_package_source(
                       runtime, pocket_memory_package_vtable(), &package));
  check("input", pocket_runtime_add_input_source(
                     runtime, pocket_input_gpio_vtable(), input));
  check("display",
        pocket_runtime_add_display(runtime, pocket_display_esp_lcd_vtable(),
                                   display, POCKET_RENDERER_SOFTWARE));

  const int32_t boot = pocket_runtime_boot(runtime);
  if (boot != POCKET_STATUS_OK) {
    ESP_LOGE(TAG, "boot: %s", pocket_guest_idf_error(guest));
    halt("boot", boot);
  }

  pocket_runner_config_t runner_config;
  pocket_runner_config_defaults(&runner_config);
  runner_config.task_name = "sticks3_ui";
  runner_config.task_stack_bytes = RUNNER_STACK_BYTES;
  runner_config.task_stack_caps = MALLOC_CAP_SPIRAM;
  runner_config.interrupt = interrupt_guest;
  runner_config.user_data = guest;
  pocket_runner_t *runner = NULL;
  ESP_ERROR_CHECK(pocket_runner_start(runtime, &runner_config, &runner));
  ESP_LOGI(TAG, "running sticks3-factory, %u bytes free internal, %u PSRAM",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
