/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5IOE1 driver (m5stack/M5IOE1 1.0.8,
 * src/M5IOE1.cpp): begin, pinMode(OUTPUT), digitalWrite, digitalRead,
 * setPwmFrequency and setPwmDuty. Every public function holds one mutex for
 * its whole read-modify-write sequence.
 */
#include "ioe1.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "m5sw_ioe1";

#define REG_UID_L 0x00
#define REG_REV 0x02
#define REG_GPIO_MODE_L 0x03
#define REG_GPIO_OUT_L 0x05
#define REG_GPIO_IN_L 0x07
#define REG_GPIO_PU_L 0x09
#define REG_GPIO_PD_L 0x0B
#define REG_GPIO_DRV_L 0x13
#define REG_PWM1_DUTY_L 0x1B
#define REG_I2C_CFG 0x23
#define REG_PWM_FREQ_L 0x25

#define PWM_POLARITY (1 << 6)
#define PWM_ENABLE (1 << 7)
#define I2C_CFG_SPEED_400K (1 << 4)
#define PIN_COUNT 14

#define RETRY_COUNT 2
#define RETRY_DELAY_MS 50

static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_lock;

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len) {
  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
    err = i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
    if (err == ESP_OK) {
      break;
    }
    if (attempt + 1 < RETRY_COUNT) {
      vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
  }
  return err;
}

static esp_err_t write_regs(uint8_t reg, const uint8_t *data, size_t len) {
  uint8_t buf[3];
  if (len > sizeof(buf) - 1) {
    return ESP_ERR_INVALID_SIZE;
  }
  buf[0] = reg;
  for (size_t i = 0; i < len; i++) {
    buf[i + 1] = data[i];
  }
  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
    err = i2c_master_transmit(s_dev, buf, len + 1, 100);
    if (err == ESP_OK) {
      break;
    }
    if (attempt + 1 < RETRY_COUNT) {
      vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
  }
  return err;
}

static esp_err_t read16(uint8_t reg, uint16_t *value) {
  uint8_t buf[2] = {0};
  esp_err_t err = read_regs(reg, buf, 2);
  *value = (uint16_t)(buf[0] | (buf[1] << 8));
  return err;
}

static esp_err_t write16(uint8_t reg, uint16_t value) {
  const uint8_t buf[2] = {(uint8_t)(value & 0xFF), (uint8_t)(value >> 8)};
  return write_regs(reg, buf, 2);
}

static esp_err_t write8(uint8_t reg, uint8_t value) {
  return write_regs(reg, &value, 1);
}

static bool init_device(void) {
  uint16_t uid = 0;
  uint8_t rev = 0;
  if (read16(REG_UID_L, &uid) != ESP_OK ||
      read_regs(REG_REV, &rev, 1) != ESP_OK) {
    return false;
  }
  ESP_LOGI(TAG, "UID=0x%04X FW=0x%02X", uid, rev);
  return true;
}

static esp_err_t add_device(i2c_master_bus_handle_t bus, uint8_t addr,
                            uint32_t hz) {
  const i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = addr,
      .scl_speed_hz = hz,
  };
  return i2c_master_bus_add_device(bus, &cfg, &s_dev);
}

esp_err_t m5_stopwatch_ioe1_begin(i2c_master_bus_handle_t bus, uint8_t addr) {
  ESP_RETURN_ON_FALSE(!s_dev, ESP_ERR_INVALID_STATE, TAG, "already started");
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "no mutex");
  }
  ESP_RETURN_ON_ERROR(add_device(bus, addr, 100000), TAG, "add device");

  /* The device boots at 100 kHz; an address probe wakes it from I2C sleep. */
  (void)i2c_master_probe(bus, addr, 10);
  vTaskDelay(pdMS_TO_TICKS(10));
  if (!init_device()) {
    vTaskDelay(pdMS_TO_TICKS(800));
    (void)i2c_master_probe(bus, addr, 10);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!init_device()) {
      i2c_master_bus_rm_device(s_dev);
      s_dev = NULL;
      return ESP_ERR_NOT_FOUND;
    }
  }

  /* I2C_CFG: sleep off, 400 kHz, wake on falling edge, INT pull enabled. */
  esp_err_t err = write8(REG_I2C_CFG, I2C_CFG_SPEED_400K);
  uint8_t actual = 0;
  if (err == ESP_OK) {
    err = read_regs(REG_I2C_CFG, &actual, 1);
  }
  if (err != ESP_OK || actual != I2C_CFG_SPEED_400K) {
    /* The device stays at 100 kHz, as M5IOE1::begin does on this failure. */
    ESP_LOGW(TAG, "I2C_CFG not applied (0x%02X)", actual);
    return ESP_OK;
  }
  i2c_master_bus_rm_device(s_dev);
  s_dev = NULL;
  ESP_RETURN_ON_ERROR(add_device(bus, addr, 400000), TAG, "re-add device");
  return ESP_OK;
}

bool m5_stopwatch_ioe1_ready(void) { return s_dev != NULL; }

static esp_err_t lock(void) {
  if (!s_dev) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  return ESP_OK;
}

static void unlock(void) { xSemaphoreGive(s_lock); }

/* PWM channel of a PWM-capable pin (IO8..IO11), or -1. */
static int pwm_channel_of(uint8_t pin) {
  switch (pin) {
  case 8:
    return 0;
  case 7:
    return 1;
  case 10:
    return 2;
  case 9:
    return 3;
  default:
    return -1;
  }
}

static uint8_t pin_of_pwm_channel(uint8_t channel) {
  static const uint8_t pins[4] = {8, 7, 10, 9};
  return pins[channel];
}

