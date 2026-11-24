/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* ST7102 MIPI-DSI panel as an esp_lcd panel, from esp_lcd_st7102 1.0.0
 * (espressif/esp-iot-solution, as vendored by M5CoreP4X-UserDemo). */
#pragma once

#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
  int cmd;
  const void *data;
  size_t data_bytes;
  unsigned int delay_ms;
} m5_corep4x_st7102_cmd_t;

typedef struct {
  const m5_corep4x_st7102_cmd_t *init_cmds;
  uint16_t init_cmds_size;
  esp_lcd_dsi_bus_handle_t dsi_bus;
  const esp_lcd_dpi_panel_config_t *dpi_config;
} m5_corep4x_st7102_config_t;

/* A DPI panel whose init sends MADCTL (RGB order), COLMOD (from
 * bits_per_pixel) and then the vendor commands in video mode. The panel has
 * no reset GPIO: reset sends SWRESET and waits 120 ms. */
esp_err_t m5_corep4x_st7102_new(esp_lcd_panel_io_handle_t io,
                                const esp_lcd_panel_dev_config_t *device,
                                const m5_corep4x_st7102_config_t *config,
                                esp_lcd_panel_handle_t *out);
