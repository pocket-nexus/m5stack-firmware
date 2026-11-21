/*
 * The MIT License
 *
 * Copyright (c) 2014-2023 Korneliusz Jarzębski
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* TI INA226, ported to C from components/power_monitor_ina226 of
 * M5Tab5-UserDemo (an ESP-IDF port of jarzebski/Arduino-INA226): configure,
 * calibrate and the four measurement reads, with the factory HAL's settings
 * (address 0x41, 16 averages, 1.1 ms conversions, 5 mOhm, 8.192 A). */
#include "esp_check.h"
#include "m5/tab5.h"
#include <math.h>

static const char *TAG = "m5_tab5_ina226";

#define INA226_ADDRESS 0x41
#define INA226_REG_CONFIG 0x00
#define INA226_REG_SHUNTVOLTAGE 0x01
#define INA226_REG_BUSVOLTAGE 0x02
#define INA226_REG_POWER 0x03
#define INA226_REG_CURRENT 0x04
#define INA226_REG_CALIBRATION 0x05
#define INA226_AVERAGES_16 2u
#define INA226_CONV_TIME_1100US 4u
#define INA226_MODE_SHUNT_BUS_CONT 7u
#define SHUNT_OHMS 0.005f
#define MAX_EXPECTED_AMPS 8.192f
#define I2C_TIMEOUT_MS 50

static i2c_master_dev_handle_t device;
static float current_lsb, power_lsb;

static esp_err_t read16(uint8_t reg, int16_t *value) {
  uint8_t buffer[2] = {0};
  ESP_RETURN_ON_ERROR(
      i2c_master_transmit_receive(device, &reg, 1, buffer, 2, I2C_TIMEOUT_MS),
      TAG, "read");
  *value = (int16_t)((buffer[0] << 8) | buffer[1]);
  return ESP_OK;
}

static esp_err_t write16(uint8_t reg, uint16_t value) {
  const uint8_t buffer[3] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
  return i2c_master_transmit(device, buffer, sizeof(buffer), I2C_TIMEOUT_MS);
}

esp_err_t m5_tab5_power_monitor_init(void) {
  if (device)
    return ESP_OK;
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  const i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = INA226_ADDRESS,
      .scl_speed_hz = 400000,
  };
  ESP_RETURN_ON_ERROR(
      i2c_master_bus_add_device(m5_tab5_i2c(), &config, &device), TAG,
      "INA226");
  const uint16_t mode =
      (uint16_t)(INA226_AVERAGES_16 << 9 | INA226_CONV_TIME_1100US << 6 |
                 INA226_CONV_TIME_1100US << 3 | INA226_MODE_SHUNT_BUS_CONT);
  ESP_RETURN_ON_ERROR(write16(INA226_REG_CONFIG, mode), TAG, "configure");
  /* calibrate(): the current LSB rounds the minimum LSB up to 0.1 mA steps. */
  float lsb = (float)(uint32_t)(MAX_EXPECTED_AMPS / 32767 * 100000000);
  lsb = ceilf(lsb / 100000000 / 0.0001f) * 0.0001f;
  current_lsb = lsb;
  power_lsb = lsb * 25;
  const uint16_t calibration = (uint16_t)(0.00512f / (lsb * SHUNT_OHMS));
  return write16(INA226_REG_CALIBRATION, calibration);
}

esp_err_t m5_tab5_power_monitor_read(m5_tab5_power_reading_t *out) {
  ESP_RETURN_ON_FALSE(device && out, ESP_ERR_INVALID_STATE, TAG, "arguments");
  int16_t bus = 0, shunt = 0, current = 0, power = 0;
  ESP_RETURN_ON_ERROR(read16(INA226_REG_BUSVOLTAGE, &bus), TAG, "bus");
  ESP_RETURN_ON_ERROR(read16(INA226_REG_SHUNTVOLTAGE, &shunt), TAG, "shunt");
  ESP_RETURN_ON_ERROR(read16(INA226_REG_POWER, &power), TAG, "power");
  ESP_RETURN_ON_ERROR(read16(INA226_REG_CURRENT, &current), TAG, "current");
  out->bus_voltage = bus * 0.00125f;
  out->shunt_voltage = shunt * 0.0000025f;
  out->power = power * power_lsb;
  out->current = current * current_lsb;
  return ESP_OK;
}
