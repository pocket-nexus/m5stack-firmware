/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * RX8130CE driver ported from RX8130_Class (M5StopWatch-UserDemo
 * main/hal/drivers/rx8130/rx8130.cpp) to the i2c_master driver. The device
 * runs at 100 kHz, the i2c_bus default the factory firmware uses.
 */
#include "rx8130.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>

static const char *TAG = "m5sw_rx8130";

#define REG_SEC 0x10
#define REG_WDAY 0x13
#define REG_CTRL0 0x1E
#define CTRL0_STOP (1 << 6)

static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_lock;

static uint8_t bcd2dec(uint8_t v) {
  return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static uint8_t dec2bcd(int v) { return (uint8_t)(((v / 10) << 4) + (v % 10)); }

static bool is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* month 1..12 */
static int days_in_month(int month, int year) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (month == 2 && is_leap_year(year)) ? 29 : days[month - 1];
}

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

static esp_err_t write_regs(uint8_t reg, const uint8_t *data, size_t len) {
  uint8_t buf[8];
  if (len > sizeof(buf) - 1) {
    return ESP_ERR_INVALID_SIZE;
  }
  buf[0] = reg;
  for (size_t i = 0; i < len; i++) {
    buf[i + 1] = data[i];
  }
  return i2c_master_transmit(s_dev, buf, len + 1, 100);
}

static esp_err_t set_stop(bool stop) {
  uint8_t ctrl = 0;
  ESP_RETURN_ON_ERROR(read_regs(REG_CTRL0, &ctrl, 1), TAG, "CTRL0");
  ctrl = stop ? (uint8_t)(ctrl | CTRL0_STOP) : (uint8_t)(ctrl & ~CTRL0_STOP);
  return write_regs(REG_CTRL0, &ctrl, 1);
}

/* Writes registers with the STOP bit set around the write. */
static esp_err_t write_stopped(uint8_t reg, const uint8_t *data, size_t len) {
  ESP_RETURN_ON_ERROR(set_stop(true), TAG, "stop");
  esp_err_t err = write_regs(reg, data, len);
  esp_err_t restart = set_stop(false);
  return err != ESP_OK ? err : restart;
}

esp_err_t m5_stopwatch_rx8130_begin(i2c_master_bus_handle_t bus) {
  ESP_RETURN_ON_FALSE(!s_dev, ESP_ERR_INVALID_STATE, TAG, "already started");
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "no mutex");
  }
  const i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_STOPWATCH_RX8130_ADDR,
      .scl_speed_hz = 100000,
  };
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG,
                      "add device");
  uint8_t ctrl = 0;
  esp_err_t err = read_regs(REG_CTRL0, &ctrl, 1);
  if (err != ESP_OK) {
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
  }
  return err;
}

static esp_err_t get_locked(struct tm *t) {
  uint8_t d[7];
  ESP_RETURN_ON_ERROR(read_regs(REG_SEC, d, sizeof(d)), TAG, "read time");
  *t = (struct tm){0};
  t->tm_sec = bcd2dec(d[0] & 0x7F);
  t->tm_min = bcd2dec(d[1] & 0x7F);
  t->tm_hour = bcd2dec(d[2] & 0x3F); /* 24-hour clock only */
  t->tm_wday = bcd2dec(d[3] & 0x7F);
  t->tm_mday = bcd2dec(d[4] & 0x3F);
  int month = bcd2dec(d[5] & 0x1F); /* 1..12 */
  t->tm_year = bcd2dec(d[6]) + 100;
  t->tm_isdst = -1;

  /* The RX8130 does not roll an invalid day or month over by itself. */
  bool changed = false;
  if (month >= 1 && month <= 12 &&
      t->tm_mday > days_in_month(month, t->tm_year + 1900)) {
    t->tm_mday = 1;
    if (++month > 12) {
      month = 1;
      t->tm_year++;
    }
    changed = true;
  } else if (month > 12) {
    month = 1;
    t->tm_year++;
    changed = true;
  } else if (month < 1) {
    month = 12;
    t->tm_year--;
    changed = true;
  }
  t->tm_mon = month - 1;

  if (changed) {
    const uint8_t date[4] = {dec2bcd(t->tm_wday), dec2bcd(t->tm_mday),
                             dec2bcd(month), dec2bcd((t->tm_year - 100) % 100)};
    ESP_RETURN_ON_ERROR(write_stopped(REG_WDAY, date, sizeof(date)), TAG,
                        "repair date");
  }
  return ESP_OK;
}

esp_err_t m5_stopwatch_rx8130_get(struct tm *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = get_locked(out);
  xSemaphoreGive(s_lock);
  return err;
}

esp_err_t m5_stopwatch_rx8130_set(const struct tm *t) {
  ESP_RETURN_ON_FALSE(t, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  ESP_RETURN_ON_FALSE(t->tm_year >= 100 && t->tm_year < 200 && t->tm_mon >= 0 &&
                          t->tm_mon < 12 && t->tm_mday >= 1 &&
                          t->tm_mday <= 31 && t->tm_hour >= 0 &&
                          t->tm_hour < 24 && t->tm_min >= 0 && t->tm_min < 60 &&
                          t->tm_sec >= 0 && t->tm_sec < 60 && t->tm_wday >= 0 &&
                          t->tm_wday < 7,
                      ESP_ERR_INVALID_ARG, TAG, "time out of range");
  const uint8_t d[7] = {dec2bcd(t->tm_sec),       dec2bcd(t->tm_min),
                        dec2bcd(t->tm_hour),      dec2bcd(t->tm_wday),
                        dec2bcd(t->tm_mday),      dec2bcd(t->tm_mon + 1),
                        dec2bcd(t->tm_year - 100)};
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = write_stopped(REG_SEC, d, sizeof(d));
  xSemaphoreGive(s_lock);
  return err;
}
