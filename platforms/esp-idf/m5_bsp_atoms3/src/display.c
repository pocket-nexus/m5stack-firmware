// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-FileCopyrightText: lovyan03 (LovyanGFX, as shipped in M5GFX)
// SPDX-License-Identifier: MIT AND BSD-2-Clause
//
// Panel bus setup follows M5GFX 0.2.19 src/M5GFX.cpp (board_M5AtomS3 and
// board_M5AtomS3R, GC9107 branches; the AtomS3R panel ID read of
// _read_panel_id; Light_M5StackAtomS3R for the AtomS3R backlight). The
// AtomS3 backlight duty formula is Light_PWM::setBrightness from
// src/lgfx/v1/platforms/esp32/Light_PWM.cpp (LovyanGFX, FreeBSD licence).
#include "m5_atoms3/display.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_io_spi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_atoms3/board.h"
#include "m5_atoms3/imu.h"
#include "panel_gc9107.h"

static const char *TAG = "m5_atoms3_display";

#define BACKLIGHT_BITS 9
#define BACKLIGHT_TIMER                                                        \
  ((ledc_timer_t)((M5_ATOMS3_BACKLIGHT_LEDC_CHANNEL >> 1) & 3))

// RDDID over the 3-wire line at M5GFX's read clock.
#define PANEL_RDDID 0x04
#define PANEL_READ_CLOCK_HZ (16 * 1000 * 1000)

// AtomS3R backlight driver registers (M5GFX Light_M5StackAtomS3R).
#define BACKLIGHT_I2C_HZ 400000
#define BACKLIGHT_I2C_TIMEOUT_MS 20
#define BACKLIGHT_REG_ENABLE 0x00
#define BACKLIGHT_REG_CONFIG 0x08
#define BACKLIGHT_REG_CURRENT 0x0E
#define BACKLIGHT_REG_RESET 0x70

typedef struct {
  gpio_num_t mosi, sclk, dc, cs, rst;
} panel_pins_t;

static const panel_pins_t PINS_ATOMS3 = {
    M5_ATOMS3_LCD_PIN_MOSI, M5_ATOMS3_LCD_PIN_SCLK, M5_ATOMS3_LCD_PIN_DC,
    M5_ATOMS3_LCD_PIN_CS,   M5_ATOMS3_LCD_PIN_RST,
};
static const panel_pins_t PINS_ATOMS3R = {
    M5_ATOMS3R_LCD_PIN_MOSI, M5_ATOMS3R_LCD_PIN_SCLK, M5_ATOMS3R_LCD_PIN_DC,
    M5_ATOMS3R_LCD_PIN_CS,   M5_ATOMS3R_LCD_PIN_RST,
};

static i2c_master_dev_handle_t s_backlight_i2c;

