/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Vibration motor on IOE1 PWM channel 0 (pin IO9). The strength to duty
 * mapping is Vibrator::get_duty_cycle from the factory firmware
 * (M5StopWatch-UserDemo main/hal/hal_ioe.cpp); a one-shot esp_timer replaces
 * the factory vibrator task to stop the motor.
 */
#include "board.h"

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ioe1.h"

static const char *TAG = "m5sw_vibrate";

static esp_timer_handle_t s_timer;
static SemaphoreHandle_t s_lock;

/* strength 1..100 -> duty 25..100 %, 0 -> off. */
static uint8_t duty_for(uint8_t strength) {
  if (strength == 0) {
    return 0;
  }
  if (strength > 100) {
    strength = 100;
  }
  return (uint8_t)(25 + ((uint32_t)strength * (100 - 25)) / 100);
}

static esp_err_t set_duty(uint8_t duty) {
  /* The factory driver keeps the channel enabled and writes duty 0 to stop. */
  return m5_stopwatch_ioe1_set_pwm_duty(M5_STOPWATCH_IOE1_MOTOR_PWM_CHANNEL,
                                        duty, false, true);
}

static void stop_cb(void *arg) {
  (void)arg;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  /* A vibrate call that re-armed the timer owns the motor now. */
  if (!esp_timer_is_active(s_timer)) {
    (void)set_duty(0);
  }
  xSemaphoreGive(s_lock);
}

esp_err_t m5_stopwatch_vibrate_init(void) {
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "no mutex");
  }
  if (!s_timer) {
    const esp_timer_create_args_t args = {
        .callback = stop_cb,
        .name = "m5sw_vibrate",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&args, &s_timer), TAG, "timer");
  }
  return set_duty(0);
}

esp_err_t m5_stopwatch_vibrate(uint16_t duration_ms, uint8_t strength) {
  ESP_RETURN_ON_FALSE(s_timer, ESP_ERR_INVALID_STATE, TAG, "not started");
  if (duration_ms == 0 || strength == 0) {
    return m5_stopwatch_vibrate_stop();
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  (void)esp_timer_stop(s_timer);
  esp_err_t err = set_duty(duty_for(strength));
  if (err == ESP_OK) {
    err = esp_timer_start_once(s_timer, (uint64_t)duration_ms * 1000);
  }
  if (err != ESP_OK) {
    (void)set_duty(0);
  }
  xSemaphoreGive(s_lock);
  return err;
}

esp_err_t m5_stopwatch_vibrate_stop(void) {
  ESP_RETURN_ON_FALSE(s_timer, ESP_ERR_INVALID_STATE, TAG, "not started");
  xSemaphoreTake(s_lock, portMAX_DELAY);
  (void)esp_timer_stop(s_timer);
  esp_err_t err = set_duty(0);
  xSemaphoreGive(s_lock);
  return err;
}
