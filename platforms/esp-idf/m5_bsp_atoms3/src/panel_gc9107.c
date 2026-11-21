/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: lovyan03 (LovyanGFX, as shipped in M5GFX)
 *
 * SPDX-License-Identifier: Apache-2.0 AND BSD-2-Clause
 *
 * esp_lcd panel driver for the GC9107 of the M5Stack AtomS3. The esp_lcd
 * panel structure follows ESP-IDF components/esp_lcd/src/
 * esp_lcd_panel_st7789.c; the vendor init sequence is Panel_GC9107 from
 * M5GFX 0.2.19 src/lgfx/v1/panel/Panel_GC9A01.hpp (LovyanGFX, FreeBSD
 * licence).
 */
#include "panel_gc9107.h"

#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <sys/cdefs.h>

static const char *TAG = "m5_atoms3_gc9107";

// Command, argument count (bit 7: a delay in ms follows the arguments),
// arguments. Panel_GC9107::getInitCommands list 0.
#define INIT_DELAY 0x80
// clang-format off
static const uint8_t gc9107_init[] = {
    0xFE, 0 | INIT_DELAY, 5,
    0xEF, 0 | INIT_DELAY, 5,
    0xB0, 1, 0xC0,
    0xB2, 1, 0x2F,
    0xB3, 1, 0x03,
    0xB6, 1, 0x19,
    0xB7, 1, 0x01,
    0xAC, 1, 0xCB,
    0xAB, 1, 0x0E,
    0xB4, 1, 0x04,
    0xA8, 1, 0x19,
    0xB8, 1, 0x08,
    0xE8, 1, 0x24,
    0xE9, 1, 0x48,
    0xEA, 1, 0x22,
    0xC6, 1, 0x30,
    0xC7, 1, 0x18,
    0xF0, 14, 0x01, 0x2b, 0x23, 0x3c, 0xb7, 0x12, 0x17, 0x60, 0x00, 0x06, 0x0c, 0x17, 0x12, 0x1f,
    0xF1, 14, 0x05, 0x2e, 0x2d, 0x44, 0xd6, 0x15, 0x17, 0xa0, 0x02, 0x0d, 0x0d, 0x1a, 0x18, 0x1f,
    0x11, 0 | INIT_DELAY, 120,
    0x29, 0,
};
// clang-format on

typedef struct {
  esp_lcd_panel_t base;
  esp_lcd_panel_io_handle_t io;
  gpio_num_t reset_gpio_num;
  int x_gap;
  int y_gap;
  uint8_t madctl_val;
} gc9107_panel_t;

static esp_err_t tx(esp_lcd_panel_io_handle_t io, int cmd, const void *data,
                    size_t size) {
  return esp_lcd_panel_io_tx_param(io, cmd, data, size);
}

static esp_err_t panel_del(esp_lcd_panel_t *panel) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  if (gc->reset_gpio_num >= 0) {
    gpio_reset_pin(gc->reset_gpio_num);
  }
  free(gc);
  return ESP_OK;
}

static esp_err_t panel_reset(esp_lcd_panel_t *panel) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  if (gc->reset_gpio_num >= 0) {
    // M5GFX _pin_reset and Panel_Device::init: low, then high for 64 ms.
    gpio_set_level(gc->reset_gpio_num, 0);
    vTaskDelay(pdMS_TO_TICKS(8));
    gpio_set_level(gc->reset_gpio_num, 1);
    vTaskDelay(pdMS_TO_TICKS(64));
  } else {
    ESP_RETURN_ON_ERROR(tx(gc->io, LCD_CMD_SWRESET, NULL, 0), TAG, "swreset");
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_t *panel) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  size_t i = 0;
  while (i < sizeof(gc9107_init)) {
    uint8_t cmd = gc9107_init[i++];
    uint8_t count = gc9107_init[i++];
    uint8_t delay = count & INIT_DELAY;
    count &= (uint8_t)~INIT_DELAY;
    ESP_RETURN_ON_ERROR(tx(gc->io, cmd, count ? &gc9107_init[i] : NULL, count),
                        TAG, "init 0x%02x", cmd);
    i += count;
    if (delay) {
      vTaskDelay(pdMS_TO_TICKS(gc9107_init[i++]));
    }
  }
  // Panel_LCD::setRotation(0) and setColorDepth(16): MADCTL with BGR order,
  // COLMOD RGB565, no inversion.
  ESP_RETURN_ON_ERROR(tx(gc->io, LCD_CMD_COLMOD, (uint8_t[]){0x55}, 1), TAG,
                      "colmod");
  ESP_RETURN_ON_ERROR(tx(gc->io, LCD_CMD_MADCTL, &gc->madctl_val, 1), TAG,
                      "madctl");
  ESP_RETURN_ON_ERROR(tx(gc->io, LCD_CMD_INVOFF, NULL, 0), TAG, "invoff");
  return ESP_OK;
}

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start,
                                   int y_start, int x_end, int y_end,
                                   const void *color_data) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  x_start += gc->x_gap;
  x_end += gc->x_gap;
  y_start += gc->y_gap;
  y_end += gc->y_gap;
  const uint8_t caset[] = {(x_start >> 8) & 0xFF, x_start & 0xFF,
                           ((x_end - 1) >> 8) & 0xFF, (x_end - 1) & 0xFF};
  const uint8_t raset[] = {(y_start >> 8) & 0xFF, y_start & 0xFF,
                           ((y_end - 1) >> 8) & 0xFF, (y_end - 1) & 0xFF};
  ESP_RETURN_ON_ERROR(tx(gc->io, LCD_CMD_CASET, caset, sizeof(caset)), TAG,
                      "caset");
  ESP_RETURN_ON_ERROR(tx(gc->io, LCD_CMD_RASET, raset, sizeof(raset)), TAG,
                      "raset");
  size_t len = (size_t)(x_end - x_start) * (size_t)(y_end - y_start) * 2;
  return esp_lcd_panel_io_tx_color(gc->io, LCD_CMD_RAMWR, color_data, len);
}

