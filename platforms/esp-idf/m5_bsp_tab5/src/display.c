/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* Panel detection, backlight and the MIPI-DSI panels, from m5stack_tab5.c of
 * M5Tab5-UserDemo (bsp_detect_display_type, bsp_display_brightness_*,
 * bsp_enable_dsi_phy_power, bsp_display_new_with_handles and
 * bsp_display_new_with_handles_to_st7123). ESP-IDF 6 names the DPI input
 * format in_color_format. */
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_ili9881c.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7121.h"
#include "esp_lcd_st7123.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_touch_st7123.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "m5/tab5.h"
#include "panel_init_ili9881c.h"
#include "panel_init_st7123.h"

static const char *TAG = "m5_tab5_lcd";

#define DSI_LANES 2
#define DSI_PHY_LDO_CHANNEL 3 /* LDO_VO3 feeds VDD_MIPI_DPHY */
#define DSI_PHY_LDO_MV 2500
#define BACKLIGHT_TIMER LEDC_TIMER_0
#define BACKLIGHT_CHANNEL LEDC_CHANNEL_1
#define BACKLIGHT_MAX_DUTY 4095 /* 12-bit */

static esp_ldo_channel_handle_t phy_power;
static bool backlight_ready;

/* The ST712x touch firmware byte tells the ST7121 (1) from the ST7123 (3);
 * any other byte falls back to the ST7123 as in the factory BSP. */
static m5_tab5_panel_t st712x_variant(void) {
  esp_lcd_panel_io_handle_t io = NULL;
  esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
  config.scl_speed_hz = 100000;
  m5_tab5_panel_t kind = M5_TAB5_PANEL_ST7123;
  if (esp_lcd_new_panel_io_i2c(m5_tab5_i2c(), &config, &io) == ESP_OK) {
    uint8_t firmware = 0;
    if (esp_lcd_panel_io_rx_param(io, 0x0000, &firmware, 1) == ESP_OK) {
      if (firmware == 1)
        kind = M5_TAB5_PANEL_ST7121;
      ESP_LOGI(TAG, "ST712x touch firmware %u", firmware);
    }
    esp_lcd_panel_io_del(io);
  }
  return kind;
}

m5_tab5_panel_t m5_tab5_panel(void) {
  static m5_tab5_panel_t cached = M5_TAB5_PANEL_UNKNOWN;
  if (cached != M5_TAB5_PANEL_UNKNOWN)
    return cached;
  if (m5_tab5_init() != ESP_OK)
    return M5_TAB5_PANEL_UNKNOWN;
  i2c_master_bus_handle_t bus = m5_tab5_i2c();
  if (i2c_master_probe(bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 50) ==
      ESP_OK) {
    cached = M5_TAB5_PANEL_ILI9881C;
  } else if (i2c_master_probe(bus, 0x55, 50) == ESP_OK) {
    cached = st712x_variant();
  } else {
    ESP_LOGW(TAG, "no touch controller answered, defaulting to ILI9881C");
    cached = M5_TAB5_PANEL_ILI9881C;
  }
  ESP_LOGI(TAG, "panel %s", m5_tab5_panel_name(cached));
  return cached;
}

const char *m5_tab5_panel_name(m5_tab5_panel_t kind) {
  switch (kind) {
  case M5_TAB5_PANEL_ILI9881C:
    return "ILI9881C";
  case M5_TAB5_PANEL_ST7123:
    return "ST7123";
  case M5_TAB5_PANEL_ST7121:
    return "ST7121";
  default:
    return "Unknown";
  }
}

static esp_err_t backlight_init(void) {
  if (backlight_ready)
    return ESP_OK;
  const ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_12_BIT,
      .timer_num = BACKLIGHT_TIMER,
      .freq_hz = 5000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
  const ledc_channel_config_t channel = {
      .gpio_num = M5_TAB5_LCD_BACKLIGHT,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = BACKLIGHT_CHANNEL,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = BACKLIGHT_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight channel");
  backlight_ready = true;
  return ESP_OK;
}

esp_err_t m5_tab5_backlight_set(uint32_t percent) {
  ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight");
  if (percent > 100)
    percent = 100;
  const uint32_t duty = BACKLIGHT_MAX_DUTY * percent / 100;
  ESP_RETURN_ON_ERROR(
      ledc_set_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL, duty), TAG, "duty");
  return ledc_update_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL);
}

