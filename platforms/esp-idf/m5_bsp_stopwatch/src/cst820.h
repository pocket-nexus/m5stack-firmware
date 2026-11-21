/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * CST820 capacitive touch controller as an esp_lcd_touch driver, from the
 * factory driver (M5StopWatch-UserDemo main/hal/drivers/cst820).
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_touch.h"

#define M5_STOPWATCH_CST820_ADDR 0x15

/*
 * Adds the controller at 100 kHz and checks that the chip ID (0xA7) and
 * firmware version (0xA9) registers read non-zero. The caller resets the
 * controller first. config->x_max and y_max clamp the reported coordinates.
 */
esp_err_t m5_stopwatch_cst820_new(i2c_master_bus_handle_t bus,
                                  const esp_lcd_touch_config_t *config,
                                  esp_lcd_touch_handle_t *out);
