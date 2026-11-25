/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* M5Stack CoreP4X (C157, ESP32-P4) board support, taken from the board code
 * of M5CoreP4X-UserDemo. The component has no PocketJS dependency: it powers
 * the board up through the M5PM1 PMIC and the M5IOE1 expander and returns
 * esp_lcd / esp_lcd_touch handles and small driver APIs. */
#pragma once
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Panel geometry in its scan order. The panel is mounted upside down: the
 * factory firmware turns its picture 180 degrees (LV_DISPLAY_ROTATION_180). */
#define M5_COREP4X_LCD_WIDTH 480u
#define M5_COREP4X_LCD_HEIGHT 480u

/* Internal I2C: M5PM1, M5IOE1, CST3530, ES8311, ES7210, LSM6DS3TR-C,
 * RX8130CE. */
#define M5_COREP4X_I2C_PORT I2C_NUM_0
#define M5_COREP4X_I2C_SDA GPIO_NUM_11
#define M5_COREP4X_I2C_SCL GPIO_NUM_9
/* Port A (HY2.0-4P, the factory demo's external I2C). */
#define M5_COREP4X_PORT_A_I2C_PORT I2C_NUM_1
#define M5_COREP4X_PORT_A_SDA GPIO_NUM_18
#define M5_COREP4X_PORT_A_SCL GPIO_NUM_16

#define M5_COREP4X_PMIC_IRQ GPIO_NUM_0
#define M5_COREP4X_TOUCH_INT GPIO_NUM_1
#define M5_COREP4X_I2S_MCLK GPIO_NUM_2
#define M5_COREP4X_I2S_DOUT GPIO_NUM_3 /* to ES8311 DSDIN */
#define M5_COREP4X_I2S_WS GPIO_NUM_4
#define M5_COREP4X_I2S_DIN GPIO_NUM_5 /* from ES7210 SDOUT1 (TDM) */
#define M5_COREP4X_I2S_BCLK GPIO_NUM_6
#define M5_COREP4X_SD_MOSI GPIO_NUM_7
#define M5_COREP4X_SD_MISO GPIO_NUM_8
#define M5_COREP4X_SD_CLK GPIO_NUM_10
#define M5_COREP4X_SD_CS GPIO_NUM_50
#define M5_COREP4X_IR_RX GPIO_NUM_13
#define M5_COREP4X_IR_TX GPIO_NUM_49
#define M5_COREP4X_ETH_RESET GPIO_NUM_52
/* The ESP32-C6 socket (A172 Stamp-AddOn C6, not driven by this BSP). */
#define M5_COREP4X_C6_WAKE GPIO_NUM_40
#define M5_COREP4X_C6_RESET GPIO_NUM_42
#define M5_COREP4X_C6_SDIO_FIRST GPIO_NUM_43
#define M5_COREP4X_C6_SDIO_LAST GPIO_NUM_48

/* M5IOE1 pins, zero-based (M5IOE1_PIN_n is n - 1). */
#define M5_COREP4X_IOE1_AU_PWR_EN 0     /* IO1: audio codecs */
#define M5_COREP4X_IOE1_USBA_OUT_EN 1   /* IO2: USB-A 5 V */
#define M5_COREP4X_IOE1_SPK_PWR_EN 2    /* IO3: AW8737A amplifier */
#define M5_COREP4X_IOE1_P4USBC_OUT_EN 3 /* IO4: USB-C OTG 5 V */
#define M5_COREP4X_IOE1_GROVE_PWR_EN 4  /* IO5: Port A 5 V */
#define M5_COREP4X_IOE1_CHG_STAT 5      /* IO6: charger status, low charging */
#define M5_COREP4X_IOE1_TP_RST 7        /* IO8: touch reset, open drain */
#define M5_COREP4X_IOE1_LCD_BL_PWM 8    /* IO9: backlight, PWM channel 0 */
#define M5_COREP4X_IOE1_LCD_PWR_EN 9    /* IO10: LCD power */
#define M5_COREP4X_IOE1_LCD_RST 10      /* IO11: LCD reset, open drain */
#define M5_COREP4X_IOE1_L3B_EN 11       /* IO12: L3B rail (Ethernet, IR, SD) */
#define M5_COREP4X_IOE1_SD_DET 12       /* IO13: microSD detect, low inserted */

/* M5PM1 pins. */
#define M5_COREP4X_PM1_IMU_INT 0  /* PYG0: LSM6DS3TR-C interrupt */
#define M5_COREP4X_PM1_CHG_PROG 1 /* PYG1: charge current, low 650 mA */
#define M5_COREP4X_PM1_IRQ 2      /* PYG2: interrupt to GPIO0 */
#define M5_COREP4X_PM1_RTC_INT 4  /* PYG4: RX8130CE interrupt */

/* ---- board ---------------------------------------------------------------
 */

