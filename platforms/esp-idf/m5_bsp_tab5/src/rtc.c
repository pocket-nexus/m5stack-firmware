/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Epson RX8130CE, ported to C from main/hal/utils/rx8130/rx8130.cpp of
 * M5Tab5-UserDemo (begin, initBat, setTime, getTime, clearIrqFlags,
 * disableIrq, setAlarmIrq). Datasheet:
 * https://download.epsondevice.com/td/pdf/app/RX8130CE_en.pdf */
#include "esp_check.h"
#include "m5/tab5.h"
#include <string.h>

static const char *TAG = "m5_tab5_rtc";

#define RX8130_ADDRESS 0x32
#define RX8130_REG_SEC 0x10
#define RX8130_REG_ALMIN 0x17
#define RX8130_REG_ALHOUR 0x18
#define RX8130_REG_ALWDAY 0x19
#define RX8130_REG_FLAG 0x1D
#define RX8130_REG_CTRL0 0x1E
#define RX8130_REG_CTRL1 0x1F
#define RX8130_BIT_CTRL_AIE (1u << 3)
#define RX8130_BIT_CTRL_UIE (1u << 5)
#define RX8130_BIT_CTRL_STOP (1u << 6)
#define I2C_TIMEOUT_MS 50

static i2c_master_dev_handle_t device;

static uint8_t bcd2dec(uint8_t value) {
  return (uint8_t)((value >> 4) * 10 + (value & 0x0f));
}

static uint8_t dec2bcd(int value) {
  return (uint8_t)(((value / 10) << 4) + (value % 10));
}

static esp_err_t read_registers(uint8_t reg, uint8_t *buffer, size_t length) {
  return i2c_master_transmit_receive(device, &reg, 1, buffer, length,
                                     I2C_TIMEOUT_MS);
}

static esp_err_t write_registers(uint8_t reg, const uint8_t *buffer,
                                 size_t length) {
  uint8_t frame[8];
  if (length + 1 > sizeof(frame))
    return ESP_ERR_INVALID_SIZE;
  frame[0] = reg;
  memcpy(frame + 1, buffer, length);
  return i2c_master_transmit(device, frame, length + 1, I2C_TIMEOUT_MS);
}

static esp_err_t read8(uint8_t reg, uint8_t *value) {
  return read_registers(reg, value, 1);
}

static esp_err_t write8(uint8_t reg, uint8_t value) {
  return write_registers(reg, &value, 1);
}

esp_err_t m5_tab5_rtc_init(void) {
  if (device)
    return ESP_OK;
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  const i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = RX8130_ADDRESS,
      .scl_speed_hz = 400000,
  };
  ESP_RETURN_ON_ERROR(
      i2c_master_bus_add_device(m5_tab5_i2c(), &config, &device), TAG,
      "RX8130CE");
  /* initBat: Control 1 bits 4 and 5 switch the backup battery in. */
  uint8_t control = 0;
  ESP_RETURN_ON_ERROR(read8(RX8130_REG_CTRL1, &control), TAG, "control 1");
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_CTRL1, control | 0x30), TAG,
                      "battery switch");
  return m5_tab5_rtc_clear_irq();
}

esp_err_t m5_tab5_rtc_clear_irq(void) {
  ESP_RETURN_ON_FALSE(device, ESP_ERR_INVALID_STATE, TAG, "not initialized");
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_FLAG, 0), TAG, "flags");
  return write8(RX8130_REG_CTRL0, 0);
}

esp_err_t m5_tab5_rtc_get(struct tm *out) {
  ESP_RETURN_ON_FALSE(device && out, ESP_ERR_INVALID_STATE, TAG, "arguments");
  uint8_t date[7];
  ESP_RETURN_ON_ERROR(read_registers(RX8130_REG_SEC, date, sizeof(date)), TAG,
                      "read");
  memset(out, 0, sizeof(*out));
  out->tm_sec = bcd2dec(date[0] & 0x7f);
  out->tm_min = bcd2dec(date[1] & 0x7f);
  out->tm_hour = bcd2dec(date[2] & 0x3f);
  out->tm_wday = bcd2dec(date[3] & 0x7f);
  out->tm_mday = bcd2dec(date[4] & 0x3f);
  out->tm_mon = bcd2dec(date[5] & 0x1f);
  out->tm_year = bcd2dec(date[6]) + 100;
  return ESP_OK;
}

esp_err_t m5_tab5_rtc_set(const struct tm *time) {
  ESP_RETURN_ON_FALSE(device && time, ESP_ERR_INVALID_STATE, TAG, "arguments");
  uint8_t control = 0;
  /* STOP holds the clock while the calendar changes. */
  ESP_RETURN_ON_ERROR(read8(RX8130_REG_CTRL0, &control), TAG, "control 0");
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_CTRL0, control | RX8130_BIT_CTRL_STOP),
                      TAG, "stop");
  const uint8_t date[7] = {
      dec2bcd(time->tm_sec),
      dec2bcd(time->tm_min),
      dec2bcd(time->tm_hour),
      dec2bcd(time->tm_wday),
      dec2bcd(time->tm_mday),
      dec2bcd(time->tm_mon),
      dec2bcd((time->tm_year - 100) % 100),
  };
  ESP_RETURN_ON_ERROR(write_registers(RX8130_REG_SEC, date, sizeof(date)), TAG,
                      "write");
  ESP_RETURN_ON_ERROR(read8(RX8130_REG_CTRL0, &control), TAG, "control 0");
  return write8(RX8130_REG_CTRL0, control & ~RX8130_BIT_CTRL_STOP);
}

/* setAlarmIrq: AE set in the minute, hour and weekday alarm registers, so the
 * alarm fires on the next minute boundary; AIE drives /INT. `time` is unused
 * by the factory sequence and kept for a future minute match. */
esp_err_t m5_tab5_rtc_set_alarm(const struct tm *time) {
  (void)time;
  ESP_RETURN_ON_FALSE(device, ESP_ERR_INVALID_STATE, TAG, "not initialized");
  uint8_t control = 0;
  ESP_RETURN_ON_ERROR(read8(RX8130_REG_CTRL0, &control), TAG, "control 0");
  control &= (uint8_t)~(RX8130_BIT_CTRL_AIE | RX8130_BIT_CTRL_UIE);
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_CTRL0, control), TAG, "AIE off");
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_ALWDAY, 0x80), TAG, "weekday alarm");
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_ALHOUR, 0x80), TAG, "hour alarm");
  ESP_RETURN_ON_ERROR(write8(RX8130_REG_ALMIN, 0x80), TAG, "minute alarm");
  ESP_RETURN_ON_ERROR(read8(RX8130_REG_CTRL0, &control), TAG, "control 0");
  return write8(RX8130_REG_CTRL0, control | RX8130_BIT_CTRL_AIE);
}
