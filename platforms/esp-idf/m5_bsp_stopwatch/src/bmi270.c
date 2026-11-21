/*
 * SPDX-FileCopyrightText: 2023 Bosch Sensortec GmbH
 * SPDX-FileCopyrightText: 2026 lbuque <1102390310@qq.com>
 * SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
 *
 * SPDX-License-Identifier: MIT AND BSD-3-Clause
 */
/*
 * BMI270 bring-up as register writes. The sequence is the one bmi270_init()
 * performs in the Bosch SensorAPI (bmi2.c: bmi2_sec_init, bmi2_soft_reset,
 * bmi2_write_config_file), without re-enabling advanced power save; the
 * sensor settings are those of configure_bmi2_sensor() in
 * lbuque/BMI270_BMM150_Sensor 0.1.2. The device runs at 400 kHz.
 */
#include "bmi270.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "m5sw_bmi270";

#define REG_CHIP_ID 0x00
#define REG_ACC_X_LSB 0x0C
#define REG_INTERNAL_STATUS 0x21
#define REG_ACC_CONF 0x40
#define REG_ACC_RANGE 0x41
#define REG_GYR_CONF 0x42
#define REG_GYR_RANGE 0x43
#define REG_INIT_CTRL 0x59
#define REG_INIT_ADDR_0 0x5B
#define REG_INIT_DATA 0x5E
#define REG_PWR_CONF 0x7C
#define REG_PWR_CTRL 0x7D
#define REG_CMD 0x7E

#define CHIP_ID 0x24
#define CMD_SOFT_RESET 0xB6
#define PWR_CONF_ADV_POWER_SAVE 0x01
#define PWR_CTRL_GYR_EN 0x02
#define PWR_CTRL_ACC_EN 0x04
#define CONFIG_LOAD_SUCCESS 0x01
#define UPLOAD_CHUNK 128

/* ACC_CONF: filter_perf (bit 7) = performance, bwp = OSR2/AVG2 (1),
 * odr = 100 Hz (0x08). ACC_RANGE: ±4 g (1). */
#define ACC_CONF_VALUE ((1 << 7) | (0x01 << 4) | 0x08)
#define ACC_RANGE_VALUE 0x01
/* GYR_CONF: filter_perf = performance, noise_perf = power optimized,
 * bwp = OSR2 (1), odr = 100 Hz (0x08). GYR_RANGE: ±2000 dps (0), OIS range
 * ±2000 dps (bit 3). */
#define GYR_CONF_VALUE ((1 << 7) | (0x01 << 4) | 0x08)
#define GYR_RANGE_VALUE ((1 << 3) | 0x00)

static i2c_master_dev_handle_t s_dev;

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
  const uint8_t buf[2] = {reg, value};
  esp_err_t err = i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
  /* Writes in advanced power save mode need 450 us between them. */
  esp_rom_delay_us(450);
  return err;
}

static esp_err_t upload_config(void) {
  uint8_t buf[1 + UPLOAD_CHUNK];
  for (size_t index = 0; index < M5_STOPWATCH_BMI270_CONFIG_SIZE;
       index += UPLOAD_CHUNK) {
    /* INIT_ADDR counts 16-bit words: bits 3..0 in ADDR_0, 11..4 in ADDR_1. */
    const uint8_t addr[3] = {REG_INIT_ADDR_0, (uint8_t)((index / 2) & 0x0F),
                             (uint8_t)((index / 2) >> 4)};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_dev, addr, sizeof(addr), 100),
                        TAG, "INIT_ADDR");
    buf[0] = REG_INIT_DATA;
    for (size_t i = 0; i < UPLOAD_CHUNK; i++) {
      buf[1 + i] = m5_stopwatch_bmi270_config_file[index + i];
    }
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_dev, buf, sizeof(buf), 100), TAG,
                        "INIT_DATA");
  }
  return ESP_OK;
}