/* Power switches behind the M5IOE1 and the M5PM1. */
typedef enum {
  M5_COREP4X_SWITCH_L3B,      /* IOE1 IO12: Ethernet PHY, IR, microSD rail */
  M5_COREP4X_SWITCH_AUDIO,    /* IOE1 IO1: ES8311 and ES7210 */
  M5_COREP4X_SWITCH_SPEAKER,  /* IOE1 IO3: AW8737A amplifier */
  M5_COREP4X_SWITCH_LCD,      /* IOE1 IO10: LCD power */
  M5_COREP4X_SWITCH_GROVE,    /* IOE1 IO5: Port A 5 V */
  M5_COREP4X_SWITCH_USB_A,    /* IOE1 IO2: USB-A host 5 V */
  M5_COREP4X_SWITCH_USB_C,    /* IOE1 IO4: USB-C OTG 5 V */
  M5_COREP4X_SWITCH_M5BUS_5V, /* PM1 PWR_CFG BOOST_EN: M5-Bus 5 V out */
  M5_COREP4X_SWITCH_COUNT,
} m5_corep4x_switch_t;

typedef enum {
  M5_COREP4X_CHARGE_OFF,
  M5_COREP4X_CHARGE_180MA, /* PM1 PYG1 high impedance */
  M5_COREP4X_CHARGE_650MA, /* PM1 PYG1 push-pull low */
} m5_corep4x_charge_t;

/* Creates the internal I2C bus and starts the expander and the PMIC as the
 * factory m5_corep4_tool::init and app_main do: touch and LCD reset lines
 * released (open drain), L3B on, PM1 watchdog off and its LED off, audio,
 * LCD and Port A power on, USB-A and USB-C 5 V cycled off and on (200 ms),
 * charging at 180 mA. Later calls return ESP_OK without repeating it. */
esp_err_t m5_corep4x_init(void);
i2c_master_bus_handle_t m5_corep4x_i2c(void);
esp_err_t m5_corep4x_set_switch(m5_corep4x_switch_t which, bool on);
/* The level the program last wrote, read back from the device. */
esp_err_t m5_corep4x_get_switch(m5_corep4x_switch_t which, bool *on);
esp_err_t m5_corep4x_set_charge(m5_corep4x_charge_t current);
m5_corep4x_charge_t m5_corep4x_charge(void);

/* The microSD slot's detect switch (IOE1 IO13, low = card inserted). */
esp_err_t m5_corep4x_sd_detected(bool *inserted);
/* The detect input with its pull-up (on), or without it before sleep. */
esp_err_t m5_corep4x_sd_detect_enable(bool on);

typedef struct {
  uint16_t battery_mv; /* PM1 VBAT */
  uint16_t vin_mv;     /* PM1 VIN (USB-C) */
  uint16_t bus_mv;     /* PM1 5VINOUT (M5-Bus 5 V) */
  uint8_t source;      /* PM1 PWR_SRC: 0 5VIN, 1 5VINOUT, 2 battery */
  bool charging;       /* IOE1 IO6 low */
} m5_corep4x_power_t;
esp_err_t m5_corep4x_power_read(m5_corep4x_power_t *out);
/* Releases the board through the PMIC (SYS_CMD shutdown). */
esp_err_t m5_corep4x_power_off(void);
esp_err_t m5_corep4x_reboot(void);

/* ---- PMIC events and deep sleep ------------------------------------------
 */

bool m5_corep4x_pmic_ready(void);     /* the M5PM1 answered at init */
bool m5_corep4x_expander_ready(void); /* the M5IOE1 answered at init */

/* PM1 WAKE_SRC, read and cleared: bit 0 timer, 1 VIN, 2 power button, 3
 * reset button, 4 command reset, 5 external GPIO, 6 5VINOUT. */
esp_err_t m5_corep4x_wake_source(uint8_t *source);

/* PM1 button events as p4_pm1_irq_init sets them up: single-click reset
 * off, PM1 GPIO2 as the IRQ output to GPIO0 (falling edge; on_irq runs in
 * the ISR), GPIO IRQs masked but GPIO0 (IMU), system IRQs masked. After a
 * deep-sleep wake it clears the button IRQ of the wake press, waits 500 ms
 * and turns the PM1 LED off. */
esp_err_t m5_corep4x_pmic_irq_start(void (*on_irq)(void *context),
                                    void *context);
#define M5_COREP4X_BUTTON_CLICK (1u << 0)
#define M5_COREP4X_BUTTON_WAKE (1u << 1)
#define M5_COREP4X_BUTTON_DOUBLE (1u << 2)
/* Reads and clears the PM1 button (M5_COREP4X_BUTTON_*) and GPIO IRQ
 * status. */
esp_err_t m5_corep4x_pmic_irq_take(uint8_t *button, uint8_t *gpio);

/* Reset, input, no pulls. */
void m5_corep4x_gpio_high_z(gpio_num_t pin);
/* Releases the reset-line holds of m5_corep4x_deep_sleep (done by
 * m5_corep4x_init after a deep-sleep wake). */
