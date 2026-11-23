// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-FileCopyrightText: lovyan03 (LovyanGFX, as shipped in M5GFX)
// SPDX-License-Identifier: MIT AND BSD-2-Clause
//
// Panel bus setup follows M5GFX 0.2.19 src/M5GFX.cpp (board_M5AtomS3,
// GC9107 branch). The backlight duty formula is Light_PWM::setBrightness
// from src/lgfx/v1/platforms/esp32/Light_PWM.cpp (LovyanGFX, FreeBSD
// licence).
#include "m5_atoms3/display.h"

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_io_spi.h"
#include "m5_atoms3/board.h"
#include "panel_gc9107.h"

static const char *TAG = "m5_atoms3_display";

#define BACKLIGHT_BITS 9
#define BACKLIGHT_TIMER                                                        \
  ((ledc_timer_t)((M5_ATOMS3_BACKLIGHT_LEDC_CHANNEL >> 1) & 3))

esp_err_t m5_atoms3_display_init(const m5_atoms3_display_config_t *config,
                                 m5_atoms3_display_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
  size_t max_transfer = config && config->max_transfer_bytes
                            ? config->max_transfer_bytes
                            : M5_ATOMS3_LCD_WIDTH * M5_ATOMS3_LCD_HEIGHT * 2;
  size_t queue_depth =
      config && config->trans_queue_depth ? config->trans_queue_depth : 10;
  out->io = NULL;
  out->panel = NULL;

  const spi_bus_config_t bus = {
      .mosi_io_num = M5_ATOMS3_LCD_PIN_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = M5_ATOMS3_LCD_PIN_SCLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = (int)max_transfer,
  };
  ESP_RETURN_ON_ERROR(
      spi_bus_initialize(M5_ATOMS3_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG,
      "spi bus");

  esp_err_t err;
  const esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = M5_ATOMS3_LCD_PIN_CS,
      .dc_gpio_num = M5_ATOMS3_LCD_PIN_DC,
      .spi_mode = 0,
      .pclk_hz = M5_ATOMS3_LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = queue_depth,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  err = esp_lcd_new_panel_io_spi(
      (esp_lcd_spi_bus_handle_t)M5_ATOMS3_LCD_SPI_HOST, &io_config, &out->io);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "panel io: %s", esp_err_to_name(err));
    goto fail;
  }
  err = m5_atoms3_new_panel_gc9107(out->io, M5_ATOMS3_LCD_PIN_RST, &out->panel);
  if (err != ESP_OK) {
    goto fail;
  }
  if ((err = esp_lcd_panel_reset(out->panel)) != ESP_OK ||
      (err = esp_lcd_panel_init(out->panel)) != ESP_OK ||
      (err = esp_lcd_panel_set_gap(out->panel, M5_ATOMS3_LCD_GAP_X,
                                   M5_ATOMS3_LCD_GAP_Y)) != ESP_OK ||
      (err = esp_lcd_panel_disp_on_off(out->panel, true)) != ESP_OK) {
    ESP_LOGE(TAG, "panel init: %s", esp_err_to_name(err));
    goto fail;
  }
  return ESP_OK;

fail:
  m5_atoms3_display_deinit(out);
  return err;
}

void m5_atoms3_display_deinit(m5_atoms3_display_t *display) {
  if (!display) {
    return;
  }
  if (display->panel) {
    esp_lcd_panel_del(display->panel);
    display->panel = NULL;
  }
  if (display->io) {
    esp_lcd_panel_io_del(display->io);
    display->io = NULL;
  }
  spi_bus_free(M5_ATOMS3_LCD_SPI_HOST);
}

esp_err_t m5_atoms3_backlight_init(uint8_t brightness) {
  const ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = (ledc_timer_bit_t)BACKLIGHT_BITS,
      .timer_num = BACKLIGHT_TIMER,
      .freq_hz = M5_ATOMS3_BACKLIGHT_FREQ_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
  const ledc_channel_config_t channel = {
      .gpio_num = M5_ATOMS3_BACKLIGHT_PIN,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = (ledc_channel_t)M5_ATOMS3_BACKLIGHT_LEDC_CHANNEL,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = BACKLIGHT_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight channel");
  return m5_atoms3_backlight_set(brightness);
}

esp_err_t m5_atoms3_backlight_set(uint8_t brightness) {
  uint32_t duty = 0;
  if (brightness) {
    uint32_t ofs = (M5_ATOMS3_BACKLIGHT_OFFSET * 259) >> 8;
    duty = brightness * (257 - ofs);
    duty += ofs * 255;
    duty += 1 << (15 - BACKLIGHT_BITS);
    duty >>= 16 - BACKLIGHT_BITS;
  }
  ledc_channel_t channel = (ledc_channel_t)M5_ATOMS3_BACKLIGHT_LEDC_CHANNEL;
  ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty), TAG,
                      "backlight duty");
  return ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}
