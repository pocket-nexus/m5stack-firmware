// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-License-Identifier: MIT
#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Largest color transfer in bytes; the SPI bus DMA descriptors are sized
  // for it. 0 selects one full frame (128 x 128 x 2).
  size_t max_transfer_bytes;
  // SPI transaction queue depth of the panel IO; 0 selects 10.
  size_t trans_queue_depth;
} m5_atoms3_display_config_t;

typedef struct {
  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_handle_t panel;
} m5_atoms3_display_t;

// Brings up SPI3, the panel IO and the GC9107: reset, vendor init, RGB565,
// rotation 0 with the 32-row GRAM offset, display on. The panel expects
// big-endian RGB565 on the wire. No color-transfer callback is registered on
// the IO, so a display driver may install its own. The GRAM is not cleared.
esp_err_t m5_atoms3_display_init(const m5_atoms3_display_config_t *config,
                                 m5_atoms3_display_t *out);
// Deletes the panel and IO and frees the SPI bus.
void m5_atoms3_display_deinit(m5_atoms3_display_t *display);

// LEDC backlight on GPIO16 (M5GFX Light_PWM: 256 Hz, 9 bits, offset 48).
// `brightness` is 0..255; M5GFX starts at 127.
esp_err_t m5_atoms3_backlight_init(uint8_t brightness);
esp_err_t m5_atoms3_backlight_set(uint8_t brightness);

#ifdef __cplusplus
}
#endif
