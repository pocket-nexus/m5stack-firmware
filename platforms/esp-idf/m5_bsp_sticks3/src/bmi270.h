/*
 * SPDX-FileCopyrightText: 2023 Bosch Sensortec GmbH
 * SPDX-FileCopyrightText: 2026 lbuque <1102390310@qq.com>
 * SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
 *
 * SPDX-License-Identifier: MIT AND BSD-3-Clause
 */
/*
 * Minimal BMI270 driver: the register sequence of the Bosch SensorAPI
 * bmi270_init() and the sensor configuration of lbuque/BMI270_BMM150_Sensor
 * 0.1.2 (src/bmi270_bmm150.c, configure_bmi2_sensor). Copied from
 * platforms/esp-idf/m5_bsp_stopwatch/src/bmi270.h with the symbol prefix
 * changed.
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#define M5_STICKS3_BMI270_CONFIG_SIZE 8192

/* Bosch configuration file (bmi270_config.c, BSD-3-Clause). */
extern const uint8_t
    m5_sticks3_bmi270_config_file[M5_STICKS3_BMI270_CONFIG_SIZE];

typedef struct {
  int16_t acc[3]; /* x, y, z, sensor frame */
  int16_t gyr[3];
} m5_sticks3_bmi270_raw_t;

/* Chip ID check, soft reset, configuration upload, INTERNAL_STATUS check,
 * accelerometer ±4 g / 100 Hz and gyroscope ±2000 dps / 100 Hz enabled. */
esp_err_t m5_sticks3_bmi270_begin(i2c_master_bus_handle_t bus, uint8_t address);
esp_err_t m5_sticks3_bmi270_read(m5_sticks3_bmi270_raw_t *out);
