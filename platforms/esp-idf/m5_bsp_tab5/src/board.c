/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* System I2C and the two PI4IOE5V6408 expanders, from m5stack_tab5.c of
 * M5Tab5-UserDemo (bsp_i2c_init, bsp_io_expander_pi4ioe_init, bsp_set_*,
 * bsp_*_detect, bsp_generate_poweroff_signal, bsp_reset_tp). */
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "m5/tab5.h"

static const char *TAG = "m5_tab5";

#define EXPANDER_0_ADDRESS 0x43 /* address pin low */
#define EXPANDER_1_ADDRESS 0x44 /* address pin high */
#define I2C_TIMEOUT_MS 50

/* PI4IOE5V6408 registers. */
#define PI4IO_REG_CHIP_RESET 0x01
#define PI4IO_REG_IO_DIR 0x03
#define PI4IO_REG_OUT_SET 0x05
#define PI4IO_REG_OUT_H_IM 0x07
#define PI4IO_REG_IN_DEF_STA 0x09
#define PI4IO_REG_PULL_EN 0x0B
#define PI4IO_REG_PULL_SEL 0x0D
#define PI4IO_REG_IN_STA 0x0F
#define PI4IO_REG_INT_MASK 0x11

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expanders[2];
static SemaphoreHandle_t expander_lock;

i2c_master_bus_handle_t m5_tab5_i2c(void) { return bus; }

static esp_err_t write_register(int expander, uint8_t reg, uint8_t value) {
  const uint8_t buffer[2] = {reg, value};
  return i2c_master_transmit(expanders[expander], buffer, sizeof(buffer),
                             I2C_TIMEOUT_MS);
}

static esp_err_t read_register(int expander, uint8_t reg, uint8_t *value) {
  return i2c_master_transmit_receive(expanders[expander], &reg, 1, value, 1,
                                     I2C_TIMEOUT_MS);
}

/* Read-modify-write of one output bit, serialized across tasks. */
static esp_err_t update_bit(int expander, uint8_t reg, int bit, bool set) {
  ESP_RETURN_ON_FALSE(expanders[expander], ESP_ERR_INVALID_STATE, TAG,
                      "m5_tab5_init has not run");
  xSemaphoreTake(expander_lock, portMAX_DELAY);
  uint8_t value = 0;
  esp_err_t ret = read_register(expander, reg, &value);
  if (ret == ESP_OK) {
    value =
        set ? (uint8_t)(value | (1u << bit)) : (uint8_t)(value & ~(1u << bit));
    ret = write_register(expander, reg, value);
  }
  xSemaphoreGive(expander_lock);
  return ret;
}

static esp_err_t read_bit(int expander, uint8_t reg, int bit, bool *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  ESP_RETURN_ON_FALSE(expanders[expander], ESP_ERR_INVALID_STATE, TAG,
                      "m5_tab5_init has not run");
  xSemaphoreTake(expander_lock, portMAX_DELAY);
  uint8_t value = 0;
  const esp_err_t ret = read_register(expander, reg, &value);
  xSemaphoreGive(expander_lock);
  *out = (value >> bit) & 1u;
  return ret;
}

static esp_err_t add_expander(uint8_t address, i2c_master_dev_handle_t *out) {
  const i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 400000,
  };
  return i2c_master_bus_add_device(bus, &config, out);
}

/* bsp_io_expander_pi4ioe_init. After the chip reset every pin is Hi-Z with a
 * pull-down. The panel side of LCD_RST has no 1.8 V pull-up and the expander
 * must not drive it to 3.3 V: P4 latches 0, is driven low for 10 ms and is
 * then released as an input with its pull-up. */
static esp_err_t init_expanders(void) {
  uint8_t value = 0;
  ESP_RETURN_ON_ERROR(add_expander(EXPANDER_0_ADDRESS, &expanders[0]), TAG,
                      "expander 0x43");
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_CHIP_RESET, 0xFF), TAG,
                      "expander 0x43 reset");
  ESP_RETURN_ON_ERROR(read_register(0, PI4IO_REG_CHIP_RESET, &value), TAG,
                      "expander 0x43");
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_PULL_SEL, 0x7F), TAG,
                      "pull select");
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_PULL_EN, 0x7F), TAG,
                      "pull enable");
  /* P1 speaker, P2 external 5 V, P5 touch reset released, P6 camera. */
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_OUT_SET, 0x66), TAG,
                      "outputs");
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_OUT_H_IM, 0x00), TAG,
                      "high impedance");
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_IO_DIR, 0x7F), TAG,
                      "directions");
  vTaskDelay(pdMS_TO_TICKS(10));
  ESP_RETURN_ON_ERROR(write_register(0, PI4IO_REG_IO_DIR, 0x6F), TAG,
                      "LCD reset release");
  vTaskDelay(pdMS_TO_TICKS(50));

  ESP_RETURN_ON_ERROR(add_expander(EXPANDER_1_ADDRESS, &expanders[1]), TAG,
                      "expander 0x44");
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_CHIP_RESET, 0xFF), TAG,
                      "expander 0x44 reset");
  ESP_RETURN_ON_ERROR(read_register(1, PI4IO_REG_CHIP_RESET, &value), TAG,
                      "expander 0x44");
  /* Outputs P0, P3, P4, P5, P7; inputs P1, P2, P6. */
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_IO_DIR, 0xB9), TAG,
                      "directions");
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_OUT_H_IM, 0x06), TAG,
                      "high impedance");
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_PULL_SEL, 0xB9), TAG,
                      "pull select");
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_PULL_EN, 0xF9), TAG,
                      "pull enable");
  /* P6 (USB-C VBUS) idles high and raises the expander interrupt. */
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_IN_DEF_STA, 0x40), TAG,
                      "default state");
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_INT_MASK, 0xBF), TAG,
                      "interrupt mask");
  /* P0 ESP32-C6 power and P3 USB-A 5 V on; charging off. */
  ESP_RETURN_ON_ERROR(write_register(1, PI4IO_REG_OUT_SET, 0x09), TAG,
                      "outputs");
  return ESP_OK;
}

