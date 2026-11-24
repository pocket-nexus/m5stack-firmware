/*
 * SPDX-FileCopyrightText: 2025 ESP-IDF Community
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* RX8130CE driver after otool_ic_rx8130/rx8130_tools.cpp of
 * M5CoreP4X-UserDemo (init, initBat, clearIrqFlags, disableIrq, setTime,
 * getTime, setTimerIrq) and the RTC steps of m5_corep4_tool's
 * sleep_before_work. */
#include "m5_corep4x/rtc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "m5_corep4x/bsp.h"
#include "rtc_regs.h"
#include <string.h>

static const char *TAG = "m5p4x_rtc";

#define I2C_TIMEOUT_MS 100

static i2c_master_dev_handle_t s_dev;
static bool s_ready;
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buffer;
static portMUX_TYPE s_lock_init = portMUX_INITIALIZER_UNLOCKED;

static void lock(void) {
  if (!s_lock) {
    taskENTER_CRITICAL(&s_lock_init);
    if (!s_lock) {
      s_lock = xSemaphoreCreateMutexStatic(&s_lock_buffer);
    }
    taskEXIT_CRITICAL(&s_lock_init);
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void) { xSemaphoreGive(s_lock); }

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t write_regs(uint8_t reg, const uint8_t *data, size_t len) {
  uint8_t buf[1 + RX8130_TIME_REGS];
  if (len > RX8130_TIME_REGS) {
    return ESP_ERR_INVALID_SIZE;
  }
  buf[0] = reg;
  memcpy(buf + 1, data, len);
  return i2c_master_transmit(s_dev, buf, len + 1, I2C_TIMEOUT_MS);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
  return write_regs(reg, &value, 1);
}

/* initBat and clearIrqFlags. */
static esp_err_t start_device(void) {
  uint8_t ctrl1 = 0;
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_CTRL1, &ctrl1, 1), TAG, "CTRL1");
  ctrl1 |= RX8130_CTRL1_INIEN | RX8130_CTRL1_CHGEN;
  ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_CTRL1, ctrl1), TAG, "CTRL1");
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_CTRL1, &ctrl1, 1), TAG, "CTRL1");
  ESP_LOGI(TAG, "RX8130CE CTRL1 0x%02X", ctrl1);
  return write_reg(RX8130_REG_FLAG, 0);
}

esp_err_t m5_corep4x_rtc_init(void) {
  ESP_RETURN_ON_FALSE(m5_corep4x_i2c(), ESP_ERR_INVALID_STATE, TAG, "no bus");
  lock();
  esp_err_t err = ESP_OK;
  if (s_ready) {
    goto out;
  }
  if (!s_dev) {
    err = i2c_master_probe(m5_corep4x_i2c(), M5_COREP4X_RTC_ADDRESS, 50);
    if (err != ESP_OK) {
      err = ESP_ERR_NOT_FOUND;
      goto out;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = M5_COREP4X_RTC_ADDRESS,
        .scl_speed_hz = 400000,
    };
    err = i2c_master_bus_add_device(m5_corep4x_i2c(), &cfg, &s_dev);
    if (err != ESP_OK) {
      s_dev = NULL;
      goto out;
    }
  }
  err = start_device();
  s_ready = err == ESP_OK;
out:
  unlock();
  return err;
}

bool m5_corep4x_rtc_ready(void) { return s_ready; }

esp_err_t m5_corep4x_rtc_get(struct tm *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "not started");
  uint8_t regs[RX8130_TIME_REGS];
  lock();
  const esp_err_t err = read_regs(RX8130_REG_SEC, regs, sizeof(regs));
  unlock();
  if (err == ESP_OK) {
    memset(out, 0, sizeof(*out));
    rx8130_decode(regs, out);
  }
  return err;
}

/* The clock fields setTime compares (getCurrentTime before and after);
 * the weekday is not part of mktime's result. */
static bool time_changed(const uint8_t *before, const uint8_t *after) {
  for (size_t i = 0; i < RX8130_TIME_REGS; i++) {
    if (i != 3 && before[i] != after[i]) {
      return true;
    }
  }
  return false;
}

static esp_err_t set_locked(const struct tm *time) {
  uint8_t before[RX8130_TIME_REGS];
  uint8_t regs[RX8130_TIME_REGS];
  uint8_t after[RX8130_TIME_REGS];
  uint8_t ctrl0 = 0;
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_SEC, before, sizeof(before)), TAG,
                      "time");
  rx8130_encode(time, regs);
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_CTRL0, &ctrl0, 1), TAG, "CTRL0");
  ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_CTRL0, ctrl0 | RX8130_CTRL0_STOP),
                      TAG, "STOP");
  ESP_RETURN_ON_ERROR(write_regs(RX8130_REG_SEC, regs, sizeof(regs)), TAG,
                      "time");
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_CTRL0, &ctrl0, 1), TAG, "CTRL0");
  ESP_RETURN_ON_ERROR(
      write_reg(RX8130_REG_CTRL0, ctrl0 & (uint8_t)~RX8130_CTRL0_STOP), TAG,
      "STOP");
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_SEC, after, sizeof(after)), TAG,
                      "time");
  if (time_changed(before, after)) {
    /* recalculateAllTasks -> updateNextAlarm -> disableIrq */
    ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_CTRL0, 0), TAG, "CTRL0");
  }
  return ESP_OK;
}

esp_err_t m5_corep4x_rtc_set(const struct tm *time) {
  ESP_RETURN_ON_FALSE(time && time->tm_year >= 100 && time->tm_year < 200,
                      ESP_ERR_INVALID_ARG, TAG, "year");
  ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "not started");
  lock();
  const esp_err_t err = set_locked(time);
  unlock();
  return err;
}

static esp_err_t wake_timer_locked(uint32_t seconds) {
  if (seconds == 0) {
    ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_CTRL0, 0), TAG, "CTRL0");
    return write_reg(RX8130_REG_FLAG, 0);
  }
  const uint16_t count = rx8130_timer_count(seconds);
  const uint8_t counter[2] = {(uint8_t)(count & 0xFF), (uint8_t)(count >> 8)};
  uint8_t value = 0;
  ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_FLAG, 0), TAG, "FLAG");
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_EXT, &value, 1), TAG, "EXT");
  ESP_RETURN_ON_ERROR(
      write_reg(RX8130_REG_EXT, value & (uint8_t)~RX8130_EXT_TE), TAG, "EXT");
  ESP_RETURN_ON_ERROR(write_regs(RX8130_REG_TCOUNT0, counter, sizeof(counter)),
                      TAG, "TCOUNT");
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_EXT, &value, 1), TAG, "EXT");
  ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_EXT, rx8130_ext_timer_on(value)),
                      TAG, "EXT");
  ESP_RETURN_ON_ERROR(read_regs(RX8130_REG_CTRL0, &value, 1), TAG, "CTRL0");
  ESP_RETURN_ON_ERROR(write_reg(RX8130_REG_CTRL0, value | RX8130_CTRL0_TIE),
                      TAG, "CTRL0");
  ESP_LOGI(TAG, "timer interrupt after %u s", (unsigned)count);
  return ESP_OK;
}

esp_err_t m5_corep4x_rtc_wake_timer(uint32_t seconds) {
  ESP_RETURN_ON_ERROR(m5_corep4x_rtc_init(), TAG, "RX8130CE");
  lock();
  const esp_err_t err = wake_timer_locked(seconds);
  unlock();
  return err;
}