static esp_err_t init_device(void) {
  uint8_t id = 0;
  ESP_RETURN_ON_ERROR(read_regs(REG_CHIP_ID, &id, 1), TAG, "CHIP_ID");
  ESP_RETURN_ON_FALSE(id == CHIP_ID, ESP_ERR_NOT_FOUND, TAG,
                      "chip ID 0x%02X, expected 0x%02X", id, CHIP_ID);

  ESP_RETURN_ON_ERROR(write_reg(REG_CMD, CMD_SOFT_RESET), TAG, "soft reset");
  vTaskDelay(pdMS_TO_TICKS(10)); /* SensorAPI waits 2 ms */

  uint8_t pwr_conf = 0;
  ESP_RETURN_ON_ERROR(read_regs(REG_PWR_CONF, &pwr_conf, 1), TAG, "PWR_CONF");
  ESP_RETURN_ON_ERROR(
      write_reg(REG_PWR_CONF, (uint8_t)(pwr_conf & ~PWR_CONF_ADV_POWER_SAVE)),
      TAG, "PWR_CONF");
  ESP_RETURN_ON_ERROR(write_reg(REG_INIT_CTRL, 0x00), TAG, "INIT_CTRL");
  ESP_RETURN_ON_ERROR(upload_config(), TAG, "config upload");
  ESP_RETURN_ON_ERROR(write_reg(REG_INIT_CTRL, 0x01), TAG, "INIT_CTRL");

  /* SensorAPI reads INTERNAL_STATUS 20 ms after INIT_CTRL = 1. */
  uint8_t status = 0;
  for (int i = 0; i < 5; i++) {
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(read_regs(REG_INTERNAL_STATUS, &status, 1), TAG,
                        "INTERNAL_STATUS");
    if ((status & 0x0F) == CONFIG_LOAD_SUCCESS) {
      break;
    }
  }
  ESP_RETURN_ON_FALSE((status & 0x0F) == CONFIG_LOAD_SUCCESS, ESP_FAIL, TAG,
                      "config load failed, INTERNAL_STATUS 0x%02X", status);

  ESP_RETURN_ON_ERROR(write_reg(REG_ACC_CONF, ACC_CONF_VALUE), TAG, "ACC_CONF");
  ESP_RETURN_ON_ERROR(write_reg(REG_ACC_RANGE, ACC_RANGE_VALUE), TAG,
                      "ACC_RANGE");
  ESP_RETURN_ON_ERROR(write_reg(REG_GYR_CONF, GYR_CONF_VALUE), TAG, "GYR_CONF");
  ESP_RETURN_ON_ERROR(write_reg(REG_GYR_RANGE, GYR_RANGE_VALUE), TAG,
                      "GYR_RANGE");
  ESP_RETURN_ON_ERROR(
      write_reg(REG_PWR_CTRL, PWR_CTRL_ACC_EN | PWR_CTRL_GYR_EN), TAG,
      "PWR_CTRL");
  return ESP_OK;
}

esp_err_t m5_stopwatch_bmi270_begin(i2c_master_bus_handle_t bus) {
  ESP_RETURN_ON_FALSE(!s_dev, ESP_ERR_INVALID_STATE, TAG, "already started");
  const i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_STOPWATCH_BMI270_ADDR,
      .scl_speed_hz = 400000,
  };
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG,
                      "add device");
  esp_err_t err = init_device();
  if (err != ESP_OK) {
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
  }
  return err;
}

esp_err_t m5_stopwatch_bmi270_read(m5_stopwatch_bmi270_raw_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  uint8_t d[12];
  ESP_RETURN_ON_ERROR(read_regs(REG_ACC_X_LSB, d, sizeof(d)), TAG, "data");
  for (int i = 0; i < 3; i++) {
    out->acc[i] = (int16_t)(d[2 * i] | (d[2 * i + 1] << 8));
    out->gyr[i] = (int16_t)(d[6 + 2 * i] | (d[6 + 2 * i + 1] << 8));
  }
  return ESP_OK;
}
