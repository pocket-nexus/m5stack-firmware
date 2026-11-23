/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2020 lovyan03 (https://github.com/lovyan03)
 *
 * SPDX-License-Identifier: MIT AND BSD-2-Clause-Views
 */
/*
 * CO5300 panel driver. The init list is Panel_CO5300 from the factory
 * firmware (M5StopWatch-UserDemo main/hal/hal_display.cpp, MIT). The QSPI
 * framing follows Panel_AMOLED (LovyanGFX, distributed in M5GFX 0.2.19 as
 * src/lgfx/v1/panel/Panel_AMOLED.cpp, FreeBSD license): a register write is
 * the bytes 0x02 0x00 cmd 0x00 followed by the parameters on one data line;
 * a pixel write is 0x32 0x00 0x2C 0x00 followed by pixel data on four lines;
 * the window start column and the window width must be even.
 */
#include "co5300.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <sys/cdefs.h>

static const char *TAG = "m5sw_co5300";

/*
 * Memory column of frame column 0, as in the factory firmware
 * (M5StopWatch-UserDemo main/hal/hal_display.cpp lines 105-107:
 * panel_width 468, offset_x 6) and M5GFX (esp32s3_board_data.hpp,
 * stopwatch panel_co5300 offset_x 6). Frame column x lands on the same
 * memory column as the factory's LVGL column x; the factory's columns 466
 * and 467 stay black.
 */
#define CO5300_COLUMN_OFFSET 6
#define CO5300_ROW_OFFSET 0

#define CMD_NOP 0x00
#define CMD_SWRESET 0x01
#define CMD_SLPIN 0x10
#define CMD_SLPOUT 0x11
#define CMD_INVOFF 0x20
#define CMD_INVON 0x21
#define CMD_DISPOFF 0x28
#define CMD_DISPON 0x29
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_COLMOD 0x3A
#define CMD_WRDISBV 0x51

typedef struct {
  uint8_t cmd;
  uint8_t len;
  uint8_t data[2];
  uint16_t delay_ms;
} co5300_init_cmd_t;

/* Panel_CO5300::getInitCommands list 0 (factory hal_display.cpp). */
// clang-format off
static const co5300_init_cmd_t s_init_cmds[] = {
    {0x11, 0, {0x00, 0x00}, 150}, /* Sleep out */
    {0xC4, 1, {0x80, 0x00}, 0},
    {0x35, 1, {0x80, 0x00}, 0},   /* Tearing effect on */
    {0x44, 2, {0x01, 0xD2}, 0},   /* Tear scanline 0x1D2 = 466 */
    {0x53, 1, {0x20, 0x00}, 0},   /* Brightness control on */
    {0x20, 0, {0x00, 0x00}, 0},   /* Inversion off */
    {0x36, 1, {0x00, 0x00}, 0},   /* MADCTL */
    {0x51, 1, {0xA0, 0x00}, 0},   /* Brightness */
    {0x29, 0, {0x00, 0x00}, 0},   /* Display on */
};
// clang-format on

typedef struct {
  esp_lcd_panel_t base;
  esp_lcd_panel_io_handle_t io;
  /* One task at a time uses the panel IO: the display driver draws from its
   * own task while the program sets the brightness from another, and the
   * IO's parameter transfer collects the results of queued pixel transfers. */
  SemaphoreHandle_t lock;
  int x_gap;
  int y_gap;
} co5300_panel_t;

static esp_err_t tx_cmd(esp_lcd_panel_io_handle_t io, uint8_t cmd,
                        const void *data, size_t len) {
  return esp_lcd_panel_io_tx_param(io, M5_STOPWATCH_CO5300_CMD(cmd), data, len);
}

static esp_err_t set_range(esp_lcd_panel_io_handle_t io, uint8_t cmd, int start,
                           int end_inclusive) {
  const uint8_t data[4] = {
      (uint8_t)(start >> 8),
      (uint8_t)(start & 0xFF),
      (uint8_t)(end_inclusive >> 8),
      (uint8_t)(end_inclusive & 0xFF),
  };
  return tx_cmd(io, cmd, data, sizeof(data));
}

static esp_err_t panel_reset(esp_lcd_panel_t *panel) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  ESP_RETURN_ON_ERROR(tx_cmd(p->io, CMD_SWRESET, NULL, 0), TAG, "SWRESET");
  vTaskDelay(pdMS_TO_TICKS(120));
  return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_t *panel) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  for (size_t i = 0; i < sizeof(s_init_cmds) / sizeof(s_init_cmds[0]); i++) {
    const co5300_init_cmd_t *c = &s_init_cmds[i];
    ESP_RETURN_ON_ERROR(tx_cmd(p->io, c->cmd, c->len ? c->data : NULL, c->len),
                        TAG, "init cmd 0x%02X", c->cmd);
    if (c->delay_ms) {
      vTaskDelay(pdMS_TO_TICKS(c->delay_ms));
    }
  }
  /* Panel_AMOLED::setColorDepth(rgb565_2Byte). */
  const uint8_t colmod = 0x55;
  return tx_cmd(p->io, CMD_COLMOD, &colmod, 1);
}

static esp_err_t panel_del(esp_lcd_panel_t *panel) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  vSemaphoreDelete(p->lock);
  free(p);
  return ESP_OK;
}