// M5GFX _read_panel_id: after a reset, RDDID with one dummy clock before
// the ID bytes, read on MOSI. esp_lcd reads whole bytes, so the 32 bits read
// start with the dummy bit. Returns ID1 | ID2 << 8 | ID3 << 16, the value
// M5GFX compares.
static esp_err_t read_panel_id(const panel_pins_t *pins, uint32_t *out) {
  const gpio_config_t rst = {
      .pin_bit_mask = 1ULL << pins->rst,
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&rst), TAG, "panel reset pin");
  gpio_set_level(pins->rst, 0);
  vTaskDelay(pdMS_TO_TICKS(2));
  gpio_set_level(pins->rst, 1);
  vTaskDelay(pdMS_TO_TICKS(120));
  // Release the pin: the panel driver configures it again.
  gpio_reset_pin(pins->rst);

  const esp_lcd_panel_io_spi_config_t config = {
      .cs_gpio_num = pins->cs,
      .dc_gpio_num = pins->dc,
      .spi_mode = 0,
      .pclk_hz = PANEL_READ_CLOCK_HZ,
      .trans_queue_depth = 1,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .flags.sio_mode = 1,
  };
  esp_lcd_panel_io_handle_t io = NULL;
  ESP_RETURN_ON_ERROR(
      esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)M5_ATOMS3_LCD_SPI_HOST,
                               &config, &io),
      TAG, "panel id io");
  uint8_t bytes[4] = {0};
  esp_err_t err = esp_lcd_panel_io_rx_param(io, PANEL_RDDID, bytes, 4);
  esp_lcd_panel_io_del(io);
  ESP_RETURN_ON_ERROR(err, TAG, "RDDID");
  const uint32_t bits = ((uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 |
                         (uint32_t)bytes[2] << 8 | bytes[3])
                        << 1;
  *out =
      (bits >> 24) | ((bits >> 16) & 0xFFU) << 8 | ((bits >> 8) & 0xFFU) << 16;
  return ESP_OK;
}

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
  const bool atoms3r = m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3R;
  const panel_pins_t *pins = atoms3r ? &PINS_ATOMS3R : &PINS_ATOMS3;

  const spi_bus_config_t bus = {
      .mosi_io_num = pins->mosi,
      .miso_io_num = -1,
      .sclk_io_num = pins->sclk,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = (int)max_transfer,
  };
  ESP_RETURN_ON_ERROR(
      spi_bus_initialize(M5_ATOMS3_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG,
      "spi bus");

  esp_err_t err;
  if (atoms3r) {
    // Some AtomS3R units carry an ST7735S, which this component does not
    // drive; M5GFX tells the two apart by RDDID.
    uint32_t id = 0;
    if ((err = read_panel_id(pins, &id)) != ESP_OK) {
      goto fail;
    }
    if ((id & 0xFFFFFFU) != M5_ATOMS3R_PANEL_ID_GC9107) {
      ESP_LOGE(TAG, "AtomS3R panel id 0x%06x: %s, not supported", (unsigned)id,
               (id & 0xFFU) == M5_ATOMS3R_PANEL_ID_ST7735S ? "ST7735S"
                                                           : "unknown");
      err = ESP_ERR_NOT_SUPPORTED;
      goto fail;
    }
    ESP_LOGI(TAG, "AtomS3R panel GC9107 (id 0x%06x)", (unsigned)id);
  }
  const esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = pins->cs,
      .dc_gpio_num = pins->dc,
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
  err = m5_atoms3_new_panel_gc9107(out->io, pins->rst, &out->panel);
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

static esp_err_t backlight_write(uint8_t reg, uint8_t value) {
  const uint8_t data[2] = {reg, value};
  return i2c_master_transmit(s_backlight_i2c, data, sizeof(data),
                             BACKLIGHT_I2C_TIMEOUT_MS);
}

// AtomS3R: the LED driver at 0x30 on the internal I2C bus.
static esp_err_t backlight_i2c_init(uint8_t brightness) {
  if (!s_backlight_i2c) {
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(m5_atoms3_i2c_internal_init(&bus), TAG, "i2c bus");
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = M5_ATOMS3R_BACKLIGHT_ADDRESS,
        .scl_speed_hz = BACKLIGHT_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus, &device, &s_backlight_i2c), TAG,
        "backlight device");
  }
  ESP_RETURN_ON_ERROR(backlight_write(BACKLIGHT_REG_ENABLE, 0x40), TAG,
                      "backlight enable");
  vTaskDelay(pdMS_TO_TICKS(1));
  ESP_RETURN_ON_ERROR(backlight_write(BACKLIGHT_REG_CONFIG, 0x01), TAG,
                      "backlight config");
  ESP_RETURN_ON_ERROR(backlight_write(BACKLIGHT_REG_RESET, 0x00), TAG,
                      "backlight reset");
  return m5_atoms3_backlight_set(brightness);
}

esp_err_t m5_atoms3_backlight_init(uint8_t brightness) {
  if (m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3R) {
    return backlight_i2c_init(brightness);
  }
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
  if (m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3R) {
    ESP_RETURN_ON_FALSE(s_backlight_i2c, ESP_ERR_INVALID_STATE, TAG,
                        "backlight not initialised");
    return backlight_write(BACKLIGHT_REG_CURRENT, brightness);
  }
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
