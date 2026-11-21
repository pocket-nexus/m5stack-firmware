// SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
// SPDX-License-Identifier: MIT
//
// M5Stack AtomS3 (SKU C123) pin map. Values from M5GFX 0.2.19
// (src/M5GFX.cpp, board_M5AtomS3), M5Unified 0.3.0 (src/M5Unified.inl) and
// the M5AtomS3-UserDemo factory firmware (src/main.cpp).
#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 0.85" 128x128 IPS, GC9107 on SPI3 (M5GFX autodetect, GC9107 branch).
#define M5_ATOMS3_LCD_WIDTH 128
#define M5_ATOMS3_LCD_HEIGHT 128
#define M5_ATOMS3_LCD_SPI_HOST SPI3_HOST
#define M5_ATOMS3_LCD_PIN_MOSI GPIO_NUM_21
#define M5_ATOMS3_LCD_PIN_SCLK GPIO_NUM_17
#define M5_ATOMS3_LCD_PIN_DC GPIO_NUM_33
#define M5_ATOMS3_LCD_PIN_CS GPIO_NUM_15
#define M5_ATOMS3_LCD_PIN_RST GPIO_NUM_34
#define M5_ATOMS3_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
// GC9107 GRAM is 128x160; the visible 128x128 window starts at row 32.
#define M5_ATOMS3_LCD_GAP_X 0
#define M5_ATOMS3_LCD_GAP_Y 32

// Backlight: LEDC PWM, 256 Hz, 9-bit, offset 48 (M5GFX _set_pwm_backlight).
#define M5_ATOMS3_BACKLIGHT_PIN GPIO_NUM_16
#define M5_ATOMS3_BACKLIGHT_LEDC_CHANNEL 7
#define M5_ATOMS3_BACKLIGHT_FREQ_HZ 256
#define M5_ATOMS3_BACKLIGHT_OFFSET 48

// The button under the screen (BtnA), active low with an external pull-up.
#define M5_ATOMS3_BUTTON_PIN GPIO_NUM_41

// Internal I2C bus: MPU6886 IMU at 0x68.
#define M5_ATOMS3_I2C_INTERNAL_SDA GPIO_NUM_38
#define M5_ATOMS3_I2C_INTERNAL_SCL GPIO_NUM_39

// Port A (HY2.0-4P Grove): G2 is SDA / white, G1 is SCL / yellow.
#define M5_ATOMS3_PORT_A_PIN_G1 GPIO_NUM_1
#define M5_ATOMS3_PORT_A_PIN_G2 GPIO_NUM_2

// Infrared LED, driven by RMT with a 38 kHz carrier.
#define M5_ATOMS3_IR_PIN GPIO_NUM_4

#ifdef __cplusplus
}
#endif
