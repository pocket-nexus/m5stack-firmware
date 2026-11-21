/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * CST820 driver for esp_lcd_touch. Register layout and decoding follow the
 * factory Cst820 class (M5StopWatch-UserDemo main/hal/drivers/cst820/
 * cst820.cpp): one read of registers 0x00..0x06 gives the finger count
 * (byte 2), the event (bits 7..6 of byte 3) and 12-bit X and Y.
 */
#include "cst820.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

static const char *TAG = "m5sw_cst820";

#define REG_STATUS 0x00
#define REG_CHIP_ID 0xA7
#define REG_SOFT_VER 0xA9
#define REG_SLEEP 0xE5

typedef struct {
  esp_lcd_touch_t base;
  i2c_master_dev_handle_t dev;
} cst820_t;

static esp_err_t read_regs(cst820_t *t, uint8_t reg, uint8_t *data,
                           size_t len) {
  return i2c_master_transmit_receive(t->dev, &reg, 1, data, len, 100);
}

static uint16_t clamp(uint16_t v, uint16_t max) {
  return (max > 0 && v >= max) ? (uint16_t)(max - 1) : v;
}

static esp_err_t cst820_read_data(esp_lcd_touch_handle_t tp) {
  cst820_t *t = __containerof(tp, cst820_t, base);
  uint8_t buf[7];
  ESP_RETURN_ON_ERROR(read_regs(t, REG_STATUS, buf, sizeof(buf)), TAG,
                      "read touch data");

  const uint8_t fingers = buf[2];
  const uint8_t event = (buf[3] & 0xC0) >> 6; /* 0 down, 1 up, 2 contact */
  const bool pressed = fingers > 0 && (event == 0 || event == 2);
  const uint16_t x = (uint16_t)(((buf[3] & 0x0F) << 8) | buf[4]);
  const uint16_t y = (uint16_t)(((buf[5] & 0x0F) << 8) | buf[6]);

  portENTER_CRITICAL(&tp->data.lock);
  tp->data.points = pressed ? 1 : 0;
  if (pressed) {
    tp->data.coords[0].x = clamp(x, tp->config.x_max);
    tp->data.coords[0].y = clamp(y, tp->config.y_max);
    tp->data.coords[0].strength = 0;
    tp->data.coords[0].track_id = 0;
  }
  portEXIT_CRITICAL(&tp->data.lock);
  return ESP_OK;
}

static bool cst820_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                          uint16_t *strength, uint8_t *point_num,
                          uint8_t max_point_num) {
  portENTER_CRITICAL(&tp->data.lock);
  const uint8_t n =
      tp->data.points < max_point_num ? tp->data.points : max_point_num;
  for (uint8_t i = 0; i < n; i++) {
    x[i] = tp->data.coords[i].x;
    y[i] = tp->data.coords[i].y;
    if (strength) {
      strength[i] = tp->data.coords[i].strength;
    }
  }
  *point_num = n;
  tp->data.points = 0;
  portEXIT_CRITICAL(&tp->data.lock);
  return n > 0;
}

static esp_err_t cst820_enter_sleep(esp_lcd_touch_handle_t tp) {
  cst820_t *t = __containerof(tp, cst820_t, base);
  const uint8_t buf[2] = {REG_SLEEP, 0x03};
  return i2c_master_transmit(t->dev, buf, sizeof(buf), 100);
}

static esp_err_t cst820_del(esp_lcd_touch_handle_t tp) {
  cst820_t *t = __containerof(tp, cst820_t, base);
  if (tp->config.int_gpio_num != GPIO_NUM_NC) {
    gpio_reset_pin(tp->config.int_gpio_num);
  }
  i2c_master_bus_rm_device(t->dev);
  free(t);
  return ESP_OK;
}

esp_err_t m5_stopwatch_cst820_new(i2c_master_bus_handle_t bus,
                                  const esp_lcd_touch_config_t *config,
                                  esp_lcd_touch_handle_t *out) {
  ESP_RETURN_ON_FALSE(bus && config && out, ESP_ERR_INVALID_ARG, TAG, "arg");
  cst820_t *t = calloc(1, sizeof(*t));
  ESP_RETURN_ON_FALSE(t, ESP_ERR_NO_MEM, TAG, "no memory");

  const i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_STOPWATCH_CST820_ADDR,
      .scl_speed_hz = 100000,
  };
  esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &t->dev);
  if (err != ESP_OK) {
    free(t);
    return err;
  }

  uint8_t chip_id = 0, soft_ver = 0;
  err = read_regs(t, REG_CHIP_ID, &chip_id, 1);
  if (err == ESP_OK) {
    err = read_regs(t, REG_SOFT_VER, &soft_ver, 1);
  }
  if (err == ESP_OK && (chip_id == 0 || soft_ver == 0)) {
    err = ESP_ERR_NOT_FOUND;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "no CST820 at 0x%02X: %s", M5_STOPWATCH_CST820_ADDR,
             esp_err_to_name(err));
    i2c_master_bus_rm_device(t->dev);
    free(t);
    return err;
  }
  ESP_LOGI(TAG, "chip ID 0x%02X, firmware 0x%02X", chip_id, soft_ver);

  t->base.config = *config;
  t->base.read_data = cst820_read_data;
  t->base.get_xy = cst820_get_xy;
  t->base.enter_sleep = cst820_enter_sleep;
  t->base.del = cst820_del;
  portMUX_INITIALIZE(&t->base.data.lock);

  if (config->int_gpio_num != GPIO_NUM_NC) {
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << config->int_gpio_num,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&io);
    if (err == ESP_OK && config->interrupt_callback) {
      err = esp_lcd_touch_register_interrupt_callback(
          &t->base, config->interrupt_callback);
    }
    if (err != ESP_OK) {
      i2c_master_bus_rm_device(t->dev);
      free(t);
      return err;
    }
  }
  *out = &t->base;
  return ESP_OK;
}
