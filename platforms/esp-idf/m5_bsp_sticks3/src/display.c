/*----------------------------------------------------------------------------/
  Lovyan GFX - Graphics library for embedded devices.

Original Source:
 https://github.com/lovyan03/LovyanGFX/

Licence:
 [FreeBSD](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt)

Author:
 [lovyan03](https://twitter.com/lovyan03)

Contributors:
 [ciniml](https://github.com/ciniml)
 [mongonta0716](https://github.com/mongonta0716)
 [tobozo](https://github.com/tobozo)
/----------------------------------------------------------------------------*/
// SPDX-License-Identifier: BSD-2-Clause AND MIT
//
// StickS3 panel bring-up for ESP-IDF esp_lcd. The ST7789 vendor sequence is
// M5GFX src/lgfx/v1/panel/Panel_ST7789.hpp (getInitCommands list0); bus,
// offsets, inversion and backlight come from M5GFX
// src/board_detect/m5/generated/esp32s3_board_data.hpp (specs::sticks3) and
// src/lgfx/v1/platforms/esp32/Light_PWM.inl.

#include "m5_sticks3/display.h"
#include "m5_sticks3/board.h"

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "m5_sticks3_lcd";

#define LCD_SPI_HOST SPI3_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

#define BL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BL_LEDC_TIMER LEDC_TIMER_3
#define BL_LEDC_CHANNEL LEDC_CHANNEL_7
#define BL_PWM_BITS 9
#define BL_PWM_HZ 256
#define BL_OFFSET 16

typedef struct {
  uint8_t command;
  uint8_t length;
  uint8_t data[14];
} lcd_init_command_t;

// Panel_ST7789 list0 without SLPOUT/RAMCTRL, which esp_lcd_panel_init sends
// (RAMCTRL 0x00,0xF0 selects the same big-endian RGB565 interface as M5GFX's
// 0x00,0xC0), and without IDMOFF/DISPON, sent after the gap is set.
// clang-format off
static const lcd_init_command_t k_st7789_init[] = {
    {0xB7, 1, {0x35}},             // GCTRL gate control
    {0xBB, 1, {0x28}},             // VCOMS
    {0xC0, 1, {0x0C}},             // LCMCTRL
    {0xC2, 2, {0x01, 0xFF}},       // VDVVRHEN
    {0xC3, 1, {0x10}},             // VRHS
    {0xC4, 1, {0x20}},             // VDVSET
    {0xD0, 2, {0xA4, 0xA1}},       // PWCTRL1
    {0xE0, 14, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44,
                0x42, 0x06, 0x0E, 0x12, 0x14, 0x17}}, // PVGAMCTRL
    {0xE1, 14, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54,
                0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E}}, // NVGAMCTRL
    {0x38, 0, {0}},                // IDMOFF
};
// clang-format on

