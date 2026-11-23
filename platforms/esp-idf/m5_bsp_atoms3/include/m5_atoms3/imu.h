// SPDX-FileCopyrightText: 2021 M5Stack
// SPDX-License-Identifier: MIT
//
// The IMU on the internal I2C bus: the AtomS3's MPU6886, ported to ESP-IDF's
// i2c_master driver from M5Unified 0.3.0 src/utility/imu/MPU6886_Class.inl,
// or the AtomS3R's BMI270 (src/bmi270.c), in M5Unified's AtomS3R axis frame.
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M5_ATOMS3_MPU6886_ADDRESS 0x68
#define M5_ATOMS3_MPU6886_WHOAMI 0x19

typedef enum {
  M5_ATOMS3_IMU_MPU6886, // AtomS3
  M5_ATOMS3_IMU_BMI270,  // AtomS3R
} m5_atoms3_imu_chip_t;

typedef struct {
  m5_atoms3_imu_chip_t chip;
  i2c_master_dev_handle_t dev; // MPU6886; the BMI270 driver keeps its own
  uint8_t who_am_i;            // MPU6886
} m5_atoms3_imu_t;

// Accelerometer in g, gyroscope in degrees per second. Ranges: MPU6886
// +-8 g and +-2000 dps; BMI270 +-4 g and +-2000 dps.
typedef struct {
  float ax, ay, az;
  float gx, gy, gz;
} m5_atoms3_imu_sample_t;

// The internal I2C bus on I2C port 1: SDA 38 / SCL 39 on the AtomS3, SDA 45
// / SCL 0 on the AtomS3R. The first call creates it; later calls return the
// same bus (the AtomS3R backlight driver is on it too).
esp_err_t m5_atoms3_i2c_internal_init(i2c_master_bus_handle_t *out);

// AtomS3: probes WHO_AM_I and applies the M5Unified MPU6886 configuration;
// ESP_ERR_NOT_FOUND when no MPU6886/MPU6050/MPU9250 answers at 0x68.
// AtomS3R: checks the BMI270 chip ID, uploads its configuration file and
// enables the accelerometer and gyroscope at 100 Hz.
esp_err_t m5_atoms3_imu_init(i2c_master_bus_handle_t bus, m5_atoms3_imu_t *out);
// Reads one sample. MPU6886: ESP_ERR_NOT_FINISHED when the data-ready bit is
// clear. BMI270: the latest sample, X = +Y, Y = -X, Z = +Z of the sensor
// (M5Unified's AtomS3R axis order).
esp_err_t m5_atoms3_imu_read(const m5_atoms3_imu_t *imu,
                             m5_atoms3_imu_sample_t *out);
// Removes the MPU6886 device; the BMI270 stays configured.
void m5_atoms3_imu_deinit(m5_atoms3_imu_t *imu);

#ifdef __cplusplus
}
#endif
