/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 Waveshare
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* esp_lcd_touch_cst3xx.c as vendored by M5CoreP4X-UserDemo
 * (components/m5_corep4_component/third_party_component/
 * esp_lcd_touch_cst3xx): register addresses are 32-bit words sent big-endian
 * as one command byte and three parameter bytes; a read sends the address,
 * then reads with the command byte 0x00. A report is 9 bytes for the first
 * point and 5 bytes for each further one, and is acknowledged with
 * 0xD00002AB. The reset GPIO, the firmware updater and the work modes other
 * than normal and deep sleep are left out. */
#include "cst3530.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "m5p4x_cst3530";

#define MAX_POINTS 5
#define POINT_LIMIT                                                            \
  (MAX_POINTS < CONFIG_ESP_LCD_TOUCH_MAX_POINTS                                \
       ? MAX_POINTS                                                            \
       : CONFIG_ESP_LCD_TOUCH_MAX_POINTS)

#define REG_INFO 0xD0030000u
#define REG_REPORT 0xD0070000u
#define CMD_END_OF_REPORT 0xD00002ABu
#define CMD_LP_SCAN_OFF 0xD0000400u
#define CMD_NORMAL_0 0xD0000000u
#define CMD_NORMAL_1 0xD0000C00u
#define CMD_NORMAL_2 0xD0000100u
#define CMD_DEEP_SLEEP 0xD00022ABu

static esp_err_t write_cmd(esp_lcd_touch_handle_t tp, uint32_t reg) {
  const uint8_t bytes[4] = {(uint8_t)(reg >> 24), (uint8_t)(reg >> 16),
                            (uint8_t)(reg >> 8), (uint8_t)reg};
  return esp_lcd_panel_io_tx_param(tp->io, bytes[0], &bytes[1], 3);
}

static esp_err_t read_reg(esp_lcd_touch_handle_t tp, uint32_t reg,
                          uint8_t *data, size_t len) {
  ESP_RETURN_ON_ERROR(write_cmd(tp, reg), TAG, "address");
  return esp_lcd_panel_io_rx_param(tp->io, 0x00, data, len);
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp) {
  uint8_t data[9 + (MAX_POINTS - 1) * 5] = {0};
  esp_err_t err = read_reg(tp, REG_REPORT, data, 9);
  if (err == ESP_OK) {
    const uint8_t fingers = data[3] & 0x0F;
    const uint8_t keys = (data[3] & 0xF0) >> 4;
    uint8_t items = (uint8_t)(keys + fingers);
    items = items > POINT_LIMIT ? POINT_LIMIT : items;
    if (items > 1) {
      (void)esp_lcd_panel_io_rx_param(tp->io, 0x00, &data[9],
                                      (size_t)(items - 1) * 5);
    }
    err = write_cmd(tp, CMD_END_OF_REPORT);
    uint8_t points = fingers > POINT_LIMIT ? POINT_LIMIT : fingers;
    if (keys >= items) {
      points = 0;
    } else if (keys + points > items) {
      points = (uint8_t)(items - keys);
    }
    /* A zero key state is a hover or a spurious report. */
    const uint8_t state = data[8] >> 4;
    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;
    if (err == ESP_OK && fingers && state != 0) {
      for (uint8_t i = 0; i < points; i++) {
        const size_t index = (size_t)(keys + i) * 5;
        tp->data.coords[i].x =
            (uint16_t)(data[index + 4] + ((data[index + 7] & 0x0F) << 8));
        tp->data.coords[i].y =
            (uint16_t)(data[index + 5] + ((data[index + 7] & 0xF0) << 4));
        tp->data.coords[i].strength = 1;
        tp->data.coords[i].track_id = i;
      }
      tp->data.points = points;
    }
    portEXIT_CRITICAL(&tp->data.lock);
  }
  if (err != ESP_OK) {
    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);
    static TickType_t last_warning;
    const TickType_t now = xTaskGetTickCount();
    if (now - last_warning > pdMS_TO_TICKS(1000)) {
      ESP_LOGW(TAG, "read: %s", esp_err_to_name(err));
      last_warning = now;
    }
  }
  /* The factory driver reports a failed read as no contact. */
  return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                   uint16_t *strength, uint8_t *point_num,
                   uint8_t max_point_num) {
  portENTER_CRITICAL(&tp->data.lock);
  const uint8_t n =
      tp->data.points > max_point_num ? max_point_num : tp->data.points;
  for (uint8_t i = 0; i < n; i++) {
    x[i] = tp->data.coords[i].x;
    y[i] = tp->data.coords[i].y;
    if (strength) {
      strength[i] = 1;
    }
  }
  *point_num = n;
  portEXIT_CRITICAL(&tp->data.lock);
  return n > 0;
}

static esp_err_t del(esp_lcd_touch_handle_t tp) {
  if (tp->config.int_gpio_num != GPIO_NUM_NC) {
    gpio_reset_pin(tp->config.int_gpio_num);
  }
  if (tp->io) {
    esp_lcd_panel_io_del(tp->io);
  }
  free(tp);
  return ESP_OK;
}

