// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
// SPDX-License-Identifier: MIT
//
// Ported to ESP-IDF C (i2c_master) from M5Unified
// src/utility/power/M5PM1_Class.inl. Register map and command values are
// unchanged.

#include "m5_sticks3/pm1.h"
#include "m5_sticks3/board.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "m5_sticks3_pm1";

#define PM1_REG_DEVICE_ID 0x00
#define PM1_REG_PWR_SRC 0x04
#define PM1_REG_WAKE_SRC 0x05
#define PM1_REG_PWR_CFG 0x06
#define PM1_REG_I2C_CFG 0x09
#define PM1_REG_WDT_CNT 0x0A
#define PM1_REG_SYS_CMD 0x0C
#define PM1_REG_GPIO_MODE 0x10
#define PM1_REG_GPIO_OUT 0x11
#define PM1_REG_GPIO_IN 0x12
#define PM1_REG_GPIO_DRV 0x13
#define PM1_REG_GPIO_FUNC0 0x16
#define PM1_REG_VBAT_L 0x22
#define PM1_REG_VIN_L 0x24
#define PM1_REG_5VOUT_L 0x26
#define PM1_REG_IRQ_STATUS3 0x42

#define PM1_PWR_CFG_CHG_EN (1 << 0)
#define PM1_PWR_CFG_BOOST_EN (1 << 3)
#define PM1_SYS_CMD_SHUTDOWN 0xA1

#define PM1_TIMEOUT_MS 50

static i2c_master_dev_handle_t s_dev;
static uint8_t s_wake_source;

esp_err_t m5_sticks3_pm1_read(uint8_t reg, uint8_t *data, size_t length) {
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  return i2c_master_transmit_receive(s_dev, &reg, 1, data, length,
                                     PM1_TIMEOUT_MS);
}

esp_err_t m5_sticks3_pm1_write8(uint8_t reg, uint8_t value) {
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  const uint8_t buffer[2] = {reg, value};
  return i2c_master_transmit(s_dev, buffer, sizeof(buffer), PM1_TIMEOUT_MS);
}

esp_err_t m5_sticks3_pm1_bits(uint8_t reg, uint8_t mask, bool on) {
  uint8_t value = 0;
  // A failed read must not rewrite the other bits of the register.
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_read(reg, &value, 1), TAG, "read 0x%02x",
                      reg);
  value = on ? (uint8_t)(value | mask) : (uint8_t)(value & ~mask);
  return m5_sticks3_pm1_write8(reg, value);
}

esp_err_t m5_sticks3_pm1_begin(i2c_master_bus_handle_t bus) {
  if (s_dev) {
    return ESP_OK;
  }
  const i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_STICKS3_PM1_ADDR,
      .scl_speed_hz = M5_STICKS3_I2C_HZ,
  };
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &s_dev), TAG,
                      "add device");
  // The first transfer after an idle period can be NACKed; the next one
  // answers (M5PM1_Class::begin).
  uint8_t id[4] = {0};
  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < 3 && err != ESP_OK; ++attempt) {
    err = m5_sticks3_pm1_read(PM1_REG_DEVICE_ID, id, sizeof(id));
    if (err != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  if (err != ESP_OK) {
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    ESP_LOGE(TAG, "no M5PM1 at 0x%02x: %s", M5_STICKS3_PM1_ADDR,
             esp_err_to_name(err));
    return err;
  }
  uint8_t wake = 0;
  if (m5_sticks3_pm1_read(PM1_REG_WAKE_SRC, &wake, 1) == ESP_OK) {
    s_wake_source = wake & 0x7F;
  }
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_write8(PM1_REG_I2C_CFG, 0x00), TAG,
                      "disable I2C idle sleep");
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_write8(PM1_REG_WDT_CNT, 0x00), TAG,
                      "disable watchdog");
  ESP_LOGI(TAG, "M5PM1 id %02x%02x%02x%02x wake 0x%02x", id[0], id[1], id[2],
           id[3], s_wake_source);
  return ESP_OK;
}

uint8_t m5_sticks3_pm1_wake_source(void) { return s_wake_source; }

