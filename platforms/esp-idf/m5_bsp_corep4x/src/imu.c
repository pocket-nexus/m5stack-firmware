/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* LSM6DS3TR-C driver after otool_lsm6ds3trc/lsm6ds3trc_tools.cpp and
 * m5_corep4_tool::imu_init / imu_prepare_for_sleep of M5CoreP4X-UserDemo. */
#include "m5_corep4x/imu.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "m5_corep4x/bsp.h"
#include "pm1.h"

static const char *TAG = "m5p4x_imu";

#define ADDR 0x6A
#define ADDR_ALT 0x6B
#define CHIP_ID 0x6A

#define REG_INT1_CTRL 0x0D
#define REG_WHO_AM_I 0x0F
#define REG_CTRL1_XL 0x10
#define REG_CTRL2_G 0x11
#define REG_CTRL3_C 0x12
#define REG_WAKE_UP_SRC 0x1B
#define REG_STATUS 0x1E
#define REG_OUTX_L_G 0x22
#define REG_TAP_CFG 0x58
#define REG_WAKE_UP_THS 0x5B
#define REG_WAKE_UP_DUR 0x5C
#define REG_MD1_CFG 0x5E

#define CTRL3_SW_RESET 0x01
#define CTRL3_IF_INC 0x04
#define CTRL3_PP_OD 0x10
#define CTRL3_H_LACTIVE 0x20
#define CTRL3_BDU 0x40
#define CTRL1_XL_104HZ_2G 0x40
#define CTRL2_G_104HZ_2000DPS 0x4C
#define TAP_CFG_INTERRUPTS_ENABLE 0x80
#define TAP_CFG_LIR 0x01
#define MD_CFG_WAKE_UP 0x20

#define ACC_MG_PER_LSB 0.061f
#define GYR_MDPS_PER_LSB 70.0f

/* The device the register helpers address; `s_ready` turns true only once
 * it is identified and configured, so readers never see a half-initialised
 * sensor. Init and sleep preparation hold `s_lock`. */
static i2c_master_dev_handle_t s_dev;
static volatile bool s_ready;
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buffer;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

static void lock(void) {
  taskENTER_CRITICAL(&s_lock_mux);
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutexStatic(&s_lock_buffer);
  }
  taskEXIT_CRITICAL(&s_lock_mux);
  xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void) { xSemaphoreGive(s_lock); }

static esp_err_t read_regs(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
  const uint8_t buf[2] = {reg, value};
  return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t add_device(uint8_t addr) {
  const i2c_device_config_t cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = addr,
      .scl_speed_hz = 400000,
  };
  return i2c_master_bus_add_device(m5_corep4x_i2c(), &cfg, &s_dev);
}

/* select_device: 0x6A, then 0x6B, accepting (WHO_AM_I & 0x7F) == 0x6A. */
static esp_err_t select_device(void) {
  static const uint8_t addrs[] = {ADDR, ADDR_ALT};
  esp_err_t err = ESP_ERR_NOT_FOUND;
  for (size_t i = 0; i < sizeof(addrs); i++) {
    if (i2c_master_probe(m5_corep4x_i2c(), addrs[i], 50) != ESP_OK) {
      continue;
    }
    ESP_RETURN_ON_ERROR(add_device(addrs[i]), TAG, "add device");
    uint8_t id = 0;
    err = read_regs(REG_WHO_AM_I, &id, 1);
    if (err == ESP_OK && (id & 0x7F) == CHIP_ID) {
      ESP_LOGI(TAG, "LSM6DS3TR-C at 0x%02X", addrs[i]);
      return ESP_OK;
    }
    if (err == ESP_OK) {
      err = ESP_ERR_NOT_FOUND;
    }
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
  }
  return err;
}

