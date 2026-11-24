/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Declarations shared by the source files of the component. */
#pragma once

#include "m5_corep4x/bsp.h"

/* Drives an open-drain IOE1 reset line low for low_ms, releases it and
 * waits settle_ms. */
esp_err_t m5_corep4x_pulse_reset(uint8_t pin, uint32_t low_ms,
                                 uint32_t settle_ms);
