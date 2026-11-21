// SPDX-License-Identifier: MIT
//
// StickS3 LCD: ST7789 on SPI, 135x240 portrait, RGB565 big-endian on the wire.
#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_handle_t panel;
  uint32_t width, height;
} m5_sticks3_display_t;

/// Creates the SPI bus, panel IO and ST7789 panel, runs the M5GFX init
/// sequence, applies the GRAM offset and colour inversion and turns the
/// display on. The PMIC LCD rail must be on (m5_sticks3_power_init).
/// `max_transfer_bytes` bounds one color transfer (a strip of rows).
/// The panel accepts RGB565 big-endian; a little-endian renderer swaps bytes.
esp_err_t m5_sticks3_display_init(size_t max_transfer_bytes,
                                  m5_sticks3_display_t *out);

/// Fills the visible area with one RGB565 color, before a renderer owns the
/// panel IO's color transfers.
esp_err_t m5_sticks3_display_clear(const m5_sticks3_display_t *display,
                                   uint16_t rgb565);

/// Configures the LEDC backlight PWM (256 Hz, 9 bit) on GPIO38.
esp_err_t m5_sticks3_backlight_init(void);

/// Sets the backlight brightness 0..255 with M5GFX's Light_PWM curve
/// (offset 16 for StickS3). 0 switches the backlight off.
esp_err_t m5_sticks3_backlight_set(uint8_t brightness);

#ifdef __cplusplus
}
#endif
