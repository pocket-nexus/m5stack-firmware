/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* M5Stack Tab5 board support, derived from the m5stack_tab5 BSP and the HAL
 * of M5Tab5-UserDemo. The component has no PocketJS dependency: it powers the
 * board up and returns esp_lcd / esp_lcd_touch handles and small driver
 * APIs. */
#pragma once
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "sdmmc_cmd.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Panel geometry in its native portrait scan order. */
#define M5_TAB5_LCD_WIDTH 720u
#define M5_TAB5_LCD_HEIGHT 1280u

/* System I2C: both IO expanders, touch, ES8388, ES7210, BMI270, RX8130CE,
 * INA226. */
#define M5_TAB5_I2C_PORT I2C_NUM_0
#define M5_TAB5_I2C_SDA GPIO_NUM_31
#define M5_TAB5_I2C_SCL GPIO_NUM_32
/* Port A (Grove, the factory demo's "Ext I2C") on the second controller. */
#define M5_TAB5_PORT_A_I2C_PORT I2C_NUM_1
#define M5_TAB5_PORT_A_SDA GPIO_NUM_53
#define M5_TAB5_PORT_A_SCL GPIO_NUM_54
#define M5_TAB5_LCD_BACKLIGHT GPIO_NUM_22
#define M5_TAB5_TOUCH_INT GPIO_NUM_23
#define M5_TAB5_I2S_MCLK GPIO_NUM_30
#define M5_TAB5_I2S_BCLK GPIO_NUM_27
#define M5_TAB5_I2S_WS GPIO_NUM_29
#define M5_TAB5_I2S_DOUT GPIO_NUM_26
#define M5_TAB5_I2S_DIN GPIO_NUM_28
#define M5_TAB5_SD_CLK GPIO_NUM_43
#define M5_TAB5_SD_CMD GPIO_NUM_44
#define M5_TAB5_SD_D0 GPIO_NUM_39
#define M5_TAB5_SD_D1 GPIO_NUM_40
#define M5_TAB5_SD_D2 GPIO_NUM_41
#define M5_TAB5_SD_D3 GPIO_NUM_42
#define M5_TAB5_RS485_TX GPIO_NUM_20
#define M5_TAB5_RS485_RX GPIO_NUM_21
#define M5_TAB5_RS485_DE GPIO_NUM_34

/* ---- board ---------------------------------------------------------------
 */

/* Switches behind the two PI4IOE5V6408 expanders (0x43 and 0x44). */
typedef enum {
  M5_TAB5_SWITCH_EXT_ANTENNA, /* expander 0x43 P0: RF switch to the SMA port */
  M5_TAB5_SWITCH_SPEAKER,     /* expander 0x43 P1: NS4150B amplifier */
  M5_TAB5_SWITCH_EXT_5V,      /* expander 0x43 P2: 5 V on the expansion ports */
  M5_TAB5_SWITCH_CAMERA,      /* expander 0x43 P6: camera module */
  M5_TAB5_SWITCH_WIFI,        /* expander 0x44 P0: ESP32-C6 power */
  M5_TAB5_SWITCH_USB_A_5V,    /* expander 0x44 P3: USB-A host 5 V */
  M5_TAB5_SWITCH_QUICK_CHARGE, /* expander 0x44 P5, active low */
  M5_TAB5_SWITCH_CHARGE,       /* expander 0x44 P7: IP2326 charge enable */
  M5_TAB5_SWITCH_COUNT,
} m5_tab5_switch_t;

/* Creates the system I2C bus and runs the factory expander sequence: the LCD
 * reset pulse, speaker amplifier, external 5 V, camera, ESP32-C6 and USB-A
 * 5 V on, charging off. Later calls return ESP_OK without repeating it. */
esp_err_t m5_tab5_init(void);
i2c_master_bus_handle_t m5_tab5_i2c(void);
esp_err_t m5_tab5_set_switch(m5_tab5_switch_t which, bool on);
/* The level the program last wrote, read back from the expander. */
esp_err_t m5_tab5_get_switch(m5_tab5_switch_t which, bool *on);
/* Expander 0x43 P7 reads the headphone jack, 0x44 P6 reads USB-C VBUS. */
esp_err_t m5_tab5_headphone_detect(bool *inserted);
esp_err_t m5_tab5_usb_c_detect(bool *powered);
/* Pulses expander 0x44 P4 three times, the power path's off request. */
esp_err_t m5_tab5_power_off(void);

/* ---- display -------------------------------------------------------------
 */

/* Board revisions differ by LCD and touch controller. */
typedef enum {
  M5_TAB5_PANEL_UNKNOWN = 0,
  M5_TAB5_PANEL_ILI9881C, /* LCD ILI9881C, touch GT911 at 0x14 */
  M5_TAB5_PANEL_ST7123,   /* LCD and touch ST7123 at 0x55, firmware 3 */
  M5_TAB5_PANEL_ST7121,   /* LCD ST7121, touch at 0x55, firmware 1 */
} m5_tab5_panel_t;

typedef struct {
  m5_tab5_panel_t kind;
  esp_lcd_dsi_bus_handle_t bus;
  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_handle_t panel;
} m5_tab5_display_t;

/* Resets the touch controller through expander 0x43 (P4 LCD reset, P5 touch
 * reset) as the factory HAL does before it starts the display. */
esp_err_t m5_tab5_touch_reset(void);
/* Probes the touch controller (GT911 at 0x14, then ST712x at 0x55 and its
 * firmware byte) and caches the result. */