esp_err_t m5_sticks3_display_init(size_t max_transfer_bytes,
                                  m5_sticks3_display_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  memset(out, 0, sizeof(*out));
  const spi_bus_config_t bus = {
      .mosi_io_num = M5_STICKS3_LCD_MOSI,
      .miso_io_num = GPIO_NUM_NC,
      .sclk_io_num = M5_STICKS3_LCD_SCLK,
      .quadwp_io_num = GPIO_NUM_NC,
      .quadhd_io_num = GPIO_NUM_NC,
      .max_transfer_sz = (int)max_transfer_bytes,
  };
  ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO),
                      TAG, "SPI bus");

  const esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = M5_STICKS3_LCD_CS,
      .dc_gpio_num = M5_STICKS3_LCD_DC,
      .spi_mode = 0,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = 10,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  esp_err_t err = esp_lcd_new_panel_io_spi(
      (esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &out->io);
  if (err != ESP_OK) {
    spi_bus_free(LCD_SPI_HOST);
    return err;
  }

  const esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = M5_STICKS3_LCD_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
      .bits_per_pixel = 16,
  };
  err = esp_lcd_new_panel_st7789(out->io, &panel_config, &out->panel);
  if (err == ESP_OK) {
    err = esp_lcd_panel_reset(out->panel);
  }
  if (err == ESP_OK) {
    err = esp_lcd_panel_init(out->panel);
  }
  for (size_t i = 0;
       err == ESP_OK && i < sizeof(k_st7789_init) / sizeof(k_st7789_init[0]);
       ++i) {
    err = esp_lcd_panel_io_tx_param(out->io, k_st7789_init[i].command,
                                    k_st7789_init[i].data,
                                    k_st7789_init[i].length);
  }
  if (err == ESP_OK) {
    err = esp_lcd_panel_invert_color(out->panel, true);
  }
  if (err == ESP_OK) {
    err = esp_lcd_panel_set_gap(out->panel, M5_STICKS3_LCD_OFFSET_X,
                                M5_STICKS3_LCD_OFFSET_Y);
  }
  if (err == ESP_OK) {
    err = esp_lcd_panel_disp_on_off(out->panel, true);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "panel init: %s", esp_err_to_name(err));
    if (out->panel) {
      esp_lcd_panel_del(out->panel);
    }
    esp_lcd_panel_io_del(out->io);
    spi_bus_free(LCD_SPI_HOST);
    memset(out, 0, sizeof(*out));
    return err;
  }
  out->width = M5_STICKS3_LCD_WIDTH;
  out->height = M5_STICKS3_LCD_HEIGHT;
  return ESP_OK;
}

esp_err_t m5_sticks3_display_clear(const m5_sticks3_display_t *display,
                                   uint16_t rgb565) {
  ESP_RETURN_ON_FALSE(display && display->panel, ESP_ERR_INVALID_STATE, TAG,
                      "display");
  enum { ROWS = 16 };
  const size_t pixels = (size_t)display->width * ROWS;
  uint16_t *strip = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
  ESP_RETURN_ON_FALSE(strip, ESP_ERR_NO_MEM, TAG, "clear strip");
  const uint16_t wire = (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
  for (size_t i = 0; i < pixels; ++i) {
    strip[i] = wire;
  }
  esp_err_t err = ESP_OK;
  // Every queued transfer reads the same unchanged strip.
  for (uint32_t y = 0; err == ESP_OK && y < display->height; y += ROWS) {
    const uint32_t end =
        y + ROWS < display->height ? y + ROWS : display->height;
    err = esp_lcd_panel_draw_bitmap(display->panel, 0, (int)y,
                                    (int)display->width, (int)end, strip);
  }
  // A parameter transaction waits for the queued color transfers.
  const esp_err_t barrier =
      esp_lcd_panel_io_tx_param(display->io, 0x00 /* NOP */, NULL, 0);
  heap_caps_free(strip);
  return err != ESP_OK ? err : barrier;
}

esp_err_t m5_sticks3_backlight_init(void) {
  const ledc_timer_config_t timer = {
      .speed_mode = BL_LEDC_MODE,
      .duty_resolution = (ledc_timer_bit_t)BL_PWM_BITS,
      .timer_num = BL_LEDC_TIMER,
      .freq_hz = BL_PWM_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "LEDC timer");
  const ledc_channel_config_t channel = {
      .gpio_num = M5_STICKS3_LCD_BL,
      .speed_mode = BL_LEDC_MODE,
      .channel = BL_LEDC_CHANNEL,
      .timer_sel = BL_LEDC_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  return ledc_channel_config(&channel);
}

esp_err_t m5_sticks3_backlight_set(uint8_t brightness) {
  // Light_PWM::setBrightness with PWM_BITS 9 and offset 16.
  uint32_t duty = 0;
  if (brightness) {
    const uint32_t offset = (BL_OFFSET * 259u) >> 8;
    duty = brightness * (257u - offset);
    duty += offset * 255u;
    duty += 1u << (15 - BL_PWM_BITS);
    duty >>= 16 - BL_PWM_BITS;
  }
  ESP_RETURN_ON_ERROR(ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty), TAG,
                      "duty");
  return ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}
