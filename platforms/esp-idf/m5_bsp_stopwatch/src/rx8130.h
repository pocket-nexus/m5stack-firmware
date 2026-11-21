/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * RX8130CE real-time clock, from the factory driver (M5StopWatch-UserDemo
 * main/hal/drivers/rx8130).
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <time.h>

#define M5_STOPWATCH_RX8130_ADDR 0x32

esp_err_t m5_stopwatch_rx8130_begin(i2c_master_bus_handle_t bus);
/* Reads the clock; repairs a day or month past the end of the calendar the
 * way RX8130_Class::getTime does. Fields follow struct tm. */
esp_err_t m5_stopwatch_rx8130_get(struct tm *out);
/* Stops the clock, writes the seven time registers and restarts it. Years
 * 2000..2099. */
esp_err_t m5_stopwatch_rx8130_set(const struct tm *time);
