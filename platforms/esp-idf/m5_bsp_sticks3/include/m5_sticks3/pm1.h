// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
// SPDX-License-Identifier: MIT
//
// M5PM1 power management IC, ported to ESP-IDF C from M5Unified
// src/utility/power/M5PM1_Class.{hpp,inl}.
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  M5_STICKS3_PM1_GPIO0 = 0,
  M5_STICKS3_PM1_GPIO1 = 1,
  M5_STICKS3_PM1_GPIO2 = 2,
  M5_STICKS3_PM1_GPIO3 = 3,
  M5_STICKS3_PM1_GPIO4 = 4,
} m5_sticks3_pm1_gpio_t;

typedef enum {
  M5_STICKS3_PM1_FUNC_GPIO = 0,
  M5_STICKS3_PM1_FUNC_IRQ = 1,
  M5_STICKS3_PM1_FUNC_SPECIAL = 3,
} m5_sticks3_pm1_function_t;

/// PWR_SRC bitmap: bit0 5VIN valid, bit1 5VINOUT valid, bit2 VBAT node.
enum {
  M5_STICKS3_PM1_SRC_VIN = 1 << 0,
  M5_STICKS3_PM1_SRC_VINOUT = 1 << 1,
  M5_STICKS3_PM1_SRC_BATTERY = 1 << 2,
};

/// Power key events reported by m5_sticks3_pm1_take_key_event().
typedef enum {
  M5_STICKS3_PM1_KEY_NONE = 0,
  M5_STICKS3_PM1_KEY_CLICK = 1,
  M5_STICKS3_PM1_KEY_DOUBLE_CLICK = 2,
} m5_sticks3_pm1_key_event_t;

/// Adds the PMIC to the bus, reads its device id and wake source, and turns
/// off I2C idle sleep and the PMIC watchdog.
esp_err_t m5_sticks3_pm1_begin(i2c_master_bus_handle_t bus);

/// Wake source bits latched by m5_sticks3_pm1_begin(): 0x01 timer, 0x02 VIN,
/// 0x04 power key, 0x08 reset key, 0x10 reset command, 0x20 GPIO, 0x40 5VINOUT.
uint8_t m5_sticks3_pm1_wake_source(void);

esp_err_t m5_sticks3_pm1_read(uint8_t reg, uint8_t *data, size_t length);
esp_err_t m5_sticks3_pm1_write8(uint8_t reg, uint8_t value);
esp_err_t m5_sticks3_pm1_bits(uint8_t reg, uint8_t mask, bool on);

esp_err_t m5_sticks3_pm1_set_gpio_function(m5_sticks3_pm1_gpio_t pin,
                                           m5_sticks3_pm1_function_t function);
esp_err_t m5_sticks3_pm1_set_gpio_output_mode(m5_sticks3_pm1_gpio_t pin,
                                              bool output);
esp_err_t m5_sticks3_pm1_set_gpio_open_drain(m5_sticks3_pm1_gpio_t pin,
                                             bool open_drain);
esp_err_t m5_sticks3_pm1_set_gpio_output(m5_sticks3_pm1_gpio_t pin, bool high);
esp_err_t m5_sticks3_pm1_get_gpio_inputs(uint8_t *bits);

/// Grove/Hat 5 V boost output.
esp_err_t m5_sticks3_pm1_set_ext_output(bool enable);
esp_err_t m5_sticks3_pm1_set_battery_charge(bool enable);
esp_err_t m5_sticks3_pm1_get_power_source(uint8_t *sources);

esp_err_t m5_sticks3_pm1_get_battery_mv(uint16_t *millivolt);
esp_err_t m5_sticks3_pm1_get_vbus_mv(uint16_t *millivolt);
esp_err_t m5_sticks3_pm1_get_5vout_mv(uint16_t *millivolt);

/// Reads and clears a pending power key click (IRQ_STATUS3 bits 0 and 2).
m5_sticks3_pm1_key_event_t m5_sticks3_pm1_take_key_event(void);

/// Sends the shutdown command. On battery power the board switches off.
esp_err_t m5_sticks3_pm1_power_off(void);

#ifdef __cplusplus
}
#endif
