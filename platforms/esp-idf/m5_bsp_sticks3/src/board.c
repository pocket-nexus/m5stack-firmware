// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
// SPDX-License-Identifier: MIT
//
// StickS3 power-up, keys and battery state. The PMIC sequence follows M5GFX
// src/board_detect/m5/pmic_ops.hpp (sticks3_power_on) and M5Unified
// src/M5Unified.inl / src/utility/Power_Class.inl (board_M5StickS3).

#include "m5_sticks3/board.h"
#include "m5_sticks3/pm1.h"
#include "m5_sticks3/power.h"

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

static const char *TAG = "m5_sticks3";

static i2c_master_bus_handle_t s_bus;

esp_err_t m5_sticks3_i2c_bus(i2c_master_bus_handle_t *out) {
  if (!s_bus) {
    const i2c_master_bus_config_t config = {
        .i2c_port = M5_STICKS3_I2C_PORT,
        .sda_io_num = M5_STICKS3_I2C_SDA,
        .scl_io_num = M5_STICKS3_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        // The board has external pull-ups; the internal ones are harmless.
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &s_bus), TAG, "I2C bus");
  }
  *out = s_bus;
  return ESP_OK;
}

esp_err_t m5_sticks3_power_init(void) {
  i2c_master_bus_handle_t bus;
  ESP_RETURN_ON_ERROR(m5_sticks3_i2c_bus(&bus), TAG, "I2C bus");
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_begin(bus), TAG, "M5PM1");

  // PM1 G2 drives the LCD power switch (L3B enable): GPIO function, push-pull
  // output, high.
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_gpio_function(
                          M5_STICKS3_PM1_GPIO2, M5_STICKS3_PM1_FUNC_GPIO),
                      TAG, "G2 function");
  ESP_RETURN_ON_ERROR(
      m5_sticks3_pm1_set_gpio_output_mode(M5_STICKS3_PM1_GPIO2, true), TAG,
      "G2 mode");
  ESP_RETURN_ON_ERROR(
      m5_sticks3_pm1_set_gpio_open_drain(M5_STICKS3_PM1_GPIO2, false), TAG,
      "G2 drive");
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_gpio_output(M5_STICKS3_PM1_GPIO2, 1),
                      TAG, "G2 high");

  // PM1 G0 reads the charger CHG_STAT line.
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_gpio_function(
                          M5_STICKS3_PM1_GPIO0, M5_STICKS3_PM1_FUNC_GPIO),
                      TAG, "G0 function");
  ESP_RETURN_ON_ERROR(
      m5_sticks3_pm1_set_gpio_output_mode(M5_STICKS3_PM1_GPIO0, false), TAG,
      "G0 mode");

  // PM1 G3 enables the speaker amplifier; keep it off until audio starts.
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_gpio_function(
                          M5_STICKS3_PM1_GPIO3, M5_STICKS3_PM1_FUNC_GPIO),
                      TAG, "G3 function");
  ESP_RETURN_ON_ERROR(
      m5_sticks3_pm1_set_gpio_output_mode(M5_STICKS3_PM1_GPIO3, true), TAG,
      "G3 mode");
  ESP_RETURN_ON_ERROR(
      m5_sticks3_pm1_set_gpio_open_drain(M5_STICKS3_PM1_GPIO3, false), TAG,
      "G3 drive");
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_gpio_output(M5_STICKS3_PM1_GPIO3, 0),
                      TAG, "G3 low");

  // UIFlow2 startup (m5stack/modules/startup/sticks3.py): 5 V output off,
  // battery charging on.
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_ext_output(false), TAG, "5V output");
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_set_battery_charge(true), TAG, "charge");

  // M5GFX waits 100 ms after switching the LCD rail on.
  vTaskDelay(pdMS_TO_TICKS(100));
  return ESP_OK;
}

// Edge times per key, written by key_isr. 64-bit values are read with the
// interrupt masked through a critical section.
typedef struct {
  int64_t press_us;
  int64_t rise_us;
} key_edges_t;

