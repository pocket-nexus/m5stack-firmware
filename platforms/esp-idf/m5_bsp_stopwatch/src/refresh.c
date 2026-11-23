/*
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Panel refresh timing from the CO5300 tearing-effect output (GPIO38). The
 * init sequence in co5300.c turns the output on (0x35) and sets the tear
 * scanline to 466 (0x44), the row after the last one: the signal rises when
 * the panel has finished a scan of its 466 rows.
 */
#include "board.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* A period outside these bounds is a missed or a spurious edge. */
#define PERIOD_MIN_US 4000
#define PERIOD_MAX_US 100000

static const char *TAG = "m5_stopwatch";

static SemaphoreHandle_t s_edge;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_start_us;
static uint32_t s_period_us;

static void IRAM_ATTR on_edge(void *context) {
  (void)context;
  const int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&s_lock);
  const int64_t period = now - s_start_us;
  if (period >= PERIOD_MIN_US && period <= PERIOD_MAX_US)
    s_period_us = (uint32_t)period;
  s_start_us = now;
  portEXIT_CRITICAL_ISR(&s_lock);
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(s_edge, &woken);
  if (woken == pdTRUE)
    portYIELD_FROM_ISR();
}

esp_err_t m5_stopwatch_refresh_init(void) {
  if (s_edge)
    return ESP_OK;
  s_edge = xSemaphoreCreateBinary();
  ESP_RETURN_ON_FALSE(s_edge, ESP_ERR_NO_MEM, TAG, "TE semaphore");
  /* Another component may have installed the service. */
  const esp_err_t service = gpio_install_isr_service(0);
  ESP_RETURN_ON_FALSE(service == ESP_OK || service == ESP_ERR_INVALID_STATE,
                      service, TAG, "GPIO ISR service");
  ESP_RETURN_ON_ERROR(
      gpio_set_intr_type(M5_STOPWATCH_PIN_LCD_TE, GPIO_INTR_POSEDGE), TAG,
      "TE edge");
  ESP_RETURN_ON_ERROR(
      gpio_isr_handler_add(M5_STOPWATCH_PIN_LCD_TE, on_edge, NULL), TAG,
      "TE handler");
  return ESP_OK;
}

bool m5_stopwatch_display_wait_refresh(uint32_t timeout_ms, int64_t *start_us,
                                       uint32_t *period_us) {
  if (!s_edge || !start_us || !period_us)
    return false;
  /* Drop an edge that has passed: the caller wants the next one. */
  (void)xSemaphoreTake(s_edge, 0);
  if (xSemaphoreTake(s_edge, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
    return false;
  portENTER_CRITICAL(&s_lock);
  *start_us = s_start_us;
  *period_us = s_period_us;
  portEXIT_CRITICAL(&s_lock);
  return *period_us != 0;
}
