// SPDX-FileCopyrightText: 2021 M5Stack
// SPDX-License-Identifier: MIT
//
// MPU6886 on the internal I2C bus, ported to ESP-IDF's i2c_master driver
// from M5Unified 0.3.0 src/utility/imu/MPU6886_Class.inl.
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M5_ATOMS3_MPU6886_ADDRESS 0x68
#define M5_ATOMS3_MPU6886_WHOAMI 0x19

typedef struct {
  i2c_master_dev_handle_t dev;
  uint8_t who_am_i;
} m5_atoms3_imu_t;

// Accelerometer in g (+-8 g range), gyroscope in degrees per second
// (+-2000 dps range).
typedef struct {
  float ax, ay, az;
  float gx, gy, gz;
} m5_atoms3_imu_sample_t;

// Creates the internal I2C bus (SDA 38, SCL 39) on I2C port 1.
esp_err_t m5_atoms3_i2c_internal_init(i2c_master_bus_handle_t *out);

// Probes WHO_AM_I and applies the M5Unified MPU6886 configuration.
// ESP_ERR_NOT_FOUND when no MPU6886/MPU6050/MPU9250 answers at 0x68.
esp_err_t m5_atoms3_imu_init(i2c_master_bus_handle_t bus, m5_atoms3_imu_t *out);
// Reads one sample. ESP_ERR_NOT_FINISHED when the data-ready bit is clear.
esp_err_t m5_atoms3_imu_read(const m5_atoms3_imu_t *imu,
                             m5_atoms3_imu_sample_t *out);
void m5_atoms3_imu_deinit(m5_atoms3_imu_t *imu);

#ifdef __cplusplus
}
#endif
