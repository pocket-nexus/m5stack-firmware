/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* LSM6DS3TR-C on the internal I2C bus (0x6A, or 0x6B), as the factory's
 * otool_lsm6ds3trc driver sets it up: accelerometer 104 Hz +/-2 g,
 * gyroscope 104 Hz 2000 dps. */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One reading in the sensor's own axes (the package sits turned 90
 * degrees on the board: board = (y, -x, z)). */
typedef struct {
  float ax, ay, az; /* g */
  float gx, gy, gz; /* degrees per second */
  bool acc_valid;   /* STATUS_REG XLDA */
  bool gyr_valid;   /* STATUS_REG GDA */
  int64_t time_ms;  /* esp_timer time of the read */
} m5_corep4x_imu_sample_t;

/* L3B on, 50 ms, then the device at 0x6A or 0x6B (WHO_AM_I 0x6A), a
 * software reset (at most 100 ms) and the factory registers: CTRL3_C 0x44,
 * CTRL1_XL 0x40, CTRL2_G 0x4C. Later calls return ESP_OK. */
esp_err_t m5_corep4x_imu_init(void);
bool m5_corep4x_imu_ready(void);
/* STATUS_REG, then the 12 output bytes when a flag is set (both flags
 * false and ESP_OK when no new data). */
esp_err_t m5_corep4x_imu_read(m5_corep4x_imu_sample_t *out);
/* The factory imu_prepare_for_sleep. With motion wake: the sensor
 * initialised if needed, gyroscope off, accelerometer on and the wake-up
 * interrupt on INT1 (active low, open drain, latched, threshold 0x04
 * (about 125 mg), duration 1), PM1 GPIO0 as a pulled-up input with its IRQ
 * unmasked. Without: both sensors and INT1 off, PM1 GPIO0 wake disabled,
 * IRQ masked, no pull. */
esp_err_t m5_corep4x_imu_prepare_sleep(bool motion_wake);

#ifdef __cplusplus
}
#endif
