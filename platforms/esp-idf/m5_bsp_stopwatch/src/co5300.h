/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2020 lovyan03 (https://github.com/lovyan03)
 *
 * SPDX-License-Identifier: MIT AND BSD-2-Clause-Views
 */
/*
 * CO5300 AMOLED controller as an esp_lcd_panel_t over a quad-mode
 * esp_lcd_panel_io_spi handle (lcd_cmd_bits 32, lcd_param_bits 8).
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

/* Controller memory size. */
#define M5_STOPWATCH_CO5300_MEMORY_SIZE 480

/* Builds the 32-bit command word for a register write (single line). */
#define M5_STOPWATCH_CO5300_CMD(cmd) ((0x02 << 24) | ((cmd) << 8))
/* Builds the 32-bit command word for a quad pixel write of RAMWR. */
#define M5_STOPWATCH_CO5300_PIXELS ((0x32 << 24) | (0x2C << 8))

/*
 * Creates the panel. esp_lcd_panel_init sends the factory init list and
 * COLMOD 0x55 (RGB565); draw_bitmap sends RGB565 pixels as stored (the panel
 * reads the high byte first) and rejects an odd x_start or an odd width with
 * ESP_ERR_INVALID_ARG. mirror and swap_xy are not implemented.
 */
esp_err_t m5_stopwatch_co5300_new(esp_lcd_panel_io_handle_t io,
                                  esp_lcd_panel_handle_t *out);

/* Fills the whole 480x480 controller memory with black, `rows` rows per
 * transfer, using a DMA buffer allocated for the duration of the call. */
esp_err_t m5_stopwatch_co5300_clear_memory(esp_lcd_panel_handle_t panel,
                                           int rows);
