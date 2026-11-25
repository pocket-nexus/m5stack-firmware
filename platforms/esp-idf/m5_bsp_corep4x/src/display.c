/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Panel, backlight and touch, after lcd_init, set_lcd_power, reset_lcd,
 * reset_touch, set_lcd_brightness, touch_init, touch_enter_sleep and
 * touch_resume of M5CoreP4X-UserDemo
 * (components/m5_corep4_component/m5_corep4_component.cpp) and the panel
 * constants of m5_corep4_component.h. */
#include "board.h"
#include "cst3530.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "ioe1.h"
#include "panel_init_st7102.h"
#include "st7102.h"

static const char *TAG = "m5p4x_lcd";

#define DSI_LANES 2
#define DSI_LANE_MBPS 700
#define DSI_PHY_LDO_CHANNEL 3 /* ESP_LDO3 feeds the DSI PHY */
#define DSI_PHY_LDO_MV 2500
#define DPI_CLOCK_MHZ 24
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_PWM_HZ 50000

static esp_ldo_channel_handle_t s_phy_power;

esp_err_t m5_corep4x_backlight_set(uint32_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  /* set_lcd_brightness: frequency, duty on channel 0 (disabled at 0 %), then
   * IO9 push-pull. The expander comes out of a power cycle with IO9 open
   * drain, which leaves the backlight off. */
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_set_pwm_frequency(BACKLIGHT_PWM_HZ), TAG,
                      "PWM frequency");
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_set_pwm_duty(BACKLIGHT_PWM_CHANNEL,
                                                   (uint8_t)percent, false,
                                                   percent > 0),
                      TAG, "PWM duty");
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_drive(M5_COREP4X_IOE1_LCD_BL_PWM, false),
                      TAG, "IO9 push-pull");
  return ESP_OK;
}

static esp_err_t phy_power_on(void) {
  if (s_phy_power) {
    return ESP_OK;
  }
  const esp_ldo_channel_config_t config = {.chan_id = DSI_PHY_LDO_CHANNEL,
                                           .voltage_mv = DSI_PHY_LDO_MV};
  return esp_ldo_acquire_channel(&config, &s_phy_power);
}

esp_err_t m5_corep4x_display_new(uint32_t fb_count, m5_corep4x_display_t *out) {
  ESP_RETURN_ON_FALSE(out && fb_count >= 1 && fb_count <= 3,
                      ESP_ERR_INVALID_ARG, TAG, "arguments");
  *out = (m5_corep4x_display_t){0};
  ESP_RETURN_ON_ERROR(m5_corep4x_init(), TAG, "board");
  ESP_RETURN_ON_ERROR(m5_corep4x_set_switch(M5_COREP4X_SWITCH_LCD, true), TAG,
                      "LCD power");
  ESP_RETURN_ON_ERROR(m5_corep4x_pulse_reset(M5_COREP4X_IOE1_LCD_RST, 100, 120),
                      TAG, "LCD reset");
  ESP_RETURN_ON_ERROR(phy_power_on(), TAG, "DSI PHY power");

  esp_err_t ret = ESP_OK;
  const esp_lcd_dsi_bus_config_t bus = {
      .bus_id = 0,
      .num_data_lanes = DSI_LANES,
      .lane_bit_rate_mbps = DSI_LANE_MBPS,
  };
  ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus, &out->bus), failed, TAG,
                    "DSI bus");
  const esp_lcd_dbi_io_config_t dbi = {
      .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8};
  ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(out->bus, &dbi, &out->io), failed,
                    TAG, "DBI IO");
  esp_lcd_dpi_panel_config_t dpi = {
      .virtual_channel = 0,
      .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
      .dpi_clock_freq_mhz = DPI_CLOCK_MHZ,
      .in_color_format = LCD_COLOR_FMT_RGB565,
      .out_color_format = LCD_COLOR_FMT_RGB565,
      .num_fbs = fb_count,
  };
  dpi.video_timing.h_size = M5_COREP4X_LCD_WIDTH;
  dpi.video_timing.v_size = M5_COREP4X_LCD_HEIGHT;
  dpi.video_timing.hsync_pulse_width = 2;
  dpi.video_timing.hsync_back_porch = 40;
  dpi.video_timing.hsync_front_porch = 40;
  dpi.video_timing.vsync_pulse_width = 4;
  dpi.video_timing.vsync_back_porch = 8;
  dpi.video_timing.vsync_front_porch = 200;
  const m5_corep4x_st7102_config_t vendor = {
      .init_cmds = m5_corep4x_st7102_init,
      .init_cmds_size =
          sizeof(m5_corep4x_st7102_init) / sizeof(m5_corep4x_st7102_init[0]),
      .dsi_bus = out->bus,
      .dpi_config = &dpi,
  };
  const esp_lcd_panel_dev_config_t device = {
      .reset_gpio_num = GPIO_NUM_NC,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .bits_per_pixel = 16,
  };
  ESP_GOTO_ON_ERROR(
      m5_corep4x_st7102_new(out->io, &device, &vendor, &out->panel), failed,
      TAG, "panel");
  ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(out->panel), failed, TAG, "reset");
  ESP_GOTO_ON_ERROR(esp_lcd_panel_init(out->panel), failed, TAG, "init");
  ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(out->panel, true), failed, TAG,
                    "display on");
  ESP_LOGI(TAG, "ST7102 %ux%u", M5_COREP4X_LCD_WIDTH, M5_COREP4X_LCD_HEIGHT);
  return ESP_OK;