static esp_err_t panel_invert_color(esp_lcd_panel_t *panel, bool invert) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  return tx(gc->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
}

static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool mirror_x,
                              bool mirror_y) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  gc->madctl_val &= (uint8_t)~(LCD_CMD_MX_BIT | LCD_CMD_MY_BIT);
  gc->madctl_val |=
      (mirror_x ? LCD_CMD_MX_BIT : 0) | (mirror_y ? LCD_CMD_MY_BIT : 0);
  return tx(gc->io, LCD_CMD_MADCTL, &gc->madctl_val, 1);
}

static esp_err_t panel_swap_xy(esp_lcd_panel_t *panel, bool swap_axes) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  gc->madctl_val &= (uint8_t)~LCD_CMD_MV_BIT;
  gc->madctl_val |= swap_axes ? LCD_CMD_MV_BIT : 0;
  return tx(gc->io, LCD_CMD_MADCTL, &gc->madctl_val, 1);
}

static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  gc->x_gap = x_gap;
  gc->y_gap = y_gap;
  return ESP_OK;
}

static esp_err_t panel_disp_on_off(esp_lcd_panel_t *panel, bool on) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  return tx(gc->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
}

static esp_err_t panel_sleep(esp_lcd_panel_t *panel, bool sleep) {
  gc9107_panel_t *gc = __containerof(panel, gc9107_panel_t, base);
  ESP_RETURN_ON_ERROR(
      tx(gc->io, sleep ? LCD_CMD_SLPIN : LCD_CMD_SLPOUT, NULL, 0), TAG,
      "sleep");
  vTaskDelay(pdMS_TO_TICKS(120));
  return ESP_OK;
}

esp_err_t m5_atoms3_new_panel_gc9107(esp_lcd_panel_io_handle_t io,
                                     gpio_num_t reset_gpio_num,
                                     esp_lcd_panel_handle_t *ret_panel) {
  ESP_RETURN_ON_FALSE(io && ret_panel, ESP_ERR_INVALID_ARG, TAG,
                      "invalid argument");
  if (reset_gpio_num >= 0) {
    const gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << reset_gpio_num,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "reset gpio");
    gpio_set_level(reset_gpio_num, 1);
  }
  gc9107_panel_t *gc = calloc(1, sizeof(*gc));
  ESP_RETURN_ON_FALSE(gc, ESP_ERR_NO_MEM, TAG, "no memory for panel");
  gc->io = io;
  gc->reset_gpio_num = reset_gpio_num;
  gc->madctl_val = LCD_CMD_BGR_BIT;
  gc->base.del = panel_del;
  gc->base.reset = panel_reset;
  gc->base.init = panel_init;
  gc->base.draw_bitmap = panel_draw_bitmap;
  gc->base.invert_color = panel_invert_color;
  gc->base.set_gap = panel_set_gap;
  gc->base.mirror = panel_mirror;
  gc->base.swap_xy = panel_swap_xy;
  gc->base.disp_on_off = panel_disp_on_off;
  gc->base.disp_sleep = panel_sleep;
  *ret_panel = &gc->base;
  return ESP_OK;
}
