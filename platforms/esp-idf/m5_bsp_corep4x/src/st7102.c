/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* esp_lcd_st7102.c of esp_lcd_st7102 1.0.0 (espressif/esp-iot-solution
 * commit d09966201afeab0135aa741e8ad6ed5a1ed09b6a, as vendored by
 * M5CoreP4X-UserDemo): the DPI panel's init, reset, invert and display on/off
 * are replaced; mirror and swap_xy report ESP_ERR_NOT_SUPPORTED, as in
 * M5Stack's copy. The default init sequence and the GPIO reset path are
 * left out: the CoreP4X passes its own sequence and resets the panel through
 * the IO expander. */
#include "st7102.h"

#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "m5p4x_st7102";

typedef struct {
  esp_lcd_panel_io_handle_t io;
  uint8_t madctl;
  uint8_t colmod;
  const m5_corep4x_st7102_cmd_t *init_cmds;
  uint16_t init_cmds_size;
  /* The functions of the DPI panel this driver wraps. */
  esp_err_t (*dpi_del)(esp_lcd_panel_t *panel);
  esp_err_t (*dpi_init)(esp_lcd_panel_t *panel);
} st7102_t;

static esp_err_t panel_del(esp_lcd_panel_t *panel) {
  st7102_t *st7102 = panel->user_data;
  st7102->dpi_del(panel);
  free(st7102);
  return ESP_OK;
}

static esp_err_t panel_init(esp_lcd_panel_t *panel) {
  st7102_t *st7102 = panel->user_data;
  esp_lcd_panel_io_handle_t io = st7102->io;
  ESP_RETURN_ON_ERROR(st7102->dpi_init(panel), TAG, "DPI init");
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &st7102->madctl, 1), TAG,
      "MADCTL");
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, &st7102->colmod, 1), TAG,
      "COLMOD");
  for (uint16_t i = 0; i < st7102->init_cmds_size; i++) {
    const m5_corep4x_st7102_cmd_t *cmd = &st7102->init_cmds[i];
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, cmd->cmd, cmd->data, cmd->data_bytes),
        TAG, "command 0x%02X", cmd->cmd);
    vTaskDelay(pdMS_TO_TICKS(cmd->delay_ms));
  }
  return ESP_OK;
}

static esp_err_t panel_reset(esp_lcd_panel_t *panel) {
  st7102_t *st7102 = panel->user_data;
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_param(st7102->io, LCD_CMD_SWRESET, NULL, 0), TAG,
      "SWRESET");
  vTaskDelay(pdMS_TO_TICKS(120));
  return ESP_OK;
}

static esp_err_t panel_invert(esp_lcd_panel_t *panel, bool invert) {
  st7102_t *st7102 = panel->user_data;
  return esp_lcd_panel_io_tx_param(
      st7102->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
}

static esp_err_t panel_mirror(esp_lcd_panel_t *panel, bool mirror_x,
                              bool mirror_y) {
  (void)panel;
  (void)mirror_x;
  (void)mirror_y;
  return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_swap_xy(esp_lcd_panel_t *panel, bool swap) {
  (void)panel;
  (void)swap;
  return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_on_off(esp_lcd_panel_t *panel, bool on) {
  st7102_t *st7102 = panel->user_data;
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_io_tx_param(st7102->io,
                                on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0),
      TAG, "display on/off");
  if (on) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return ESP_OK;
}

esp_err_t m5_corep4x_st7102_new(esp_lcd_panel_io_handle_t io,
                                const esp_lcd_panel_dev_config_t *device,
                                const m5_corep4x_st7102_config_t *config,
                                esp_lcd_panel_handle_t *out) {
  ESP_RETURN_ON_FALSE(io && device && config && config->dsi_bus &&
                          config->dpi_config && out,
                      ESP_ERR_INVALID_ARG, TAG, "arguments");
  uint8_t colmod = 0;
  switch (device->bits_per_pixel) {
  case 16:
    colmod = 0x55;
    break;
  case 18:
    colmod = 0x66;
    break;
  case 24:
    colmod = 0x77;
    break;
  default:
    return ESP_ERR_NOT_SUPPORTED;
  }
  st7102_t *st7102 = calloc(1, sizeof(*st7102));
  ESP_RETURN_ON_FALSE(st7102, ESP_ERR_NO_MEM, TAG, "panel");
  st7102->io = io;
  st7102->madctl =
      device->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR ? LCD_CMD_BGR_BIT : 0;
  st7102->colmod = colmod;
  st7102->init_cmds = config->init_cmds;
  st7102->init_cmds_size = config->init_cmds_size;
  esp_lcd_panel_handle_t panel = NULL;
  esp_err_t err =
      esp_lcd_new_panel_dpi(config->dsi_bus, config->dpi_config, &panel);
  if (err != ESP_OK) {
    free(st7102);
    return err;
  }
  st7102->dpi_del = panel->del;
  st7102->dpi_init = panel->init;
  panel->del = panel_del;
  panel->init = panel_init;
  panel->reset = panel_reset;
  panel->mirror = panel_mirror;
  panel->swap_xy = panel_swap_xy;
  panel->invert_color = panel_invert;
  panel->disp_on_off = panel_on_off;
  panel->user_data = st7102;
  *out = panel;
  return ESP_OK;
}