failed:
  m5_corep4x_display_del(out);
  return ret;
}

void m5_corep4x_display_del(m5_corep4x_display_t *d) {
  if (!d) {
    return;
  }
  if (d->panel) {
    esp_lcd_panel_disp_on_off(d->panel, false);
    esp_lcd_panel_del(d->panel);
  }
  if (d->io) {
    esp_lcd_panel_io_del(d->io);
  }
  if (d->bus) {
    esp_lcd_del_dsi_bus(d->bus);
  }
  *d = (m5_corep4x_display_t){0};
  if (m5_corep4x_set_switch(M5_COREP4X_SWITCH_LCD, false) != ESP_OK) {
    ESP_LOGW(TAG, "LCD power off failed");
  }
  if (s_phy_power) {
    esp_ldo_release_channel(s_phy_power);
    s_phy_power = NULL;
  }
}

esp_err_t m5_corep4x_touch_new(esp_lcd_touch_handle_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  ESP_RETURN_ON_ERROR(m5_corep4x_init(), TAG, "board");
  /* The factory resets the controller before it brings the LCD up, which
   * gives the CST3530 the panel's start-up time to boot; the reset here
   * comes after the panel, so it waits that time itself. */
  ESP_RETURN_ON_ERROR(m5_corep4x_pulse_reset(M5_COREP4X_IOE1_TP_RST, 100, 200),
                      TAG, "touch reset");
  esp_lcd_panel_io_handle_t io = NULL;
  ESP_RETURN_ON_ERROR(m5_corep4x_cst3530_io_new(m5_corep4x_i2c(), &io), TAG,
                      "touch IO");
  const esp_lcd_touch_config_t config = {
      .x_max = M5_COREP4X_LCD_WIDTH,
      .y_max = M5_COREP4X_LCD_HEIGHT,
      .rst_gpio_num = GPIO_NUM_NC,
      .int_gpio_num = M5_COREP4X_TOUCH_INT,
  };
  esp_err_t ret = m5_corep4x_cst3530_new(io, &config, out);
  if (ret != ESP_OK) {
    esp_lcd_panel_io_del(io);
  }
  return ret;
}

esp_err_t m5_corep4x_touch_sleep(esp_lcd_touch_handle_t touch) {
  return m5_corep4x_cst3530_sleep(touch);
}

/* touch_resume: the reset pulse, 50 ms, then normal mode. */
esp_err_t m5_corep4x_touch_info(esp_lcd_touch_handle_t touch,
                                m5_corep4x_touch_info_t *out) {
  return m5_corep4x_cst3530_info(touch, out);
}

esp_err_t m5_corep4x_touch_power_down(esp_lcd_touch_handle_t touch) {
  esp_err_t err = touch ? m5_corep4x_cst3530_sleep(touch) : ESP_OK;
  if (touch) {
    esp_lcd_touch_del(touch);
  }
  /* TP_RST open drain low; the interrupt line released. */
  const esp_err_t reset = m5_corep4x_ioe1_output(M5_COREP4X_IOE1_TP_RST, true);
  if (reset == ESP_OK) {
    m5_corep4x_ioe1_write(M5_COREP4X_IOE1_TP_RST, false);
  }
  m5_corep4x_gpio_high_z(M5_COREP4X_TOUCH_INT);
  return err != ESP_OK ? err : reset;
}

esp_err_t m5_corep4x_touch_wake(esp_lcd_touch_handle_t touch) {
  ESP_RETURN_ON_ERROR(m5_corep4x_pulse_reset(M5_COREP4X_IOE1_TP_RST, 10, 50),
                      TAG, "touch reset");
  return m5_corep4x_cst3530_normal(touch);
}
