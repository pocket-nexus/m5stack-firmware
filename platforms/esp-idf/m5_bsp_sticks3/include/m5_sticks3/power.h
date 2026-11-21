// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
// SPDX-License-Identifier: MIT
//
// Battery and charge state of the StickS3, following M5Unified
// src/utility/Power_Class.inl (getBatteryLevel, getChargeState) for the
// board_M5StickS3 / pmic_m5pm1 case.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Battery level in percent (0..100) from the PMIC battery voltage, with
/// M5Unified's linear 3300..4150 mV mapping. Returns -1 when the voltage
/// cannot be read.
int m5_sticks3_battery_level(void);

/// Battery voltage in millivolts, or 0 when it cannot be read.
uint16_t m5_sticks3_battery_mv(void);

/// True while the charger reports charging (CHG_STAT on PMIC G0, low active).
bool m5_sticks3_is_charging(void);

/// True while 5 V is present on USB or the Hat 5VIN pin.
bool m5_sticks3_is_usb_powered(void);

/// Switches the board off through the PMIC.
esp_err_t m5_sticks3_power_off(void);

#ifdef __cplusplus
}
#endif
