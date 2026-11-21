// SPDX-License-Identifier: MIT
//
// StickS3 BMI270 accelerometer and gyroscope on the internal I2C bus (0x68).
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float accel_x, accel_y, accel_z; ///< g
  float gyro_x, gyro_y, gyro_z;    ///< degrees per second
} m5_sticks3_imu_sample_t;

/// Probes the BMI270, uploads the Bosch configuration file and enables the
/// accelerometer (±4 g, 100 Hz) and the gyroscope (±2000 dps, 100 Hz).
/// Requires m5_sticks3_power_init() (the I2C bus).
esp_err_t m5_sticks3_imu_init(void);

/// Reads the latest sample, in M5Unified's StickS3 board frame: X = +Y,
/// Y = -X, Z = +Z of the sensor (IMU_Class::begin, board_M5StickS3).
/// Returns ESP_ERR_INVALID_STATE before m5_sticks3_imu_init().
esp_err_t m5_sticks3_imu_read(m5_sticks3_imu_sample_t *out);

#ifdef __cplusplus
}
#endif
