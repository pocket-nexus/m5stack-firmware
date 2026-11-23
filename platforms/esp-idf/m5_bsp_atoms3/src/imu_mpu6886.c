// SPDX-FileCopyrightText: 2021 M5Stack
// SPDX-License-Identifier: MIT
//
// Ported to ESP-IDF from M5Unified 0.3.0
// src/utility/imu/MPU6886_Class.inl (begin, setGyroFsr, setAccelFsr,
// getImuRawData). Register values are unchanged. The AtomS3's IMU behind
// the m5_atoms3/imu.h API (src/imu.c).
#include "imu_chips.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "m5_atoms3_imu";

#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_CONFIG2 0x1D
#define REG_FIFO_EN 0x23
#define REG_INT_PIN_CFG 0x37
#define REG_INT_ENABLE 0x38
#define REG_INT_STATUS 0x3A
#define REG_ACCEL_XOUT_H 0x3B
#define REG_USER_CTRL 0x6A
#define REG_PWR_MGMT_1 0x6B
#define REG_WHOAMI 0x75

#define I2C_TIMEOUT_MS 20
#define ACCEL_RES (8.0f / 32768.0f)
#define GYRO_RES (2000.0f / 32768.0f)

static esp_err_t write_reg(const m5_atoms3_imu_t *imu, uint8_t reg,
                           uint8_t value) {
  const uint8_t data[2] = {reg, value};
  return i2c_master_transmit(imu->dev, data, sizeof(data), I2C_TIMEOUT_MS);
}

static esp_err_t read_regs(const m5_atoms3_imu_t *imu, uint8_t reg,
                           uint8_t *data, size_t length) {
  return i2c_master_transmit_receive(imu->dev, &reg, 1, data, length,
                                     I2C_TIMEOUT_MS);
}

esp_err_t m5_atoms3_mpu6886_init(i2c_master_bus_handle_t bus,
                                 m5_atoms3_imu_t *out) {
  ESP_RETURN_ON_FALSE(bus && out, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
  out->dev = NULL;
  out->who_am_i = 0;
  if (i2c_master_probe(bus, M5_ATOMS3_MPU6886_ADDRESS, I2C_TIMEOUT_MS) !=
      ESP_OK) {
    return ESP_ERR_NOT_FOUND;
  }
  const i2c_device_config_t dev = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_ATOMS3_MPU6886_ADDRESS,
      .scl_speed_hz = 400000,
  };
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev, &out->dev), TAG,
                      "add device");
  esp_err_t err = read_regs(out, REG_WHOAMI, &out->who_am_i, 1);
  if (err != ESP_OK || (out->who_am_i != M5_ATOMS3_MPU6886_WHOAMI &&
                        out->who_am_i != 0x68 && out->who_am_i != 0x71)) {
    ESP_LOGW(TAG, "no MPU6886 (WHO_AM_I 0x%02x)", out->who_am_i);
    m5_atoms3_mpu6886_deinit(out);
    return ESP_ERR_NOT_FOUND;
  }
  write_reg(out, REG_PWR_MGMT_1, 0x80);
  vTaskDelay(pdMS_TO_TICKS(10));
  // clang-format off
  static const uint8_t init[] = {
      REG_PWR_MGMT_1,    0x01,
      REG_ACCEL_CONFIG,  0x10, // +-8 g
      REG_GYRO_CONFIG,   0x18, // +-2000 dps
      REG_CONFIG,        0x01,
      REG_SMPLRT_DIV,    0x03,
      REG_INT_PIN_CFG,   0xC0, // active low, open drain
      REG_INT_ENABLE,    0x00,
      REG_ACCEL_CONFIG2, 0x00,
      REG_USER_CTRL,     0x00,
      REG_FIFO_EN,       0x00,
  };
  // clang-format on
  for (size_t i = 0; i < sizeof(init); i += 2) {
    uint8_t readback = 0;
    int retry = 16;
    do {
      write_reg(out, init[i], init[i + 1]);
    } while ((read_regs(out, init[i], &readback, 1) != ESP_OK ||
              readback != init[i + 1]) &&
             --retry);
  }
  ESP_LOGI(TAG, "MPU6886 ready (WHO_AM_I 0x%02x)", out->who_am_i);
  return ESP_OK;
}

esp_err_t m5_atoms3_mpu6886_read(const m5_atoms3_imu_t *imu,
                                 m5_atoms3_imu_sample_t *out) {
  ESP_RETURN_ON_FALSE(imu && imu->dev && out, ESP_ERR_INVALID_ARG, TAG,
                      "invalid arg");
  uint8_t status = 0;
  ESP_RETURN_ON_ERROR(read_regs(imu, REG_INT_STATUS, &status, 1), TAG,
                      "status");
  if ((status & 1) == 0) {
    return ESP_ERR_NOT_FINISHED;
  }
  uint8_t b[14];
  ESP_RETURN_ON_ERROR(read_regs(imu, REG_ACCEL_XOUT_H, b, sizeof(b)), TAG,
                      "data");
  out->ax = (int16_t)((b[0] << 8) | b[1]) * ACCEL_RES;
  out->ay = (int16_t)((b[2] << 8) | b[3]) * ACCEL_RES;
  out->az = (int16_t)((b[4] << 8) | b[5]) * ACCEL_RES;
  out->gx = (int16_t)((b[8] << 8) | b[9]) * GYRO_RES;
  out->gy = (int16_t)((b[10] << 8) | b[11]) * GYRO_RES;
  out->gz = (int16_t)((b[12] << 8) | b[13]) * GYRO_RES;
  return ESP_OK;
}

void m5_atoms3_mpu6886_deinit(m5_atoms3_imu_t *imu) {
  if (imu && imu->dev) {
    i2c_master_bus_rm_device(imu->dev);
    imu->dev = NULL;
  }
}