static esp_err_t panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start,
                                   int y_start, int x_end, int y_end,
                                   const void *color_data) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  ESP_RETURN_ON_FALSE(x_start >= 0 && y_start >= 0 && x_end > x_start &&
                          y_end > y_start && color_data,
                      ESP_ERR_INVALID_ARG, TAG, "invalid window");
  /* Panel_AMOLED restriction: x and width must be divisible by 2. */
  ESP_RETURN_ON_FALSE(((x_start | (x_end - x_start)) & 1) == 0,
                      ESP_ERR_INVALID_ARG, TAG, "odd x %d or width %d", x_start,
                      x_end - x_start);
  const int xs = x_start + p->x_gap + CO5300_COLUMN_OFFSET;
  const int xe = x_end - 1 + p->x_gap + CO5300_COLUMN_OFFSET;
  const int ys = y_start + p->y_gap + CO5300_ROW_OFFSET;
  const int ye = y_end - 1 + p->y_gap + CO5300_ROW_OFFSET;
  ESP_RETURN_ON_FALSE(xe < M5_STOPWATCH_CO5300_MEMORY_SIZE &&
                          ye < M5_STOPWATCH_CO5300_MEMORY_SIZE,
                      ESP_ERR_INVALID_ARG, TAG, "window outside memory");

  const size_t bytes =
      (size_t)(x_end - x_start) * (size_t)(y_end - y_start) * 2;
  xSemaphoreTake(p->lock, portMAX_DELAY);
  esp_err_t err = set_range(p->io, CMD_CASET, xs, xe);
  if (err == ESP_OK)
    err = set_range(p->io, CMD_RASET, ys, ye);
  if (err == ESP_OK)
    err = esp_lcd_panel_io_tx_color(p->io, M5_STOPWATCH_CO5300_PIXELS,
                                    color_data, bytes);
  xSemaphoreGive(p->lock);
  return err;
}

static esp_err_t panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  ESP_RETURN_ON_FALSE((x_gap & 1) == 0, ESP_ERR_INVALID_ARG, TAG, "odd x gap");
  p->x_gap = x_gap;
  p->y_gap = y_gap;
  return ESP_OK;
}

/* A command sent while the panel may be drawn from another task. */
static esp_err_t locked_cmd(co5300_panel_t *p, uint8_t cmd, const void *data,
                            size_t len) {
  xSemaphoreTake(p->lock, portMAX_DELAY);
  const esp_err_t err = tx_cmd(p->io, cmd, data, len);
  xSemaphoreGive(p->lock);
  return err;
}

static esp_err_t panel_invert_color(esp_lcd_panel_t *panel, bool invert) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  return locked_cmd(p, invert ? CMD_INVON : CMD_INVOFF, NULL, 0);
}

static esp_err_t panel_disp_on_off(esp_lcd_panel_t *panel, bool on) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  return locked_cmd(p, on ? CMD_DISPON : CMD_DISPOFF, NULL, 0);
}

static esp_err_t panel_disp_sleep(esp_lcd_panel_t *panel, bool sleep) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  ESP_RETURN_ON_ERROR(locked_cmd(p, sleep ? CMD_SLPIN : CMD_SLPOUT, NULL, 0),
                      TAG, "sleep");
  if (!sleep) {
    vTaskDelay(pdMS_TO_TICKS(150)); /* Panel_AMOLED::setSleep(false) */
  }
  return ESP_OK;
}

static esp_err_t panel_set_brightness(esp_lcd_panel_t *panel, int level) {
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  const uint8_t value = (uint8_t)(level < 0 ? 0 : level > 255 ? 255 : level);
  return locked_cmd(p, CMD_WRDISBV, &value, 1);
}

esp_err_t m5_stopwatch_co5300_new(esp_lcd_panel_io_handle_t io,
                                  esp_lcd_panel_handle_t *out) {
  ESP_RETURN_ON_FALSE(io && out, ESP_ERR_INVALID_ARG, TAG, "arg");
  co5300_panel_t *p = calloc(1, sizeof(*p));
  ESP_RETURN_ON_FALSE(p, ESP_ERR_NO_MEM, TAG, "no memory");
  p->lock = xSemaphoreCreateMutex();
  if (!p->lock) {
    free(p);
    ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, TAG, "no memory");
  }
  p->io = io;
  p->base.reset = panel_reset;
  p->base.init = panel_init;
  p->base.del = panel_del;
  p->base.draw_bitmap = panel_draw_bitmap;
  p->base.set_gap = panel_set_gap;
  p->base.invert_color = panel_invert_color;
  p->base.disp_on_off = panel_disp_on_off;
  p->base.disp_sleep = panel_disp_sleep;
  p->base.set_brightness = panel_set_brightness;
  *out = &p->base;
  return ESP_OK;
}

esp_err_t m5_stopwatch_co5300_clear_memory(esp_lcd_panel_handle_t panel,
                                           int rows) {
  ESP_RETURN_ON_FALSE(panel && rows > 0, ESP_ERR_INVALID_ARG, TAG, "arg");
  co5300_panel_t *p = __containerof(panel, co5300_panel_t, base);
  const int size = M5_STOPWATCH_CO5300_MEMORY_SIZE;
  if (rows > size) {
    rows = size;
  }
  uint8_t *black = heap_caps_calloc((size_t)size * (size_t)rows, 2,
                                    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  ESP_RETURN_ON_FALSE(black, ESP_ERR_NO_MEM, TAG, "no DMA memory");

  esp_err_t err = set_range(p->io, CMD_CASET, 0, size - 1);
  for (int y = 0; err == ESP_OK && y < size; y += rows) {
    const int n = y + rows > size ? size - y : rows;
    /* tx_param waits for the previous strip, so one buffer serves all. */
    err = set_range(p->io, CMD_RASET, y, y + n - 1);
    if (err == ESP_OK) {
      err = esp_lcd_panel_io_tx_color(p->io, M5_STOPWATCH_CO5300_PIXELS, black,
                                      (size_t)size * (size_t)n * 2);
    }
  }
  /* A NOP drains the queued strips before the buffer is freed. */
  esp_err_t drain = tx_cmd(p->io, CMD_NOP, NULL, 0);
  free(black);
  return err != ESP_OK ? err : drain;
}
