// SPDX-License-Identifier: MIT
//
// M5Stack StickS3 (K150) board facts and bring-up. Pin assignments follow
// M5GFX extras/board_spec/boards/m5sticks3.json and M5Unified; see the
// component README for the source revisions.
#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Internal I2C bus shared by the M5PM1 PMIC, the BMI270 IMU and the ES8311
// codec.
#define M5_STICKS3_I2C_PORT I2C_NUM_1
#define M5_STICKS3_I2C_SDA GPIO_NUM_47
#define M5_STICKS3_I2C_SCL GPIO_NUM_48
#define M5_STICKS3_I2C_HZ 100000

#define M5_STICKS3_PM1_ADDR 0x6E
#define M5_STICKS3_BMI270_ADDR 0x68
#define M5_STICKS3_ES8311_ADDR 0x18

// ST7789 1.14" IPS, 135x240 visible inside a 240x320 GRAM.
#define M5_STICKS3_LCD_MOSI GPIO_NUM_39
#define M5_STICKS3_LCD_SCLK GPIO_NUM_40
#define M5_STICKS3_LCD_CS GPIO_NUM_41
#define M5_STICKS3_LCD_DC GPIO_NUM_45
#define M5_STICKS3_LCD_RST GPIO_NUM_21
#define M5_STICKS3_LCD_BL GPIO_NUM_38
#define M5_STICKS3_LCD_WIDTH 135
#define M5_STICKS3_LCD_HEIGHT 240
#define M5_STICKS3_LCD_OFFSET_X 52
#define M5_STICKS3_LCD_OFFSET_Y 40

// Front key (KEY1, M5Unified BtnA) and side key (KEY2, BtnB), active low with
// external pull-ups. The power key is read by the PMIC.
#define M5_STICKS3_KEY1 GPIO_NUM_11
#define M5_STICKS3_KEY2 GPIO_NUM_12

// ES8311 codec on I2S. The speaker amplifier (AW8737) is enabled by PMIC G3.
#define M5_STICKS3_I2S_MCLK GPIO_NUM_18
#define M5_STICKS3_I2S_BCLK GPIO_NUM_17
#define M5_STICKS3_I2S_WS GPIO_NUM_15
#define M5_STICKS3_I2S_DOUT GPIO_NUM_14
#define M5_STICKS3_I2S_DIN GPIO_NUM_16

// Infrared LED and receiver.
#define M5_STICKS3_IR_TX GPIO_NUM_46
#define M5_STICKS3_IR_RX GPIO_NUM_42

typedef enum {
  M5_STICKS3_KEY_1 = 0,
  M5_STICKS3_KEY_2 = 1,
} m5_sticks3_key_t;

/// Creates the internal I2C master bus once and returns it. Later calls return
/// the same handle.
esp_err_t m5_sticks3_i2c_bus(i2c_master_bus_handle_t *out);

/// Powers up the board the way M5GFX and M5Unified do for StickS3: probes the
/// PMIC, disables its I2C idle sleep and watchdog, switches on the LCD rail
/// (PMIC G2), configures the charge-status input (G0) and the amplifier
/// enable (G3, off), enables battery charging and switches the 5 V output off.
esp_err_t m5_sticks3_power_init(void);

/// Configures KEY1 and KEY2 as inputs.
esp_err_t m5_sticks3_keys_init(void);

/// Returns true while the key is held.
bool m5_sticks3_key_pressed(m5_sticks3_key_t key);

#ifdef __cplusplus
}
#endif