m5_tab5_panel_t m5_tab5_panel(void);
/* The factory names: "ILI9881C", "ST7123", "ST7121" or "Unknown". */
const char *m5_tab5_panel_name(m5_tab5_panel_t kind);
/* LDO3 at 2.5 V for the DSI PHY, a 2-lane DSI bus and an RGB565 DPI panel
 * with fb_count (1..3) PSRAM frame buffers. The DSI interrupts are allocated
 * on the calling core. */
esp_err_t m5_tab5_display_new(uint32_t fb_count, m5_tab5_display_t *out);
void m5_tab5_display_del(m5_tab5_display_t *display);
/* LEDC (12-bit, 5 kHz) on GPIO22; values above 100 clamp. */
esp_err_t m5_tab5_backlight_set(uint32_t percent);

/* Touch controller matching m5_tab5_panel() in panel coordinates
 * (0..719 x 0..1279). The caller owns the handle and may set swap/mirror. */
esp_err_t m5_tab5_touch_new(esp_lcd_touch_handle_t *out);

/* ---- storage -------------------------------------------------------------
 */

/* 4-bit SDMMC slot 0 with IO power from on-chip LDO channel 4, FAT mounted at
 * mount_point. The card is not formatted on a mount failure. */
esp_err_t m5_tab5_sdcard_mount(const char *mount_point, size_t max_files,
                               sdmmc_card_t **out);
esp_err_t m5_tab5_sdcard_unmount(const char *mount_point, sdmmc_card_t *card);

/* ---- audio ---------------------------------------------------------------
 */

/* I2S1 at 48 kHz: ES8388 DAC on a 16-bit stereo standard TX channel, ES7210
 * on a 4-slot TDM RX channel whose frames are [MIC-L, AEC, MIC-R, MIC-HP]. */
esp_err_t m5_tab5_audio_init(void);
/* Writes interleaved 16-bit stereo samples at 48 kHz; blocks until queued. */
esp_err_t m5_tab5_audio_write(const int16_t *samples, size_t bytes);
/* Reads 4-channel 16-bit frames at 48 kHz; blocks until filled. */
esp_err_t m5_tab5_audio_read(int16_t *frames, size_t bytes);
/* 0 mutes the DAC; 1..100 sets the ES8388 output volume. */
esp_err_t m5_tab5_audio_set_volume(int percent);
esp_err_t m5_tab5_audio_set_mute(bool mute);
esp_err_t m5_tab5_audio_set_in_gain(float db);

/* ---- sensors ---------------------------------------------------------------
 */

/* RX8130CE at 0x32. init enables the backup battery switch and clears the
 * interrupt flags. Times use the factory convention: tm_mon 0..11 stored as
 * is, tm_year years since 1900 stored modulo 100. */
esp_err_t m5_tab5_rtc_init(void);
esp_err_t m5_tab5_rtc_get(struct tm *out);
esp_err_t m5_tab5_rtc_set(const struct tm *time);
esp_err_t m5_tab5_rtc_clear_irq(void);
/* Alarm interrupt on the next minute match (the factory's RTC wakeup). */
esp_err_t m5_tab5_rtc_set_alarm(const struct tm *time);

/* INA226 at 0x41 on the battery path: 5 mOhm shunt, 8.192 A range, 16
 * averages of 1.1 ms conversions. The current is positive while charging. */
typedef struct {
  float bus_voltage;   /* V */
  float shunt_voltage; /* V */
  float current;       /* A */
  float power;         /* W */
} m5_tab5_power_reading_t;
esp_err_t m5_tab5_power_monitor_init(void);
esp_err_t m5_tab5_power_monitor_read(m5_tab5_power_reading_t *out);

/* BMI270 at 0x68: accelerometer +-4 g and gyroscope +-1000 dps at 200 Hz.
 * Axes follow the factory HAL: x and y in the landscape screen plane,
 * acceleration in g and rotation in units of 10 dps. */
typedef struct {
  float accel_x, accel_y, accel_z;
  float gyro_x, gyro_y, gyro_z;
} m5_tab5_imu_reading_t;
esp_err_t m5_tab5_imu_init(void);
esp_err_t m5_tab5_imu_read(m5_tab5_imu_reading_t *out);
/* Clears a latched BMI270 interrupt left from a previous wakeup. */
esp_err_t m5_tab5_imu_clear_irq(void);
/* Any-motion interrupt on INT1, the factory's shake wakeup. */
esp_err_t m5_tab5_imu_enable_motion_irq(void);

/* On-chip temperature sensor in degrees Celsius. */
esp_err_t m5_tab5_cpu_temperature(float *celsius);

/* ---- interfaces ------------------------------------------------------------
 */

/* Probes addresses 0x10..0x7F; writes up to `capacity` addresses that ACK and
 * returns their count in *count. */
esp_err_t m5_tab5_i2c_scan(i2c_master_bus_handle_t bus, uint8_t *addresses,
                           size_t capacity, size_t *count);
/* Port A bus (GPIO53 SDA, GPIO54 SCL) with internal pull-ups. */
esp_err_t m5_tab5_port_a_i2c_init(i2c_master_bus_handle_t *out);
esp_err_t m5_tab5_port_a_i2c_deinit(void);

/* RS485 half duplex on UART1 (TX 20, RX 21, DE 34), 8N1. */
esp_err_t m5_tab5_rs485_init(uint32_t baud);
esp_err_t m5_tab5_rs485_write(const uint8_t *data, size_t length);
/* Returns at most `capacity` received bytes in *received; waits up to
 * timeout_ms for the first. */
esp_err_t m5_tab5_rs485_read(uint8_t *data, size_t capacity, size_t *received,
                             uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