static esp_err_t pin_mode_output_locked(uint8_t pin) {
  const uint16_t bit = (uint16_t)(1u << pin);
  uint16_t mode = 0, pu = 0, pd = 0, drv = 0;
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_MODE_L, &mode), TAG, "GPIO_MODE");
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_PU_L, &pu), TAG, "GPIO_PU");
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_PD_L, &pd), TAG, "GPIO_PD");
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_DRV_L, &drv), TAG, "GPIO_DRV");

  const int ch = pwm_channel_of(pin);
  if (ch >= 0) {
    const uint8_t reg = (uint8_t)(REG_PWM1_DUTY_L + ch * 2);
    uint16_t pwm = 0;
    if (read16(reg, &pwm) == ESP_OK && (pwm & (PWM_ENABLE << 8))) {
      (void)write16(reg, (uint16_t)(pwm & ~(PWM_ENABLE << 8)));
    }
  }

  mode |= bit;
  pu &= (uint16_t)~bit;
  pd &= (uint16_t)~bit;
  drv &= (uint16_t)~bit; /* push-pull */
  ESP_RETURN_ON_ERROR(write16(REG_GPIO_PU_L, pu), TAG, "GPIO_PU");
  ESP_RETURN_ON_ERROR(write16(REG_GPIO_PD_L, pd), TAG, "GPIO_PD");
  ESP_RETURN_ON_ERROR(write16(REG_GPIO_DRV_L, drv), TAG, "GPIO_DRV");
  ESP_RETURN_ON_ERROR(write16(REG_GPIO_MODE_L, mode), TAG, "GPIO_MODE");

  uint16_t actual = 0;
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_MODE_L, &actual), TAG, "GPIO_MODE");
  return (actual & bit) ? ESP_OK : ESP_FAIL;
}

esp_err_t m5_stopwatch_ioe1_pin_mode_output(uint8_t pin) {
  ESP_RETURN_ON_FALSE(pin < PIN_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = pin_mode_output_locked(pin);
  unlock();
  return err;
}

static esp_err_t digital_write_locked(uint8_t pin, bool level) {
  const uint16_t bit = (uint16_t)(1u << pin);
  uint16_t out = 0;
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_OUT_L, &out), TAG, "GPIO_OUT");
  out = level ? (uint16_t)(out | bit) : (uint16_t)(out & ~bit);
  ESP_RETURN_ON_ERROR(write16(REG_GPIO_OUT_L, out), TAG, "GPIO_OUT");
  uint16_t actual = 0;
  ESP_RETURN_ON_ERROR(read16(REG_GPIO_OUT_L, &actual), TAG, "GPIO_OUT");
  return (actual & bit) == (out & bit) ? ESP_OK : ESP_FAIL;
}

esp_err_t m5_stopwatch_ioe1_digital_write(uint8_t pin, bool level) {
  ESP_RETURN_ON_FALSE(pin < PIN_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = digital_write_locked(pin, level);
  unlock();
  return err;
}

esp_err_t m5_stopwatch_ioe1_digital_read(uint8_t pin, bool *level) {
  ESP_RETURN_ON_FALSE(pin < PIN_COUNT && level, ESP_ERR_INVALID_ARG, TAG,
                      "arg");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  uint16_t in = 0;
  esp_err_t err = read16(REG_GPIO_IN_L, &in);
  unlock();
  *level = (in >> pin) & 1;
  return err;
}

esp_err_t m5_stopwatch_ioe1_set_pwm_frequency(uint16_t hz) {
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = write16(REG_PWM_FREQ_L, hz);
  uint16_t actual = 0;
  if (err == ESP_OK) {
    err = read16(REG_PWM_FREQ_L, &actual);
  }
  unlock();
  if (err == ESP_OK && actual != hz) {
    err = ESP_FAIL;
  }
  return err;
}

static esp_err_t set_pwm_locked(uint8_t channel, uint16_t duty12, bool polarity,
                                bool enable) {
  const uint8_t reg = (uint8_t)(REG_PWM1_DUTY_L + channel * 2);
  uint8_t high = (uint8_t)((duty12 >> 8) & 0x0F);
  if (polarity) {
    high |= PWM_POLARITY;
  }
  if (enable) {
    high |= PWM_ENABLE;
  }
  const uint8_t buf[2] = {(uint8_t)(duty12 & 0xFF), high};
  ESP_RETURN_ON_ERROR(write_regs(reg, buf, 2), TAG, "PWM duty");
  uint16_t actual = 0;
  ESP_RETURN_ON_ERROR(read16(reg, &actual), TAG, "PWM duty");
  if (actual != (uint16_t)(buf[0] | (buf[1] << 8))) {
    return ESP_FAIL;
  }
  if (enable) {
    /* The PWM pin must be an output. */
    uint16_t mode = 0;
    if (read16(REG_GPIO_MODE_L, &mode) == ESP_OK) {
      const uint16_t bit = (uint16_t)(1u << pin_of_pwm_channel(channel));
      if (!(mode & bit) && write16(REG_GPIO_MODE_L, mode | bit) != ESP_OK) {
        ESP_LOGW(TAG, "PWM channel %u pin not set to output", channel);
      }
    }
  }
  return ESP_OK;
}

esp_err_t m5_stopwatch_ioe1_set_pwm_duty(uint8_t channel, uint8_t duty,
                                         bool polarity, bool enable) {
  ESP_RETURN_ON_FALSE(channel < 4 && duty <= 100, ESP_ERR_INVALID_ARG, TAG,
                      "arg");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  const uint16_t duty12 = (uint16_t)((duty * 0x0FFFu) / 100u);
  esp_err_t err = set_pwm_locked(channel, duty12, polarity, enable);
  unlock();
  return err;
}