void m5_corep4x_release_sleep_holds(void);

/* The last steps of the factory sleep_before_work: the ESP32-C6 socket
 * released, rails off (amplifier, codecs, Port A, USB-C, USB-A, L3B,
 * M5-Bus 5 V), 100 ms, then the PM1 and expander I2C sleep after 3 s. The
 * LCD rail is the display's (m5_corep4x_display_del). */
esp_err_t m5_corep4x_sleep_rails_off(void);

typedef struct {
  bool rtc;   /* RX8130 timer on PM1 GPIO4 (set by the RTC driver) */
  bool touch; /* CST3530 interrupt on GPIO1 */
  bool imu;   /* LSM6DS3TR-C wake-up on PM1 GPIO0 */
} m5_corep4x_wake_t;
/* enter_deep_sleep: EXT1 any-low on GPIO0 (and GPIO1 with touch), PM1 wake
 * pins, PM1 timer cleared, IRQ masks (buttons always unmasked), flags
 * cleared, I2C pins and GPIO1 released, PHY and C6 resets held low. Returns
 * only on failure; the wake is a reset. */
esp_err_t m5_corep4x_deep_sleep(const m5_corep4x_wake_t *wake);

/* ---- display -------------------------------------------------------------
 */

typedef struct {
  esp_lcd_dsi_bus_handle_t bus;
  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_handle_t panel;
} m5_corep4x_display_t;

/* LCD power on, the reset pulse (100 ms low, 120 ms settle), LDO3 at
 * 2.5 V for the DSI PHY, a 2-lane 700 Mbps DSI bus and the ST7102 as an
 * RGB565 DPI panel at 24 MHz with fb_count (1..3) PSRAM frame buffers. The
 * DSI interrupts are allocated on the calling core. */
esp_err_t m5_corep4x_display_new(uint32_t fb_count, m5_corep4x_display_t *out);
/* Panel off and deleted with its DBI IO and DSI bus, LCD power off, the
 * DSI PHY LDO released. */
void m5_corep4x_display_del(m5_corep4x_display_t *display);
/* Refreshes the panel has completed since m5_corep4x_display_new: the DSI
 * host reads one framebuffer per refresh, 61.7 times a second, and counts
 * when it has read the last row. The count wraps. */
uint32_t m5_corep4x_display_refreshes(void);
/* Blocks until the count has passed `after`, at most timeout_ms. False on a
 * timeout and without a display. One task waits at a time. */
bool m5_corep4x_display_wait_refresh(uint32_t after, uint32_t timeout_ms);
/* A display with two or three frame buffers: the panel reads `framebuffer`
 * (one of esp_lcd_dpi_panel_get_frame_buffer) from its next refresh on. The
 * call does not wait for it. `after` receives a refresh count: once the
 * count has passed it (m5_corep4x_display_wait_refresh) the panel no longer
 * reads the frame buffer it read before. The caller has written the pixels
 * to memory: pixels written through the CPU cache need esp_cache_msync
 * first. A call from the core m5_corep4x_display_new ran on returns the
 * next refresh; a call from the other core the one after it. */
esp_err_t m5_corep4x_display_show(m5_corep4x_display_t *display,
                                  const void *framebuffer, uint32_t *after);
/* IOE1 PWM channel 0 at 50 kHz on IO9; 0 turns the channel off. Values
 * above 100 clamp. */
esp_err_t m5_corep4x_backlight_set(uint32_t percent);

/* Pulses the touch reset (IOE1 IO8, 100 ms low, 200 ms to boot) and creates
 * the CST3530 at
 * 0x58 with its interrupt on GPIO1, up to 5 contacts, coordinates
 * 0..479. The factory mirrors both axes in the touch driver and turns the
 * LVGL picture 180 degrees, which turns the contacts back: the coordinates
 * the controller reports are the coordinates of the upright picture. */
esp_err_t m5_corep4x_touch_new(esp_lcd_touch_handle_t *out);
esp_err_t m5_corep4x_touch_sleep(esp_lcd_touch_handle_t touch);
esp_err_t m5_corep4x_touch_wake(esp_lcd_touch_handle_t touch);
/* The CST3530 information block (register 0xD0030000). */
typedef struct {
  uint32_t chip_id;  /* 0xCACA220E on the CoreP4X */
  uint32_t firmware; /* major bits 31-24, minor 23-16, patch 15-0 */
  uint16_t width;
  uint16_t height;
} m5_corep4x_touch_info_t;
esp_err_t m5_corep4x_touch_info(esp_lcd_touch_handle_t touch,
                                m5_corep4x_touch_info_t *out);
/* Deep sleep without touch wake: the sleep command, the handle and its IO
 * deleted, the reset held low (open drain), GPIO1 released. */
esp_err_t m5_corep4x_touch_power_down(esp_lcd_touch_handle_t touch);

#ifdef __cplusplus
}
#endif
