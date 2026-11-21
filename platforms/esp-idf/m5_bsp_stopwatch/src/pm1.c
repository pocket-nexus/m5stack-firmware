/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Register-level subset of the M5PM1 driver (m5stack/M5PM1 1.0.6,
 * src/M5PM1.cpp): begin, I2C sleep, button, watchdog, power hold, charge,
 * LED level, GPIO, ADC and shutdown. Register reads and writes retry once
 * after 50 ms, as M5PM1_I2C_RETRY_COUNT/M5PM1_I2C_RETRY_DELAY_MS do.
 */
#include "pm1.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "m5sw_pm1";

#define REG_DEVICE_ID 0x00
#define REG_DEVICE_MODEL 0x01
#define REG_HW_REV 0x02
#define REG_SW_REV 0x03
#define REG_PWR_CFG 0x06
#define REG_HOLD_CFG 0x07
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
#define REG_BTN_CFG_1 0x49

#define PWR_CFG_CHG_EN (1 << 0)
#define PWR_CFG_LED_CTRL (1 << 4)
#define HOLD_CFG_LDO (1 << 5)
#define I2C_CFG_SLEEP_MASK 0x0F
#define SYS_CMD_KEY 0xA0
#define SYS_CMD_OFF 0x01
#define BTN_CLK_DLY_SHIFT 1
#define BTN_CLICK_DELAY_1000MS 0x03
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

/* Read-modify-write of the bits in mask, under the driver lock. */
static esp_err_t update_reg(uint8_t reg, uint8_t mask, uint8_t value) {
  if (!s_dev) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  uint8_t cur = 0;
  esp_err_t err = read_regs(reg, &cur, 1);
  if (err == ESP_OK) {
    err = write_reg(reg, (uint8_t)((cur & ~mask) | (value & mask)));
  }
  xSemaphoreGive(s_lock);
  return err;
}

static esp_err_t read_locked(uint8_t reg, uint8_t *data, size_t len) {
  if (!s_dev) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = read_regs(reg, data, len);
  xSemaphoreGive(s_lock);
  return err;
}

static bool init_device(void) {
  uint8_t id[4] = {0};
  for (int i = 0; i < 4; i++) {
    if (read_regs((uint8_t)(REG_DEVICE_ID + i), &id[i], 1) != ESP_OK) {
      return false;
    }
  }
  ESP_LOGI(TAG, "ID=0x%02X MODEL=0x%02X HW=0x%02X SW=0x%02X", id[0], id[1],
           id[2], id[3]);
  return true;
}

esp_err_t m5_stopwatch_pm1_begin(i2c_master_bus_handle_t bus) {
  ESP_RETURN_ON_FALSE(!s_dev, ESP_ERR_INVALID_STATE, TAG, "already started");
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "no mutex");
  }
  const i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = M5_STOPWATCH_PM1_ADDR,
      .scl_speed_hz = 100000,
  };
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG,
                      "add device");

  /* An address probe wakes the PM1 from I2C sleep. */
  (void)i2c_master_probe(bus, M5_STOPWATCH_PM1_ADDR, 10);
  vTaskDelay(pdMS_TO_TICKS(10));
  if (!init_device()) {
    vTaskDelay(pdMS_TO_TICKS(800));
    (void)i2c_master_probe(bus, M5_STOPWATCH_PM1_ADDR, 10);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (!init_device()) {
      i2c_master_bus_rm_device(s_dev);
      s_dev = NULL;
      ESP_LOGE(TAG, "no response at 0x%02X", M5_STOPWATCH_PM1_ADDR);
      return ESP_ERR_NOT_FOUND;
    }
  }

  /* I2C_CFG: sleep timeout off, 100 kHz (M5PM1::begin step 4). */
  esp_err_t err = write_reg(REG_I2C_CFG, 0x00);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "I2C_CFG write failed: %s", esp_err_to_name(err));
  }
  return ESP_OK;
}

bool m5_stopwatch_pm1_ready(void) { return s_dev != NULL; }

esp_err_t m5_stopwatch_pm1_set_i2c_sleep_time(uint8_t seconds) {
  ESP_RETURN_ON_FALSE(seconds <= 15, ESP_ERR_INVALID_ARG, TAG, "sleep time");
  ESP_RETURN_ON_ERROR(update_reg(REG_I2C_CFG, I2C_CFG_SLEEP_MASK, seconds), TAG,
                      "I2C_CFG");
  uint8_t actual = 0;
  ESP_RETURN_ON_ERROR(read_locked(REG_I2C_CFG, &actual, 1), TAG, "I2C_CFG");
  return (actual & I2C_CFG_SLEEP_MASK) == seconds ? ESP_OK : ESP_FAIL;
}

esp_err_t m5_stopwatch_pm1_btn_click_delay_1s(void) {
  return update_reg(REG_BTN_CFG_1, 0x03 << BTN_CLK_DLY_SHIFT,
                    BTN_CLICK_DELAY_1000MS << BTN_CLK_DLY_SHIFT);
}