esp_err_t m5_sticks3_pm1_set_gpio_function(m5_sticks3_pm1_gpio_t pin,
                                           m5_sticks3_pm1_function_t function) {
  // GPIO0..3 use FUNC0; StickS3 code never touches GPIO4 (FUNC1).
  ESP_RETURN_ON_FALSE(pin <= M5_STICKS3_PM1_GPIO3, ESP_ERR_INVALID_ARG, TAG,
                      "pin %d", (int)pin);
  const uint8_t shift = (uint8_t)(pin * 2);
  uint8_t value = 0;
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_read(PM1_REG_GPIO_FUNC0, &value, 1), TAG,
                      "read FUNC0");
  value = (uint8_t)((value & ~(0x03 << shift)) | ((uint8_t)function << shift));
  return m5_sticks3_pm1_write8(PM1_REG_GPIO_FUNC0, value);
}

esp_err_t m5_sticks3_pm1_set_gpio_output_mode(m5_sticks3_pm1_gpio_t pin,
                                              bool output) {
  return m5_sticks3_pm1_bits(PM1_REG_GPIO_MODE, (uint8_t)(1 << pin), output);
}

esp_err_t m5_sticks3_pm1_set_gpio_open_drain(m5_sticks3_pm1_gpio_t pin,
                                             bool open_drain) {
  return m5_sticks3_pm1_bits(PM1_REG_GPIO_DRV, (uint8_t)(1 << pin), open_drain);
}

esp_err_t m5_sticks3_pm1_set_gpio_output(m5_sticks3_pm1_gpio_t pin, bool high) {
  return m5_sticks3_pm1_bits(PM1_REG_GPIO_OUT, (uint8_t)(1 << pin), high);
}

esp_err_t m5_sticks3_pm1_get_gpio_inputs(uint8_t *bits) {
  return m5_sticks3_pm1_read(PM1_REG_GPIO_IN, bits, 1);
}

esp_err_t m5_sticks3_pm1_set_ext_output(bool enable) {
  return m5_sticks3_pm1_bits(PM1_REG_PWR_CFG, PM1_PWR_CFG_BOOST_EN, enable);
}

esp_err_t m5_sticks3_pm1_set_battery_charge(bool enable) {
  return m5_sticks3_pm1_bits(PM1_REG_PWR_CFG, PM1_PWR_CFG_CHG_EN, enable);
}

esp_err_t m5_sticks3_pm1_get_power_source(uint8_t *sources) {
  uint8_t value = 0;
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_read(PM1_REG_PWR_SRC, &value, 1), TAG,
                      "read PWR_SRC");
  *sources = value & 0x07;
  return ESP_OK;
}

static esp_err_t read_mv(uint8_t reg, uint16_t *millivolt) {
  uint8_t buffer[2] = {0};
  ESP_RETURN_ON_ERROR(m5_sticks3_pm1_read(reg, buffer, sizeof(buffer)), TAG,
                      "read 0x%02x", reg);
  *millivolt = (uint16_t)((buffer[1] << 8) | buffer[0]);
  return ESP_OK;
}

esp_err_t m5_sticks3_pm1_get_battery_mv(uint16_t *millivolt) {
  return read_mv(PM1_REG_VBAT_L, millivolt);
}

esp_err_t m5_sticks3_pm1_get_vbus_mv(uint16_t *millivolt) {
  return read_mv(PM1_REG_VIN_L, millivolt);
}

esp_err_t m5_sticks3_pm1_get_5vout_mv(uint16_t *millivolt) {
  return read_mv(PM1_REG_5VOUT_L, millivolt);
}

m5_sticks3_pm1_key_event_t m5_sticks3_pm1_take_key_event(void) {
  uint8_t status = 0;
  if (m5_sticks3_pm1_read(PM1_REG_IRQ_STATUS3, &status, 1) != ESP_OK) {
    return M5_STICKS3_PM1_KEY_NONE;
  }
  const uint8_t pending = status & ((1 << 0) | (1 << 2));
  if (!pending) {
    return M5_STICKS3_PM1_KEY_NONE;
  }
  // Write zero to clear the consumed bits only (M5PM1_Class::getPekPress).
  if (m5_sticks3_pm1_write8(PM1_REG_IRQ_STATUS3, (uint8_t)(~pending & 0x07)) !=
      ESP_OK) {
    return M5_STICKS3_PM1_KEY_NONE;
  }
  return (pending & (1 << 2)) ? M5_STICKS3_PM1_KEY_DOUBLE_CLICK
                              : M5_STICKS3_PM1_KEY_CLICK;
}

esp_err_t m5_sticks3_pm1_power_off(void) {
  return m5_sticks3_pm1_write8(PM1_REG_SYS_CMD, PM1_SYS_CMD_SHUTDOWN);
}
