/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Waveshare
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* CST3530 touch controller as an esp_lcd_touch driver, from
 * esp_lcd_touch_cst3xx as vendored by M5CoreP4X-UserDemo. */
#pragma once
#include "m5_corep4x/bsp.h"

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

#define M5_COREP4X_CST3530_ADDR 0x58

/* The panel IO the factory uses: 8-bit commands and parameters, no control
 * phase, 400 kHz. */
esp_err_t m5_corep4x_cst3530_io_new(i2c_master_bus_handle_t bus,
                                    esp_lcd_panel_io_handle_t *out);
/* Leaves the low-power scan, selects normal mode and reads the information
 * block (0xD0030000) into the log. */
esp_err_t m5_corep4x_cst3530_new(esp_lcd_panel_io_handle_t io,
                                 const esp_lcd_touch_config_t *config,
                                 esp_lcd_touch_handle_t *out);
/* Deep sleep (0xD00022AB); the controller leaves it on a hardware reset. */
/* The information block: chip ID, firmware version, resolution. */
esp_err_t m5_corep4x_cst3530_info(esp_lcd_touch_handle_t tp,
                                  m5_corep4x_touch_info_t *out);
esp_err_t m5_corep4x_cst3530_sleep(esp_lcd_touch_handle_t tp);
/* Normal mode after a hardware reset. */
esp_err_t m5_corep4x_cst3530_normal(esp_lcd_touch_handle_t tp);