static esp_err_t phy_power_on(void) {
  if (phy_power)
    return ESP_OK;
  const esp_ldo_channel_config_t config = {.chan_id = DSI_PHY_LDO_CHANNEL,
                                           .voltage_mv = DSI_PHY_LDO_MV};
  return esp_ldo_acquire_channel(&config, &phy_power);
}

/* The ILI9881C runs a 730 Mbps lane at a 60 MHz pixel clock; the ST712x
 * panels 965 Mbps at 70 MHz. */
static esp_lcd_dpi_panel_config_t dpi_config(m5_tab5_panel_t kind,
                                             uint32_t fb_count) {
  esp_lcd_dpi_panel_config_t config = {
      .virtual_channel = 0,
      .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
      .dpi_clock_freq_mhz = kind == M5_TAB5_PANEL_ILI9881C ? 60 : 70,
      .in_color_format = LCD_COLOR_FMT_RGB565,
      .num_fbs = fb_count,
  };
  config.video_timing.h_size = M5_TAB5_LCD_WIDTH;
  config.video_timing.v_size = M5_TAB5_LCD_HEIGHT;
  if (kind == M5_TAB5_PANEL_ILI9881C) {
    config.video_timing.hsync_back_porch = 140;
    config.video_timing.hsync_pulse_width = 40;
    config.video_timing.hsync_front_porch = 40;
    config.video_timing.vsync_back_porch = 20;
    config.video_timing.vsync_pulse_width = 4;
    config.video_timing.vsync_front_porch = 20;
  } else {
    const bool st7121 = kind == M5_TAB5_PANEL_ST7121;
    config.video_timing.hsync_pulse_width = 2;
    config.video_timing.hsync_back_porch = 40;
    config.video_timing.hsync_front_porch = 40;
    config.video_timing.vsync_pulse_width = st7121 ? 20 : 2;
    config.video_timing.vsync_back_porch = st7121 ? 24 : 8;
    config.video_timing.vsync_front_porch = st7121 ? 200 : 220;
  }
  return config;
}

static esp_err_t create_panel(m5_tab5_display_t *d,
                              const esp_lcd_dpi_panel_config_t *dpi) {
  if (d->kind == M5_TAB5_PANEL_ILI9881C) {
    const ili9881c_vendor_config_t vendor = {
        .init_cmds = m5_tab5_ili9881c_init,
        .init_cmds_size =
            sizeof(m5_tab5_ili9881c_init) / sizeof(m5_tab5_ili9881c_init[0]),
        .mipi_config = {.dsi_bus = d->bus,
                        .dpi_config = dpi,
                        .lane_num = DSI_LANES},
    };
    const esp_lcd_panel_dev_config_t device = {
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .reset_gpio_num = -1,
        .vendor_config = (void *)&vendor,
    };
    return esp_lcd_new_panel_ili9881c(d->io, &device, &d->panel);
  }
  if (d->kind == M5_TAB5_PANEL_ST7121) {
    const st7121_vendor_config_t vendor = {
        .init_cmds = NULL,
        .init_cmds_size = 0,
        .mipi_config = {.dsi_bus = d->bus, .dpi_config = dpi},
    };
    const esp_lcd_panel_dev_config_t device = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 24,
        .vendor_config = (void *)&vendor,
    };
    return esp_lcd_new_panel_st7121(d->io, &device, &d->panel);
  }
  const st7123_vendor_config_t vendor = {
      .init_cmds = m5_tab5_st7123_init,
      .init_cmds_size =
          sizeof(m5_tab5_st7123_init) / sizeof(m5_tab5_st7123_init[0]),
      .mipi_config = {.dsi_bus = d->bus,
                      .dpi_config = dpi,
                      .lane_num = DSI_LANES},
  };
  const esp_lcd_panel_dev_config_t device = {
      .reset_gpio_num = -1,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
      .bits_per_pixel = 24,
      .vendor_config = (void *)&vendor,
  };
  return esp_lcd_new_panel_st7123(d->io, &device, &d->panel);
}

