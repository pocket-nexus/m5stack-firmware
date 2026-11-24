/*
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_pocket/app.h"
#include "m5_stopwatch/bsp.h"
#include "nvs_flash.h"
#include "services.h"

static const char *TAG = "stopwatch";

/* 32 full-width RGB565 rows per transfer keep one QSPI transfer under the
 * ESP32-S3 SPI DMA limit of 32 KiB. */
#define TRANSFER_ROWS 32

static m5_stopwatch_handles_t board;

/* The panel's TE edge as the display driver's refresh report. */
static bool wait_refresh(void *context, uint32_t timeout_ms,
                         pocket_display_esp_lcd_refresh *out) {
  (void)context;
  return m5_stopwatch_display_wait_refresh(timeout_ms, &out->start_us,
                                           &out->period_us);
}

static uint32_t buttons(void *context) {
  (void)context;
  return stopwatch_services_buttons();
}

/* One read of the CST820: an I2C transaction at 100 kHz, about 1 ms. */
static bool touch_read(float *x, float *y) {
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

/* The contact is read on a task of core 0, a sample per frame at 60 Hz, so
 * that the app task's input poll does not wait for the I2C transaction. The
 * poll takes the latest sample. */
#define TOUCH_PERIOD_MS 16

static portMUX_TYPE touch_lock = portMUX_INITIALIZER_UNLOCKED;
static struct {
  bool down;
  float x, y;
} touch_latest;

static void touch_task(void *arg) {
  (void)arg;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    float x = 0, y = 0;
    const bool down = touch_read(&x, &y);
    portENTER_CRITICAL(&touch_lock);
    touch_latest.down = down;
    touch_latest.x = x;
    touch_latest.y = y;
    portEXIT_CRITICAL(&touch_lock);
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(TOUCH_PERIOD_MS));
  }
}

static bool touch(void *context, float *x, float *y) {
  (void)context;
  portENTER_CRITICAL(&touch_lock);
  const bool down = touch_latest.down;
  *x = touch_latest.x;
  *y = touch_latest.y;
  portEXIT_CRITICAL(&touch_lock);
  return down;
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
      /* Two frames in PSRAM. The presenter sends the windows a frame changed
       * behind the panel's scan, so a refresh shows one frame. */
      .strip_rows = M5_STOPWATCH_LCD_V_RES,
      .buffer_count = 2,
      .frame_buffer_budget =
          2 * M5_STOPWATCH_LCD_H_RES * M5_STOPWATCH_LCD_V_RES * 2,
      .byte_swap = true,
      .transfer_rows = TRANSFER_ROWS,
      .buffer_caps = MALLOC_CAP_SPIRAM,
      .wait_refresh = wait_refresh,
      .presenter_core = 0,
      /* The CO5300 takes a window whose first column and width are even. */
      .column_alignment = 2,
  };
  config.input.buttons = buttons;
  config.input.touch = touch;
  config.before_frame = before_frame;
  config.on_request = on_request;
  config.heap_limit = 4 * 1024 * 1024;
  config.js_heap_limit = 6 * 1024 * 1024;
  config.js_stack_limit = 48 * 1024;
  config.js_prefer_psram = true;
#if CONFIG_M5_POCKET_APP_MODE_AOT
  /* The compiled application used 68,048 bytes of stack while it mounted
   * (measured). Internal RAM has no free block of 80 KiB at this point, so
   * the stack is in PSRAM; the host runs the request hook, which writes NVS
   * and starts Wi-Fi, on its internal-stack helper task. */
  config.task_stack_bytes = 256 * 1024;
  config.task_stack_caps = MALLOC_CAP_SPIRAM;
#else
  /* Internal-RAM stack: the request hook writes NVS (flash). */
  config.task_stack_bytes = 64 * 1024;
#endif
  config.task_core = 1;

  ESP_ERROR_CHECK(xTaskCreatePinnedToCore(touch_task, "touch", 4096, NULL, 4,
                                          NULL, 0) == pdPASS
                      ? ESP_OK
                      : ESP_ERR_NO_MEM);
  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
  stopwatch_services_set_app(app);
  ESP_LOGI(TAG, "running, free internal=%u psram=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