esp_err_t m5_tab5_init(void) {
  if (bus)
    return ESP_OK;
  const i2c_master_bus_config_t config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .sda_io_num = M5_TAB5_I2C_SDA,
      .scl_io_num = M5_TAB5_I2C_SCL,
      .i2c_port = M5_TAB5_I2C_PORT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &bus), TAG, "system I2C");
  expander_lock = xSemaphoreCreateMutex();
  ESP_RETURN_ON_FALSE(expander_lock, ESP_ERR_NO_MEM, TAG, "lock");
  const esp_err_t ret = init_expanders();
  if (ret != ESP_OK) {
    for (int i = 0; i < 2; i++)
      if (expanders[i]) {
        i2c_master_bus_rm_device(expanders[i]);
        expanders[i] = NULL;
      }
    i2c_del_master_bus(bus);
    bus = NULL;
  }
  return ret;
}

typedef struct {
  int expander, bit;
  bool active_low;
} switch_pin_t;

static const switch_pin_t SWITCHES[M5_TAB5_SWITCH_COUNT] = {
    [M5_TAB5_SWITCH_EXT_ANTENNA] = {0, 0, false},
    [M5_TAB5_SWITCH_SPEAKER] = {0, 1, false},
    [M5_TAB5_SWITCH_EXT_5V] = {0, 2, false},
    [M5_TAB5_SWITCH_CAMERA] = {0, 6, false},
    [M5_TAB5_SWITCH_WIFI] = {1, 0, false},
    [M5_TAB5_SWITCH_USB_A_5V] = {1, 3, false},
    [M5_TAB5_SWITCH_QUICK_CHARGE] = {1, 5, true},
    [M5_TAB5_SWITCH_CHARGE] = {1, 7, false},
};

esp_err_t m5_tab5_set_switch(m5_tab5_switch_t which, bool on) {
  ESP_RETURN_ON_FALSE(which < M5_TAB5_SWITCH_COUNT, ESP_ERR_INVALID_ARG, TAG,
                      "switch");
  const switch_pin_t *pin = &SWITCHES[which];
  return update_bit(pin->expander, PI4IO_REG_OUT_SET, pin->bit,
                    on != pin->active_low);
}

esp_err_t m5_tab5_get_switch(m5_tab5_switch_t which, bool *on) {
  ESP_RETURN_ON_FALSE(which < M5_TAB5_SWITCH_COUNT && on, ESP_ERR_INVALID_ARG,
                      TAG, "switch");
  const switch_pin_t *pin = &SWITCHES[which];
  bool level = false;
  ESP_RETURN_ON_ERROR(
      read_bit(pin->expander, PI4IO_REG_OUT_SET, pin->bit, &level), TAG,
      "read");
  *on = level != pin->active_low;
  return ESP_OK;
}

esp_err_t m5_tab5_headphone_detect(bool *inserted) {
  return read_bit(0, PI4IO_REG_IN_STA, 7, inserted);
}

esp_err_t m5_tab5_usb_c_detect(bool *powered) {
  return read_bit(1, PI4IO_REG_IN_STA, 6, powered);
}

esp_err_t m5_tab5_power_off(void) {
  ESP_LOGW(TAG, "power off");
  /* The factory pulses the request three times to make sure it lands. */
  for (int i = 0; i < 3; i++) {
    ESP_RETURN_ON_ERROR(update_bit(1, PI4IO_REG_OUT_SET, 4, true), TAG, "on");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(update_bit(1, PI4IO_REG_OUT_SET, 4, false), TAG, "off");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return ESP_OK;
}

/* bsp_reset_tp: LCD_RST (P4) keeps latching 0 and is only driven while low;
 * it is released by turning it back into an input with its pull-up. */
esp_err_t m5_tab5_touch_reset(void) {
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  gpio_reset_pin(M5_TAB5_TOUCH_INT);
  ESP_RETURN_ON_ERROR(update_bit(0, PI4IO_REG_OUT_SET, 4, false), TAG,
                      "LCD reset latch");
  ESP_RETURN_ON_ERROR(update_bit(0, PI4IO_REG_OUT_SET, 5, false), TAG,
                      "touch reset");
  ESP_RETURN_ON_ERROR(update_bit(0, PI4IO_REG_IO_DIR, 4, true), TAG,
                      "LCD reset drive");
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_RETURN_ON_ERROR(update_bit(0, PI4IO_REG_OUT_SET, 5, true), TAG,
                      "touch release");
  ESP_RETURN_ON_ERROR(update_bit(0, PI4IO_REG_IO_DIR, 4, false), TAG,
                      "LCD reset release");
  vTaskDelay(pdMS_TO_TICKS(100));
  return ESP_OK;
}