static key_edges_t s_key_edges[2];
static portMUX_TYPE s_key_lock = portMUX_INITIALIZER_UNLOCKED;

static gpio_num_t key_pin(m5_sticks3_key_t key) {
  return key == M5_STICKS3_KEY_1 ? M5_STICKS3_KEY1 : M5_STICKS3_KEY2;
}

static void key_isr(void *arg) {
  const m5_sticks3_key_t key = (m5_sticks3_key_t)(intptr_t)arg;
  const int64_t now = esp_timer_get_time();
  key_edges_t *edges = &s_key_edges[key];
  portENTER_CRITICAL_ISR(&s_key_lock);
  if (gpio_get_level(key_pin(key)) == 0) {
    // Active low: a falling edge. Bounce after a release (or during a
    // press) follows a rising edge by less than the debounce time.
    if (now - edges->rise_us >= M5_STICKS3_KEY_DEBOUNCE_US &&
        now - edges->press_us >= M5_STICKS3_KEY_DEBOUNCE_US) {
      edges->press_us = now;
    }
  } else {
    edges->rise_us = now;
  }
  portEXIT_CRITICAL_ISR(&s_key_lock);
}

esp_err_t m5_sticks3_keys_init(void) {
  const gpio_config_t config = {
      .pin_bit_mask = (1ULL << M5_STICKS3_KEY1) | (1ULL << M5_STICKS3_KEY2),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "key GPIO");
  const esp_err_t err = gpio_install_isr_service(0);
  ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG,
                      "GPIO ISR service");
  ESP_RETURN_ON_ERROR(gpio_isr_handler_add(M5_STICKS3_KEY1, key_isr,
                                           (void *)(intptr_t)M5_STICKS3_KEY_1),
                      TAG, "KEY1 ISR");
  ESP_RETURN_ON_ERROR(gpio_isr_handler_add(M5_STICKS3_KEY2, key_isr,
                                           (void *)(intptr_t)M5_STICKS3_KEY_2),
                      TAG, "KEY2 ISR");
  return ESP_OK;
}

bool m5_sticks3_key_pressed(m5_sticks3_key_t key) {
  return gpio_get_level(key_pin(key)) == 0;
}

int64_t m5_sticks3_key_press_time_us(m5_sticks3_key_t key) {
  portENTER_CRITICAL(&s_key_lock);
  const int64_t press_us = s_key_edges[key].press_us;
  portEXIT_CRITICAL(&s_key_lock);
  return press_us;
}

uint16_t m5_sticks3_battery_mv(void) {
  uint16_t millivolt = 0;
  return m5_sticks3_pm1_get_battery_mv(&millivolt) == ESP_OK ? millivolt : 0;
}

int m5_sticks3_battery_level(void) {
  const uint16_t millivolt = m5_sticks3_battery_mv();
  if (millivolt == 0) {
    return -1;
  }
  // Power_Class::getBatteryLevel: (mv - 3300) * 100 / (4150 - 3350).
  const int level =
      (int)(((float)millivolt - 3300.0f) * 100.0f / (float)(4150 - 3350));
  return level < 0 ? 0 : level > 100 ? 100 : level;
}

bool m5_sticks3_is_charging(void) {
  uint8_t bits = 0;
  if (m5_sticks3_pm1_get_gpio_inputs(&bits) != ESP_OK) {
    return false;
  }
  return (bits & (1 << M5_STICKS3_PM1_GPIO0)) == 0;
}

bool m5_sticks3_is_usb_powered(void) {
  uint8_t sources = 0;
  if (m5_sticks3_pm1_get_power_source(&sources) != ESP_OK) {
    return false;
  }
  return (sources & (M5_STICKS3_PM1_SRC_VIN | M5_STICKS3_PM1_SRC_VINOUT)) != 0;
}

esp_err_t m5_sticks3_power_off(void) { return m5_sticks3_pm1_power_off(); }