static esp_err_t configure(void) {
  ESP_RETURN_ON_ERROR(write_reg(REG_CTRL3_C, CTRL3_SW_RESET), TAG, "reset");
  const int64_t start = esp_timer_get_time();
  for (;;) {
    uint8_t ctrl3 = 0;
    ESP_RETURN_ON_ERROR(read_regs(REG_CTRL3_C, &ctrl3, 1), TAG, "CTRL3_C");
    if (!(ctrl3 & CTRL3_SW_RESET)) {
      break;
    }
    if (esp_timer_get_time() - start > 100000) {
      return ESP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  ESP_RETURN_ON_ERROR(write_reg(REG_CTRL3_C, CTRL3_BDU | CTRL3_IF_INC), TAG,
                      "CTRL3_C");
  ESP_RETURN_ON_ERROR(write_reg(REG_CTRL1_XL, CTRL1_XL_104HZ_2G), TAG,
                      "CTRL1_XL");
  return write_reg(REG_CTRL2_G, CTRL2_G_104HZ_2000DPS);
}

static esp_err_t init_locked(void) {
  if (s_ready) {
    return ESP_OK;
  }
  ESP_RETURN_ON_FALSE(m5_corep4x_i2c(), ESP_ERR_INVALID_STATE, TAG, "no bus");
  if (m5_corep4x_set_switch(M5_COREP4X_SWITCH_L3B, true) != ESP_OK) {
    ESP_LOGW(TAG, "L3B on failed");
  }
  vTaskDelay(pdMS_TO_TICKS(50));
  ESP_RETURN_ON_ERROR(select_device(), TAG, "no LSM6DS3TR-C");
  const esp_err_t err = configure();
  if (err != ESP_OK) {
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return err;
  }
  s_ready = true;
  return ESP_OK;
}

esp_err_t m5_corep4x_imu_init(void) {
  lock();
  const esp_err_t err = init_locked();
  unlock();
  return err;
}

bool m5_corep4x_imu_ready(void) { return s_ready; }

esp_err_t m5_corep4x_imu_read(m5_corep4x_imu_sample_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "not started");
  *out = (m5_corep4x_imu_sample_t){.time_ms = esp_timer_get_time() / 1000};
  uint8_t status = 0;
  ESP_RETURN_ON_ERROR(read_regs(REG_STATUS, &status, 1), TAG, "STATUS");
  out->acc_valid = status & 0x01;
  out->gyr_valid = status & 0x02;
  if (!out->acc_valid && !out->gyr_valid) {
    return ESP_OK;
  }
  uint8_t raw[12];
  ESP_RETURN_ON_ERROR(read_regs(REG_OUTX_L_G, raw, sizeof(raw)), TAG,
                      "outputs");
  int16_t v[6];
  for (int i = 0; i < 6; i++) {
    v[i] = (int16_t)(raw[2 * i] | (raw[2 * i + 1] << 8));
  }
  out->gx = (float)v[0] * GYR_MDPS_PER_LSB / 1000.0f;
  out->gy = (float)v[1] * GYR_MDPS_PER_LSB / 1000.0f;
  out->gz = (float)v[2] * GYR_MDPS_PER_LSB / 1000.0f;
  out->ax = (float)v[3] * ACC_MG_PER_LSB / 1000.0f;
  out->ay = (float)v[4] * ACC_MG_PER_LSB / 1000.0f;
  out->az = (float)v[5] * ACC_MG_PER_LSB / 1000.0f;
  return ESP_OK;
}

/* configure_motion_interrupt(INT1, active low, open drain, latched). */
static esp_err_t motion_interrupt(void) {
  ESP_RETURN_ON_ERROR(write_reg(REG_CTRL3_C, CTRL3_BDU | CTRL3_IF_INC |
                                                 CTRL3_H_LACTIVE | CTRL3_PP_OD),
                      TAG, "CTRL3_C");
  ESP_RETURN_ON_ERROR(
      write_reg(REG_TAP_CFG, TAP_CFG_INTERRUPTS_ENABLE | TAP_CFG_LIR), TAG,
      "TAP_CFG");
  ESP_RETURN_ON_ERROR(write_reg(REG_WAKE_UP_THS, 0x04), TAG, "WAKE_UP_THS");
  uint8_t dur = 0;
  ESP_RETURN_ON_ERROR(read_regs(REG_WAKE_UP_DUR, &dur, 1), TAG, "WAKE_UP_DUR");
  ESP_RETURN_ON_ERROR(
      write_reg(REG_WAKE_UP_DUR, (uint8_t)((dur & 0xFC) | 0x01)), TAG,
      "WAKE_UP_DUR");
  ESP_RETURN_ON_ERROR(write_reg(REG_INT1_CTRL, 0), TAG, "INT1_CTRL");
  ESP_RETURN_ON_ERROR(write_reg(REG_MD1_CFG, MD_CFG_WAKE_UP), TAG, "MD1_CFG");
  /* Reading the sources clears the latched flags. */
  uint8_t status = 0;
  ESP_RETURN_ON_ERROR(read_regs(REG_WAKE_UP_SRC, &status, 1), TAG,
                      "WAKE_UP_SRC");
  return read_regs(REG_STATUS, &status, 1);
}

static esp_err_t prepare_sleep_locked(bool motion_wake) {
  esp_err_t first = ESP_OK;
  if (motion_wake) {
    if (!s_ready) {
      m5_corep4x_set_switch(M5_COREP4X_SWITCH_L3B, true);
      vTaskDelay(pdMS_TO_TICKS(20));
      first = init_locked();
    }
    if (s_ready) {
      esp_err_t err = write_reg(REG_CTRL1_XL, CTRL1_XL_104HZ_2G);
      if (err == ESP_OK) {
        err = write_reg(REG_CTRL2_G, 0);
      }
      if (err == ESP_OK) {
        err = motion_interrupt();
      }
      if (first == ESP_OK) {
        first = err;
      }
    }
    const esp_err_t pm1[] = {
        m5_corep4x_pm1_gpio_function(M5_COREP4X_PM1_IMU_INT,
                                     M5_COREP4X_PM1_FUNC_GPIO),
        m5_corep4x_pm1_gpio_input(M5_COREP4X_PM1_IMU_INT),
        m5_corep4x_pm1_gpio_pull(M5_COREP4X_PM1_IMU_INT, 1),
        m5_corep4x_pm1_gpio_wake(M5_COREP4X_PM1_IMU_INT, true, false),
        m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_IRQ_MASK1,
                              1 << M5_COREP4X_PM1_IMU_INT, 0),
    };
    for (size_t i = 0; i < sizeof(pm1) / sizeof(pm1[0]); i++) {
      if (first == ESP_OK && pm1[i] != ESP_OK) {
        first = pm1[i];
      }
    }
    return first;
  }
  if (s_ready) {
    esp_err_t err = write_reg(REG_CTRL1_XL, 0);
    if (err == ESP_OK) {
      err = write_reg(REG_CTRL2_G, 0);
    }
    if (err == ESP_OK) {
      err = write_reg(REG_INT1_CTRL, 0);
    }
    if (err == ESP_OK) {
      err = write_reg(REG_MD1_CFG, 0);
    }
    first = err;
  }
  const esp_err_t pm1[] = {
      m5_corep4x_pm1_gpio_wake(M5_COREP4X_PM1_IMU_INT, false, false),
      m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_IRQ_MASK1,
                            1 << M5_COREP4X_PM1_IMU_INT,
                            1 << M5_COREP4X_PM1_IMU_INT),
      m5_corep4x_pm1_gpio_pull(M5_COREP4X_PM1_IMU_INT, 0),
  };
  for (size_t i = 0; i < sizeof(pm1) / sizeof(pm1[0]); i++) {
    if (first == ESP_OK && pm1[i] != ESP_OK) {
      first = pm1[i];
    }
  }
  return first;
}

esp_err_t m5_corep4x_imu_prepare_sleep(bool motion_wake) {
  lock();
  const esp_err_t err = prepare_sleep_locked(motion_wake);
  unlock();
  return err;
}
