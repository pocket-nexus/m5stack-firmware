/*
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
/* Private interfaces shared by the BSP source files. */
#pragma once

#include "m5_stopwatch/bsp.h"

/* I2C bus created by m5_stopwatch_init, or NULL before it. */
i2c_master_bus_handle_t m5_stopwatch_board_i2c(void);

/* Creates the stop timer and sets the motor duty to 0. */
esp_err_t m5_stopwatch_vibrate_init(void);
