// SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
// SPDX-License-Identifier: MIT
#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_types.h"

// GC9107 panel on an SPI panel IO: RGB565, BGR element order, rotation 0.
esp_err_t m5_atoms3_new_panel_gc9107(esp_lcd_panel_io_handle_t io,
                                     gpio_num_t reset_gpio_num,
                                     esp_lcd_panel_handle_t *ret_panel);