esp_err_t m5_stopwatch_pm1_wdt_set(uint8_t timeout_sec) {
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = write_reg(REG_WDT_CNT, timeout_sec);
  xSemaphoreGive(s_lock);
  return err;
}

esp_err_t m5_stopwatch_pm1_ldo_set_power_hold(bool enable) {
  return update_reg(REG_HOLD_CFG, HOLD_CFG_LDO, enable ? HOLD_CFG_LDO : 0);
}

esp_err_t m5_stopwatch_pm1_set_charge_enable(bool enable) {
  return update_reg(REG_PWR_CFG, PWR_CFG_CHG_EN, enable ? PWR_CFG_CHG_EN : 0);
}

esp_err_t m5_stopwatch_pm1_set_led_en_level(bool level) {
  return update_reg(REG_PWR_CFG, PWR_CFG_LED_CTRL,
                    level ? PWR_CFG_LED_CTRL : 0);
}

static esp_err_t gpio_set_func_gpio(uint8_t pin) {
  const uint8_t reg = pin < 4 ? REG_GPIO_FUNC0 : REG_GPIO_FUNC1;
  const uint8_t shift = (uint8_t)((pin < 4 ? pin : pin - 4) * 2);
  return update_reg(reg, (uint8_t)(0x03 << shift), 0);
}

static esp_err_t gpio_set_pull_none(uint8_t pin) {
  const uint8_t reg = pin < 4 ? REG_GPIO_PUPD0 : REG_GPIO_PUPD1;
  const uint8_t shift = (uint8_t)((pin < 4 ? pin : pin - 4) * 2);
  return update_reg(reg, (uint8_t)(0x03 << shift), 0);
}

/* Same order as M5PM1::gpioSet: function, mode, output level, pull, drive. */
esp_err_t m5_stopwatch_pm1_gpio_output(uint8_t pin, bool level) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  const uint8_t bit = (uint8_t)(1 << pin);
  ESP_RETURN_ON_ERROR(gpio_set_func_gpio(pin), TAG, "GPIO_FUNC");
  ESP_RETURN_ON_ERROR(update_reg(REG_GPIO_MODE, bit, bit), TAG, "GPIO_MODE");
  ESP_RETURN_ON_ERROR(update_reg(REG_GPIO_OUT, bit, level ? bit : 0), TAG,
                      "GPIO_OUT");
  ESP_RETURN_ON_ERROR(gpio_set_pull_none(pin), TAG, "GPIO_PUPD");
  return update_reg(REG_GPIO_DRV, bit, 0);
}

esp_err_t m5_stopwatch_pm1_gpio_input(uint8_t pin) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT, ESP_ERR_INVALID_ARG, TAG, "pin");
  ESP_RETURN_ON_ERROR(gpio_set_func_gpio(pin), TAG, "GPIO_FUNC");
  ESP_RETURN_ON_ERROR(update_reg(REG_GPIO_MODE, (uint8_t)(1 << pin), 0), TAG,
                      "GPIO_MODE");
  return gpio_set_pull_none(pin);
}

esp_err_t m5_stopwatch_pm1_gpio_get_input(uint8_t pin, uint8_t *value) {
  ESP_RETURN_ON_FALSE(pin < GPIO_COUNT && value, ESP_ERR_INVALID_ARG, TAG,
                      "arg");
  uint8_t reg = 0;
  ESP_RETURN_ON_ERROR(read_locked(REG_GPIO_IN, &reg, 1), TAG, "GPIO_IN");
  *value = (reg >> pin) & 0x01;
  return ESP_OK;
}

esp_err_t m5_stopwatch_pm1_set_single_reset_disable(bool disable) {
  return update_reg(REG_BTN_CFG_1, 0x01, disable ? 0x01 : 0);
}

static esp_err_t read_u16(uint8_t reg, uint16_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  uint8_t buf[2] = {0};
  ESP_RETURN_ON_ERROR(read_locked(reg, buf, 2), TAG, "read 0x%02X", reg);
  *out = (uint16_t)(buf[0] | (buf[1] << 8)); /* little-endian, mV */
  return ESP_OK;
}

esp_err_t m5_stopwatch_pm1_read_vbat(uint16_t *mv) {
  return read_u16(REG_VBAT_L, mv);
}

esp_err_t m5_stopwatch_pm1_read_vin(uint16_t *mv) {
  return read_u16(REG_VIN_L, mv);
}

esp_err_t m5_stopwatch_pm1_shutdown(void) {
  ESP_RETURN_ON_FALSE(s_dev, ESP_ERR_INVALID_STATE, TAG, "not started");
  /* M5PM1::sysCmd waits 120 ms before writing the command. */
  vTaskDelay(pdMS_TO_TICKS(120));
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = write_reg(REG_SYS_CMD, SYS_CMD_KEY | SYS_CMD_OFF);
  xSemaphoreGive(s_lock);
  return err;
}
