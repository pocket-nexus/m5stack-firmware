/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5PM1 driver (m5stack/M5PM1 1.0.7,
 * src/M5PM1.cpp): begin, register access, watchdog, GPIO, ADC and system
 * commands. Register reads and writes retry once after 50 ms, as
 * M5PM1_I2C_RETRY_COUNT/M5PM1_I2C_RETRY_DELAY_MS do; every public function
 * holds one mutex for its whole read-modify-write sequence.
 */
#include "pm1.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "m5p4x_pm1";

#define REG_DEVICE_ID 0x00
#define REG_I2C_CFG 0x09
#define REG_WDT_CNT 0x0A
#define REG_SYS_CMD 0x0C
#define REG_GPIO_MODE 0x10
#define REG_GPIO_OUT 0x11
#define REG_GPIO_IN 0x12
#define REG_GPIO_DRV 0x13
#define REG_GPIO_PUPD0 0x14
#define REG_GPIO_PUPD1 0x15
#define REG_GPIO_FUNC0 0x16
#define REG_GPIO_FUNC1 0x17
#define REG_VBAT_L 0x22
#define REG_VIN_L 0x24
#define REG_5VINOUT_L 0x26

#define SYS_CMD_KEY 0xA0
#define SYS_CMD_SHUTDOWN 0x01
#define SYS_CMD_REBOOT 0x02
#define GPIO_COUNT 5

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

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
  const uint8_t buf[2] = {reg, value};
  esp_err_t err = ESP_FAIL;
  for (int attempt = 0; attempt < RETRY_COUNT; attempt++) {
    err = i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
    if (err == ESP_OK) {
      break;
    }
    if (attempt + 1 < RETRY_COUNT) {
      vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
  }
  return err;
}

static esp_err_t update_unlocked(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t cur = 0;
  ESP_RETURN_ON_ERROR(read_regs(reg, &cur, 1), TAG, "read 0x%02X", reg);
  const uint8_t next = (uint8_t)((cur & ~mask) | (value & mask));
  return next == cur ? ESP_OK : write_reg(reg, next);
}

static esp_err_t lock(void) {
  if (!s_dev) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  return ESP_OK;
}

static void unlock(void) { xSemaphoreGive(s_lock); }

static bool init_device(void) {
  uint8_t id[4] = {0};
  if (read_regs(REG_DEVICE_ID, id, sizeof(id)) != ESP_OK) {
    return false;
  }
  ESP_LOGI(TAG, "ID=0x%02X MODEL=0x%02X HW=0x%02X SW=0x%02X", id[0], id[1],
           id[2], id[3]);
  return true;
}

esp_err_t m5_corep4x_pm1_begin(i2c_master_bus_handle_t bus) {
  ESP_RETURN_ON_FALSE(!s_dev, ESP_ERR_INVALID_STATE, TAG, "already started");
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "no mutex");
  }
  const i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_COREP4X_PM1_ADDR,
      .scl_speed_hz = 100000,
  };
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG,
                      "add device");
  /* An address probe wakes the PM1 from I2C sleep. */
  (void)i2c_master_probe(bus, M5_COREP4X_PM1_ADDR, 10);
  vTaskDelay(pdMS_TO_TICKS(10));
  if (!init_device()) {
    vTaskDelay(pdMS_TO_TICKS(800));
    (void)i2c_master_probe(bus, M5_COREP4X_PM1_ADDR, 10);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!init_device()) {
      i2c_master_bus_rm_device(s_dev);
      s_dev = NULL;
      ESP_LOGE(TAG, "no response at 0x%02X", M5_COREP4X_PM1_ADDR);
      return ESP_ERR_NOT_FOUND;
    }
  }
  /* I2C_CFG: sleep timeout off, 100 kHz (M5PM1::begin). */
  esp_err_t err = write_reg(REG_I2C_CFG, 0x00);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "I2C_CFG write failed: %s", esp_err_to_name(err));
  }
  return ESP_OK;
}

bool m5_corep4x_pm1_ready(void) { return s_dev != NULL; }

