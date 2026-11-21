/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5IOE1 I/O expander driver
 * (m5stack/M5IOE1 1.0.8, src/M5IOE1.h and src/M5IOE1.cpp).
 */
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define M5_STOPWATCH_IOE1_ADDR_PRIMARY 0x4F
#define M5_STOPWATCH_IOE1_ADDR_SECONDARY 0x6F

/* Zero-based IOE1 pin indices (M5IOE1_PIN_n = n - 1), factory hal_ioe.cpp. */
#define M5_STOPWATCH_IOE1_MUX_CTR 0  /* IO1: CH442E mux control */
#define M5_STOPWATCH_IOE1_AU_EN 2    /* IO3: audio enable */
#define M5_STOPWATCH_IOE1_TP_RST 3   /* IO4: touch reset */
#define M5_STOPWATCH_IOE1_OLED_RST 4 /* IO5: OLED reset */
#define M5_STOPWATCH_IOE1_L3B_EN 7   /* IO8: L3B (display) power enable */
#define M5_STOPWATCH_IOE1_MOTOR_EN 8 /* IO9: vibration motor, PWM channel 0 */
#define M5_STOPWATCH_IOE1_SPK_PA 9   /* IO10: speaker amplifier enable */
#define M5_STOPWATCH_IOE1_MOTOR_PWM_CHANNEL 0

/* Adds the device at addr (100 kHz), reads UID and revision (one retry after
 * 800 ms), then writes I2C_CFG (sleep off, 400 kHz) and re-adds the device at
 * 400 kHz. */
esp_err_t m5_stopwatch_ioe1_begin(i2c_master_bus_handle_t bus, uint8_t addr);
bool m5_stopwatch_ioe1_ready(void);

/* Push-pull output without pulls; disables PWM on PWM-capable pins. */
esp_err_t m5_stopwatch_ioe1_pin_mode_output(uint8_t pin);
/* Writes the output bit and verifies it by reading GPIO_OUT back. */
esp_err_t m5_stopwatch_ioe1_digital_write(uint8_t pin, bool level);
esp_err_t m5_stopwatch_ioe1_digital_read(uint8_t pin, bool *level);
esp_err_t m5_stopwatch_ioe1_set_pwm_frequency(uint16_t hz);
/* duty in percent (0..100), converted to the 12-bit register value. */
esp_err_t m5_stopwatch_ioe1_set_pwm_duty(uint8_t channel, uint8_t duty,
                                         bool polarity, bool enable);