esp_err_t m5_tab5_display_new(uint32_t fb_count, m5_tab5_display_t *out) {
  ESP_RETURN_ON_FALSE(out && fb_count >= 1 && fb_count <= 3,
                      ESP_ERR_INVALID_ARG, TAG, "arguments");
  *out = (m5_tab5_display_t){0};
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight");
  ESP_RETURN_ON_ERROR(phy_power_on(), TAG, "DSI PHY power");
  out->kind = m5_tab5_panel();
  esp_err_t ret = ESP_OK;
  const esp_lcd_dsi_bus_config_t bus = {
      .bus_id = 0,
      .num_data_lanes = DSI_LANES,
      .lane_bit_rate_mbps = out->kind == M5_TAB5_PANEL_ILI9881C ? 730 : 965,
  };
  ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus, &out->bus), failed, TAG,
                    "DSI bus");
  const esp_lcd_dbi_io_config_t dbi = {
      .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8};
  ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(out->bus, &dbi, &out->io), failed,
                    TAG, "DBI IO");
  const esp_lcd_dpi_panel_config_t dpi = dpi_config(out->kind, fb_count);
  ESP_GOTO_ON_ERROR(create_panel(out, &dpi), failed, TAG, "panel");
  ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(out->panel), failed, TAG, "reset");
  ESP_GOTO_ON_ERROR(esp_lcd_panel_init(out->panel), failed, TAG, "init");
  ret = esp_lcd_panel_disp_on_off(out->panel, true);
  if (ret == ESP_ERR_NOT_SUPPORTED)
    ret = ESP_OK;
  ESP_GOTO_ON_ERROR(ret, failed, TAG, "display on");
  ESP_LOGI(TAG, "%s panel %ux%u", m5_tab5_panel_name(out->kind),
           M5_TAB5_LCD_WIDTH, M5_TAB5_LCD_HEIGHT);
  return ESP_OK;
failed:
  m5_tab5_display_del(out);
  return ret;
}

void m5_tab5_display_del(m5_tab5_display_t *d) {
  if (!d)
    return;
  if (d->panel)
    esp_lcd_panel_del(d->panel);
  if (d->io)
    esp_lcd_panel_io_del(d->io);
  if (d->bus)
    esp_lcd_del_dsi_bus(d->bus);
  *d = (m5_tab5_display_t){0};
}

/* bsp_touch_new for the GT911 at its backup address 0x14, and
 * bsp_display_indev_init_to_st7123 for the ST712x at 0x55. */
esp_err_t m5_tab5_touch_new(esp_lcd_touch_handle_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  const esp_lcd_touch_config_t config = {
      .x_max = M5_TAB5_LCD_WIDTH,
      .y_max = M5_TAB5_LCD_HEIGHT,
      .rst_gpio_num = GPIO_NUM_NC,
      .int_gpio_num = M5_TAB5_TOUCH_INT,
  };
  esp_lcd_panel_io_handle_t io = NULL;
  esp_err_t ret = ESP_OK;
  if (m5_tab5_panel() == M5_TAB5_PANEL_ILI9881C) {
    esp_lcd_panel_io_i2c_config_t io_config =
        ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
    io_config.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(m5_tab5_i2c(), &io_config, &io), TAG,
        "GT911 IO");
    ret = esp_lcd_touch_new_i2c_gt911(io, &config, out);
    if (ret == ESP_OK)
      esp_lcd_touch_exit_sleep(*out);
  } else {
    esp_lcd_panel_io_i2c_config_t io_config =
        ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    io_config.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c(m5_tab5_i2c(), &io_config, &io), TAG,
        "ST712x IO");
    ret = esp_lcd_touch_new_i2c_st7123(io, &config, out);
  }
  if (ret != ESP_OK)
    esp_lcd_panel_io_del(io);
  return ret;
}
