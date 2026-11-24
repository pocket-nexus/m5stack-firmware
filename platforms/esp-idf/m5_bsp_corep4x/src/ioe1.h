/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5IOE1 I/O expander driver
 * (m5stack/M5IOE1 1.0.9, src/M5IOE1.h and src/M5IOE1.cpp).
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* Searches the M5IOE1 auto-detect addresses (0x4F, 0x6F, 0x70..0x76, 0x50),
 * checks the REV byte ('W' at 0x4F..0x50, 'A' at 0x6F..0x76), reads UID and
 * revision (one retry after 800 ms), then writes I2C_CFG (sleep off,
 * 400 kHz) and re-adds the device at 400 kHz. */
esp_err_t m5_corep4x_ioe1_begin(i2c_master_bus_handle_t bus);
bool m5_corep4x_ioe1_ready(void);
uint8_t m5_corep4x_ioe1_address(void);

/* Output without pulls, push-pull or open drain; disables PWM on
 * PWM-capable pins. */
esp_err_t m5_corep4x_ioe1_output(uint8_t pin, bool open_drain);
/* Input with the pull-up on or off and the pull-down off. */
esp_err_t m5_corep4x_ioe1_input(uint8_t pin, bool pull_up);
/* Writes the output bit and verifies it by reading GPIO_OUT back. */
esp_err_t m5_corep4x_ioe1_write(uint8_t pin, bool level);
esp_err_t m5_corep4x_ioe1_read(uint8_t pin, bool *level);
esp_err_t m5_corep4x_ioe1_set_pwm_frequency(uint16_t hz);
/* I2C_CFG [3:0]: the expander sleeps after `seconds` without I2C traffic
 * (0 never), read back (M5IOE1::setI2cSleepTime). */
esp_err_t m5_corep4x_ioe1_i2c_sleep(uint8_t seconds);
/* duty in percent (0..100), converted to the 12-bit register value. */
esp_err_t m5_corep4x_ioe1_set_pwm_duty(uint8_t channel, uint8_t duty,
                                       bool polarity, bool enable);
