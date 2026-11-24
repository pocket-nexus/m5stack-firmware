/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5PM1 power management IC driver
 * (m5stack/M5PM1 1.0.7, src/M5PM1.h and src/M5PM1.cpp).
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define M5_COREP4X_PM1_ADDR 0x6E

#define M5_COREP4X_PM1_REG_PWR_SRC 0x04
#define M5_COREP4X_PM1_REG_WAKE_SRC 0x05
#define M5_COREP4X_PM1_REG_PWR_CFG 0x06
#define M5_COREP4X_PM1_REG_HOLD_CFG 0x07
#define M5_COREP4X_PM1_REG_GPIO_WAKE_EN 0x18
#define M5_COREP4X_PM1_REG_GPIO_WAKE_CFG 0x19
#define M5_COREP4X_PM1_REG_TIM_CNT_0 0x38
#define M5_COREP4X_PM1_REG_TIM_CFG 0x3C
#define M5_COREP4X_PM1_REG_TIM_KEY 0x3D
#define M5_COREP4X_PM1_REG_IRQ_STATUS1 0x40
#define M5_COREP4X_PM1_REG_IRQ_STATUS2 0x41
#define M5_COREP4X_PM1_REG_IRQ_STATUS3 0x42
#define M5_COREP4X_PM1_REG_IRQ_MASK1 0x43
#define M5_COREP4X_PM1_REG_IRQ_MASK2 0x44
#define M5_COREP4X_PM1_REG_IRQ_MASK3 0x45
#define M5_COREP4X_PM1_REG_BTN_STATUS 0x48
#define M5_COREP4X_PM1_REG_BTN_CFG_1 0x49

#define M5_COREP4X_PM1_PWR_CFG_CHG_EN (1 << 0)
#define M5_COREP4X_PM1_PWR_CFG_DCDC_EN (1 << 1)
#define M5_COREP4X_PM1_PWR_CFG_LDO_EN (1 << 2)
#define M5_COREP4X_PM1_PWR_CFG_BOOST_EN (1 << 3)
#define M5_COREP4X_PM1_PWR_CFG_LED_CTRL (1 << 4)

/* Adds the device at 100 kHz, wakes it with an address probe and reads the
 * ID registers (one retry after 800 ms), then disables the PM1 I2C sleep
 * timeout. */
esp_err_t m5_corep4x_pm1_begin(i2c_master_bus_handle_t bus);
bool m5_corep4x_pm1_ready(void);

esp_err_t m5_corep4x_pm1_read(uint8_t reg, uint8_t *data, size_t len);
esp_err_t m5_corep4x_pm1_write(uint8_t reg, uint8_t value);
/* Read-modify-write of the bits in mask. */
esp_err_t m5_corep4x_pm1_update(uint8_t reg, uint8_t mask, uint8_t value);

esp_err_t m5_corep4x_pm1_wdt_set(uint8_t timeout_sec);

/* GPIO function; output (push-pull or open drain) with a level, no pull. */
esp_err_t m5_corep4x_pm1_gpio_output(uint8_t pin, bool open_drain, bool level);
/* GPIO function; input with the drive set to open drain, no pull (the
 * M5PM1 setup for a high-impedance pin). */
esp_err_t m5_corep4x_pm1_gpio_hiz(uint8_t pin);
esp_err_t m5_corep4x_pm1_gpio_get(uint8_t pin, bool *level);
/* FUNC0/1 two-bit function of a pin (M5PM1_GPIO_FUNC_*). */
#define M5_COREP4X_PM1_FUNC_GPIO 0
#define M5_COREP4X_PM1_FUNC_IRQ 1
esp_err_t m5_corep4x_pm1_gpio_function(uint8_t pin, uint8_t function);
/* PUPD0/1 two-bit pull of a pin: 0 none, 1 up, 2 down. */
esp_err_t m5_corep4x_pm1_gpio_pull(uint8_t pin, uint8_t pull);
/* GPIO_MODE bit cleared: input. */
esp_err_t m5_corep4x_pm1_gpio_input(uint8_t pin);
/* GPIO_DRV bit: 1 open drain, 0 push-pull. */
esp_err_t m5_corep4x_pm1_gpio_drive(uint8_t pin, bool open_drain);
/* GPIO_WAKE_EN and GPIO_WAKE_CFG (falling edge when !rising) of a pin. */
esp_err_t m5_corep4x_pm1_gpio_wake(uint8_t pin, bool enable, bool rising);
/* I2C_CFG [3:0]: the PM1 sleeps after `seconds` without I2C traffic (0
 * never), read back. */
esp_err_t m5_corep4x_pm1_i2c_sleep(uint8_t seconds);

/* ADC readings in mV (12-bit little-endian registers). */
esp_err_t m5_corep4x_pm1_read_vbat(uint16_t *mv);
esp_err_t m5_corep4x_pm1_read_vin(uint16_t *mv);
esp_err_t m5_corep4x_pm1_read_5vinout(uint16_t *mv);

/* SYS_CMD with key 0xA after the 120 ms the M5PM1 driver waits. */
esp_err_t m5_corep4x_pm1_shutdown(void);
esp_err_t m5_corep4x_pm1_reboot(void);
