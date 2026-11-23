// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-License-Identifier: MIT
//
// M5Stack AtomS3 (SKU C123) and AtomS3R pin maps. Values from M5GFX 0.2.19
// (src/M5GFX.cpp, board_M5AtomS3 and board_M5AtomS3R), M5Unified 0.3.0
// (src/M5Unified.inl), the M5AtomS3-UserDemo factory firmware
// (src/main.cpp) and uiflow-micropython (m5stack/libs/hardware/ir.py, the
// AtomS3R IR pin).
#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// The two boards share the screen size, the button, Port A and the
// application; they differ in the chip package, the panel and IMU wiring,
// the backlight and the IR pin. The board is read from the chip's package
// version in eFuse, as M5GFX's autodetect does.
typedef enum {
  // ESP32-S3FN8 (QFN56, EFUSE_PKG_VERSION_ESP32S3), no PSRAM.
  M5_ATOMS3_BOARD_ATOMS3 = 0,
  // ESP32-S3-PICO-1-N8R8 (LGA56, EFUSE_PKG_VERSION_ESP32S3PICO), 8 MB PSRAM.
  M5_ATOMS3_BOARD_ATOMS3R = 1,
} m5_atoms3_board_t;

m5_atoms3_board_t m5_atoms3_board(void);
// "AtomS3" or "AtomS3R".
const char *m5_atoms3_board_name(void);

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

// AtomS3R. The panel is a GC9107 with the AtomS3's window (or an ST7735S on
// some units, which this component does not drive) on SPI3 at 40 MHz.
#define M5_ATOMS3R_LCD_PIN_MOSI GPIO_NUM_21
#define M5_ATOMS3R_LCD_PIN_SCLK GPIO_NUM_15
#define M5_ATOMS3R_LCD_PIN_DC GPIO_NUM_42
#define M5_ATOMS3R_LCD_PIN_CS GPIO_NUM_14
#define M5_ATOMS3R_LCD_PIN_RST GPIO_NUM_48
// RDDID (0x04) identifiers of the two panels (M5GFX autodetect).
#define M5_ATOMS3R_PANEL_ID_GC9107 0x079100U
#define M5_ATOMS3R_PANEL_ID_ST7735S 0x7CU
// Internal I2C bus: BMI270 IMU at 0x68 (BMM150 behind it, unused) and the
// backlight driver at 0x30 (M5GFX Light_M5StackAtomS3R).
#define M5_ATOMS3R_I2C_INTERNAL_SDA GPIO_NUM_45
#define M5_ATOMS3R_I2C_INTERNAL_SCL GPIO_NUM_0
#define M5_ATOMS3R_BMI270_ADDRESS 0x68
#define M5_ATOMS3R_BACKLIGHT_ADDRESS 0x30
#define M5_ATOMS3R_IR_PIN GPIO_NUM_47

#ifdef __cplusplus
}
#endif
