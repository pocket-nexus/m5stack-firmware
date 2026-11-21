// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
// SPDX-License-Identifier: MIT
//
// StickS3 IMU. The axis order is M5Unified src/utility/IMU_Class.inl
// (IMU_Class::begin, board_M5StickS3: axis_order_yxz | axis_invert_y, i.e.
// X = +Y, Y = -X, Z = +Z of the BMI270); the register driver is bmi270.c.

#include "m5_sticks3/imu.h"
#include "m5_sticks3/board.h"

#include "bmi270.h"
#include "esp_check.h"

static const char *TAG = "m5_sticks3_imu";

// ±4 g: 8192 LSB/g; ±2000 dps: 16.384 LSB/dps.
#define ACCEL_LSB_PER_G 8192.0f
#define GYRO_LSB_PER_DPS 16.384f

static bool s_started;

esp_err_t m5_sticks3_imu_init(void) {
  if (s_started) {
    return ESP_OK;
  }
  i2c_master_bus_handle_t bus;
  ESP_RETURN_ON_ERROR(m5_sticks3_i2c_bus(&bus), TAG, "I2C bus");
  ESP_RETURN_ON_ERROR(m5_sticks3_bmi270_begin(bus, M5_STICKS3_BMI270_ADDR), TAG,
                      "BMI270");
  s_started = true;
  return ESP_OK;
}

esp_err_t m5_sticks3_imu_read(m5_sticks3_imu_sample_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(s_started, ESP_ERR_INVALID_STATE, TAG, "not started");
  m5_sticks3_bmi270_raw_t raw;
  ESP_RETURN_ON_ERROR(m5_sticks3_bmi270_read(&raw), TAG, "read");
  out->accel_x = (float)raw.acc[1] / ACCEL_LSB_PER_G;
  out->accel_y = -(float)raw.acc[0] / ACCEL_LSB_PER_G;
  out->accel_z = (float)raw.acc[2] / ACCEL_LSB_PER_G;
  out->gyro_x = (float)raw.gyr[1] / GYRO_LSB_PER_DPS;
  out->gyro_y = -(float)raw.gyr[0] / GYRO_LSB_PER_DPS;
  out->gyro_z = (float)raw.gyr[2] / GYRO_LSB_PER_DPS;
  return ESP_OK;
}
