/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5PM1 power management IC driver
 * (m5stack/M5PM1 1.0.6, src/M5PM1.h and src/M5PM1.cpp).
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define M5_STOPWATCH_PM1_ADDR 0x6E

/* PM1 GPIO numbers used on the StopWatch (factory hal_pmic.cpp). */
#define M5_STOPWATCH_PM1_GPIO_CHG_STAT 2
#define M5_STOPWATCH_PM1_GPIO_CHG_PROG 3

/* Adds the device at 100 kHz, wakes it and reads the ID registers (one retry
 * after 800 ms), then disables the PM1 I2C sleep timeout. */
esp_err_t m5_stopwatch_pm1_begin(i2c_master_bus_handle_t bus);
bool m5_stopwatch_pm1_ready(void);

esp_err_t m5_stopwatch_pm1_set_i2c_sleep_time(uint8_t seconds);
/* BTN_CFG_1 CLK_DLY = 1000 ms. */
esp_err_t m5_stopwatch_pm1_btn_click_delay_1s(void);
esp_err_t m5_stopwatch_pm1_wdt_set(uint8_t timeout_sec);
esp_err_t m5_stopwatch_pm1_ldo_set_power_hold(bool enable);
esp_err_t m5_stopwatch_pm1_set_charge_enable(bool enable);
esp_err_t m5_stopwatch_pm1_set_led_en_level(bool level);
/* GPIO function, push-pull output with the given level, no pull. */
esp_err_t m5_stopwatch_pm1_gpio_output(uint8_t pin, bool level);
/* GPIO function, input, no pull. */
esp_err_t m5_stopwatch_pm1_gpio_input(uint8_t pin);
esp_err_t m5_stopwatch_pm1_gpio_get_input(uint8_t pin, uint8_t *value);
esp_err_t m5_stopwatch_pm1_set_single_reset_disable(bool disable);
esp_err_t m5_stopwatch_pm1_read_vbat(uint16_t *mv);
esp_err_t m5_stopwatch_pm1_read_vin(uint16_t *mv);
esp_err_t m5_stopwatch_pm1_shutdown(void);