static esp_err_t normal_mode(esp_lcd_touch_handle_t tp, uint32_t settle_ms) {
  /* The low-power I2C scan is left twice, as the factory driver does. */
  esp_err_t err = write_cmd(tp, CMD_LP_SCAN_OFF);
  vTaskDelay(pdMS_TO_TICKS(settle_ms));
  err |= write_cmd(tp, CMD_LP_SCAN_OFF);
  vTaskDelay(pdMS_TO_TICKS(settle_ms));
  err |= write_cmd(tp, CMD_NORMAL_0);
  err |= write_cmd(tp, CMD_NORMAL_1);
  err |= write_cmd(tp, CMD_NORMAL_2);
  return err == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t m5_corep4x_cst3530_io_new(i2c_master_bus_handle_t bus,
                                    esp_lcd_panel_io_handle_t *out) {
  const esp_lcd_panel_io_i2c_config_t config = {
      .dev_addr = M5_COREP4X_CST3530_ADDR,
      .control_phase_bytes = 1,
      .dc_bit_offset = 0,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .flags = {.disable_control_phase = 1},
      .scl_speed_hz = 400000,
  };
  return esp_lcd_new_panel_io_i2c(bus, &config, out);
}

esp_err_t m5_corep4x_cst3530_new(esp_lcd_panel_io_handle_t io,
                                 const esp_lcd_touch_config_t *config,
                                 esp_lcd_touch_handle_t *out) {
  ESP_RETURN_ON_FALSE(io && config && out, ESP_ERR_INVALID_ARG, TAG,
                      "arguments");
  esp_lcd_touch_handle_t tp =
      heap_caps_calloc(1, sizeof(esp_lcd_touch_t), MALLOC_CAP_DEFAULT);
  ESP_RETURN_ON_FALSE(tp, ESP_ERR_NO_MEM, TAG, "touch");
  tp->io = io;
  tp->read_data = read_data;
  tp->get_xy = get_xy;
  tp->del = del;
  tp->data.lock.owner = portMUX_FREE_VAL;
  memcpy(&tp->config, config, sizeof(*config));

  (void)normal_mode(tp, 20);
  uint8_t info[50] = {0};
  esp_err_t err = read_reg(tp, REG_INFO, info, sizeof(info));
  if (err != ESP_OK) {
    free(tp);
    return err;
  }
  ESP_LOGI(TAG,
           "chip 0x%02X%02X%02X%02X firmware 0x%02X%02X%02X%02X, %ux%u, TX %u "
           "RX %u",
           info[3], info[2], info[1], info[0], info[35], info[34], info[33],
           info[32], (unsigned)(info[28] | (info[29] << 8)),
           (unsigned)(info[30] | (info[31] << 8)), info[48], info[49]);

  if (config->int_gpio_num != GPIO_NUM_NC) {
    const gpio_config_t int_config = {
        .mode = GPIO_MODE_INPUT,
        .intr_type =
            config->levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
        .pin_bit_mask = BIT64(config->int_gpio_num),
    };
    err = gpio_config(&int_config);
    if (err == ESP_OK && config->interrupt_callback) {
      err = esp_lcd_touch_register_interrupt_callback(
          tp, config->interrupt_callback);
    }
    if (err != ESP_OK) {
      free(tp);
      return err;
    }
  }
  *out = tp;
  return ESP_OK;
}

esp_err_t m5_corep4x_cst3530_info(esp_lcd_touch_handle_t tp,
                                  m5_corep4x_touch_info_t *out) {
  ESP_RETURN_ON_FALSE(tp && out, ESP_ERR_INVALID_ARG, TAG, "arguments");
  uint8_t info[50] = {0};
  ESP_RETURN_ON_ERROR(read_reg(tp, REG_INFO, info, sizeof(info)), TAG, "info");
  out->chip_id = (uint32_t)info[0] | ((uint32_t)info[1] << 8) |
                 ((uint32_t)info[2] << 16) | ((uint32_t)info[3] << 24);
  out->firmware = (uint32_t)info[32] | ((uint32_t)info[33] << 8) |
                  ((uint32_t)info[34] << 16) | ((uint32_t)info[35] << 24);
  out->width = (uint16_t)(info[28] | (info[29] << 8));
  out->height = (uint16_t)(info[30] | (info[31] << 8));
  return ESP_OK;
}

esp_err_t m5_corep4x_cst3530_sleep(esp_lcd_touch_handle_t tp) {
  ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "touch");
  esp_err_t err = write_cmd(tp, CMD_LP_SCAN_OFF);
  vTaskDelay(pdMS_TO_TICKS(1));
  err |= write_cmd(tp, CMD_LP_SCAN_OFF);
  err |= write_cmd(tp, CMD_DEEP_SLEEP);
  return err == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t m5_corep4x_cst3530_normal(esp_lcd_touch_handle_t tp) {
  ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "touch");
  return normal_mode(tp, 1);
}