esp_err_t m5_corep4x_pm1_read(uint8_t reg, uint8_t *data, size_t len) {
  ESP_RETURN_ON_FALSE(data && len, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = read_regs(reg, data, len);
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_write(uint8_t reg, uint8_t value) {
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = write_reg(reg, value);
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_update(uint8_t reg, uint8_t mask, uint8_t value) {
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = update_unlocked(reg, mask, value);
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_wdt_set(uint8_t timeout_sec) {
  return m5_corep4x_pm1_write(REG_WDT_CNT, timeout_sec);
}

/* Two bits per pin in FUNC0/1 (function 0 = GPIO) and PUPD0/1 (0 = none). */
static esp_err_t pair_bits(uint8_t base, uint8_t pin, uint8_t value) {
  const uint8_t reg = (uint8_t)(base + (pin < 4 ? 0 : 1));
  const uint8_t shift = (uint8_t)((pin < 4 ? pin : pin - 4) * 2);
  return update_unlocked(reg, (uint8_t)(0x03 << shift),
                         (uint8_t)(value << shift));
}

/* Same order as M5PM1::gpioSet: function, mode, output level, pull, drive. */
esp_err_t m5_corep4x_pm1_gpio_output(uint8_t pin, bool open_drain, bool level) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  const uint8_t bit = (uint8_t)(1 << pin);
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = pair_bits(REG_GPIO_FUNC0, pin, 0);
  if (err == ESP_OK) {
    err = update_unlocked(REG_GPIO_MODE, bit, bit);
  }
  if (err == ESP_OK) {
    err = update_unlocked(REG_GPIO_OUT, bit, level ? bit : 0);
  }
  if (err == ESP_OK) {
    err = pair_bits(REG_GPIO_PUPD0, pin, 0);
  }
  if (err == ESP_OK) {
    err = update_unlocked(REG_GPIO_DRV, bit, open_drain ? bit : 0);
  }
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_gpio_hiz(uint8_t pin) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  const uint8_t bit = (uint8_t)(1 << pin);
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = pair_bits(REG_GPIO_FUNC0, pin, 0);
  if (err == ESP_OK) {
    err = update_unlocked(REG_GPIO_MODE, bit, 0);
  }
  if (err == ESP_OK) {
    err = pair_bits(REG_GPIO_PUPD0, pin, 0);
  }
  if (err == ESP_OK) {
    err = update_unlocked(REG_GPIO_DRV, bit, bit);
  }
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_gpio_get(uint8_t pin, bool *level) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT && level, ESP_ERR_INVALID_ARG, TAG,
                      "arg");
  uint8_t in = 0;
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_read(REG_GPIO_IN, &in, 1), TAG, "GPIO_IN");
  *level = (in >> pin) & 1;
  return ESP_OK;
}

esp_err_t m5_corep4x_pm1_gpio_function(uint8_t pin, uint8_t function) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT && function < 4, ESP_ERR_INVALID_ARG,
                      TAG, "arg");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = pair_bits(REG_GPIO_FUNC0, pin, function);
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_gpio_pull(uint8_t pin, uint8_t pull) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT && pull < 3, ESP_ERR_INVALID_ARG, TAG,
                      "arg");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = pair_bits(REG_GPIO_PUPD0, pin, pull);
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_gpio_input(uint8_t pin) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  return m5_corep4x_pm1_update(REG_GPIO_MODE, (uint8_t)(1 << pin), 0);
}

esp_err_t m5_corep4x_pm1_gpio_drive(uint8_t pin, bool open_drain) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  const uint8_t bit = (uint8_t)(1 << pin);
  return m5_corep4x_pm1_update(REG_GPIO_DRV, bit, open_drain ? bit : 0);
}

esp_err_t m5_corep4x_pm1_gpio_wake(uint8_t pin, bool enable, bool rising) {
  /* GPIO1 has no wake function (M5PM1::gpioSetWakeEnable). */
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT && pin != 1, ESP_ERR_INVALID_ARG, TAG,
                      "pin");
  const uint8_t bit = (uint8_t)(1 << pin);
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err =
      update_unlocked(M5_COREP4X_PM1_REG_GPIO_WAKE_EN, bit, enable ? bit : 0);
  if (err == ESP_OK && enable) {
    err = update_unlocked(M5_COREP4X_PM1_REG_GPIO_WAKE_CFG, bit,
                          rising ? bit : 0);
  }
  unlock();
  return err;
}

esp_err_t m5_corep4x_pm1_i2c_sleep(uint8_t seconds) {
  ESP_RETURN_ON_FALSE(seconds <= 15, ESP_ERR_INVALID_ARG, TAG, "seconds");
  ESP_RETURN_ON_ERROR(lock(), TAG, "not started");
  esp_err_t err = update_unlocked(REG_I2C_CFG, 0x0F, seconds);
  uint8_t actual = 0;
  if (err == ESP_OK) {
    err = read_regs(REG_I2C_CFG, &actual, 1);
  }
  unlock();
  if (err == ESP_OK && (actual & 0x0F) != seconds) {
    err = ESP_ERR_INVALID_RESPONSE;
  }
  return err;
}

static esp_err_t read_mv(uint8_t reg, uint16_t *mv) {
  ESP_RETURN_ON_FALSE(mv, ESP_ERR_INVALID_ARG, TAG, "arg");
  uint8_t buf[2] = {0};
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_read(reg, buf, 2), TAG, "read 0x%02X",
                      reg);
  *mv = (uint16_t)(buf[0] | ((buf[1] & 0x0F) << 8));
  return ESP_OK;
}

esp_err_t m5_corep4x_pm1_read_vbat(uint16_t *mv) {
  return read_mv(REG_VBAT_L, mv);
}

esp_err_t m5_corep4x_pm1_read_vin(uint16_t *mv) {
  return read_mv(REG_VIN_L, mv);
}

esp_err_t m5_corep4x_pm1_read_5vinout(uint16_t *mv) {
  return read_mv(REG_5VINOUT_L, mv);
}

static esp_err_t sys_cmd(uint8_t cmd) {
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  vTaskDelay(pdMS_TO_TICKS(120));
  return m5_corep4x_pm1_write(REG_SYS_CMD, (uint8_t)(SYS_CMD_KEY | cmd));
}

esp_err_t m5_corep4x_pm1_shutdown(void) { return sys_cmd(SYS_CMD_SHUTDOWN); }

esp_err_t m5_corep4x_pm1_reboot(void) { return sys_cmd(SYS_CMD_REBOOT); }
