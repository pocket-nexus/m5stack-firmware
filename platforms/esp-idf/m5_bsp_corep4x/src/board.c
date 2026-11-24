/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Bring-up, power switches, charging and power state, after
 * m5_corep4_tool::init, set_status (POWER_*), set_charge_current and
 * app_main of M5CoreP4X-UserDemo (components/m5_corep4_component/
 * m5_corep4_component.cpp, m5_corep4_status.cpp, main/main.cpp). */
#include "board.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ioe1.h"
#include "pm1.h"

static const char *TAG = "m5p4x_board";

static i2c_master_bus_handle_t s_bus;
static bool s_started;
static m5_corep4x_charge_t s_charge = M5_COREP4X_CHARGE_OFF;

i2c_master_bus_handle_t m5_corep4x_i2c(void) { return s_bus; }

static esp_err_t bus_init(void) {
  if (s_bus) {
    return ESP_OK;
  }
  const i2c_master_bus_config_t config = {
      .i2c_port = M5_COREP4X_I2C_PORT,
      .sda_io_num = M5_COREP4X_I2C_SDA,
      .scl_io_num = M5_COREP4X_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  return i2c_new_master_bus(&config, &s_bus);
}

static int ioe1_pin(m5_corep4x_switch_t which) {
  switch (which) {
  case M5_COREP4X_SWITCH_L3B:
    return M5_COREP4X_IOE1_L3B_EN;
  case M5_COREP4X_SWITCH_AUDIO:
    return M5_COREP4X_IOE1_AU_PWR_EN;
  case M5_COREP4X_SWITCH_SPEAKER:
    return M5_COREP4X_IOE1_SPK_PWR_EN;
  case M5_COREP4X_SWITCH_LCD:
    return M5_COREP4X_IOE1_LCD_PWR_EN;
  case M5_COREP4X_SWITCH_GROVE:
    return M5_COREP4X_IOE1_GROVE_PWR_EN;
  case M5_COREP4X_SWITCH_USB_A:
    return M5_COREP4X_IOE1_USBA_OUT_EN;
  case M5_COREP4X_SWITCH_USB_C:
    return M5_COREP4X_IOE1_P4USBC_OUT_EN;
  default:
    return -1;
  }
}

esp_err_t m5_corep4x_set_switch(m5_corep4x_switch_t which, bool on) {
  ESP_RETURN_ON_FALSE(which < M5_COREP4X_SWITCH_COUNT, ESP_ERR_INVALID_ARG, TAG,
                      "switch");
  if (which == M5_COREP4X_SWITCH_M5BUS_5V) {
    return m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_PWR_CFG,
                                 M5_COREP4X_PM1_PWR_CFG_BOOST_EN,
                                 on ? M5_COREP4X_PM1_PWR_CFG_BOOST_EN : 0);
  }
  const uint8_t pin = (uint8_t)ioe1_pin(which);
  /* set_*_power: pinMode(OUTPUT), then digitalWrite. */
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_output(pin, false), TAG, "IO%u mode",
                      pin + 1);
  return m5_corep4x_ioe1_write(pin, on);
}

esp_err_t m5_corep4x_get_switch(m5_corep4x_switch_t which, bool *on) {
  ESP_RETURN_ON_FALSE(which < M5_COREP4X_SWITCH_COUNT && on,
                      ESP_ERR_INVALID_ARG, TAG, "arguments");
  if (which == M5_COREP4X_SWITCH_M5BUS_5V) {
    uint8_t cfg = 0;
    ESP_RETURN_ON_ERROR(
        m5_corep4x_pm1_read(M5_COREP4X_PM1_REG_PWR_CFG, &cfg, 1), TAG,
        "PWR_CFG");
    *on = (cfg & M5_COREP4X_PM1_PWR_CFG_BOOST_EN) != 0;
    return ESP_OK;
  }
  return m5_corep4x_ioe1_read((uint8_t)ioe1_pin(which), on);
}

/* set_charge_current: 650 mA pulls CHG_PROG low (push-pull), 180 mA leaves
 * it high impedance, off disables charging. */
esp_err_t m5_corep4x_set_charge(m5_corep4x_charge_t current) {
  const bool enable = current != M5_COREP4X_CHARGE_OFF;
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_PWR_CFG,
                            M5_COREP4X_PM1_PWR_CFG_CHG_EN,
                            enable ? M5_COREP4X_PM1_PWR_CFG_CHG_EN : 0),
      TAG, "CHG_EN");
  if (current == M5_COREP4X_CHARGE_650MA) {
    ESP_RETURN_ON_ERROR(
        m5_corep4x_pm1_gpio_output(M5_COREP4X_PM1_CHG_PROG, false, false), TAG,
        "CHG_PROG low");
  } else {
    ESP_RETURN_ON_ERROR(m5_corep4x_pm1_gpio_hiz(M5_COREP4X_PM1_CHG_PROG), TAG,
                        "CHG_PROG hi-Z");
  }
  s_charge = current;
  return ESP_OK;
}

m5_corep4x_charge_t m5_corep4x_charge(void) { return s_charge; }

esp_err_t m5_corep4x_sd_detected(bool *inserted) {
  ESP_RETURN_ON_FALSE(inserted, ESP_ERR_INVALID_ARG, TAG, "inserted");
  bool level = true;
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_read(M5_COREP4X_IOE1_SD_DET, &level), TAG,
                      "SD_DET");
  *inserted = !level;
  return ESP_OK;
}

esp_err_t m5_corep4x_sd_detect_enable(bool on) {
  return m5_corep4x_ioe1_input(M5_COREP4X_IOE1_SD_DET, on);
}

esp_err_t m5_corep4x_power_read(m5_corep4x_power_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  *out = (m5_corep4x_power_t){0};
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_read_vbat(&out->battery_mv), TAG, "VBAT");
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_read_vin(&out->vin_mv), TAG, "VIN");
  ESP_RETURN_ON_ERROR(m5_corep4x_pm1_read_5vinout(&out->bus_mv), TAG,
                      "5VINOUT");
  uint8_t source = 0;
  ESP_RETURN_ON_ERROR(
      m5_corep4x_pm1_read(M5_COREP4X_PM1_REG_PWR_SRC, &source, 1), TAG,
      "PWR_SRC");
  out->source = source & 0x07;
  bool status = true;
  if (m5_corep4x_ioe1_read(M5_COREP4X_IOE1_CHG_STAT, &status) == ESP_OK) {
    out->charging = !status;
  }
  return ESP_OK;
}

esp_err_t m5_corep4x_power_off(void) { return m5_corep4x_pm1_shutdown(); }

esp_err_t m5_corep4x_reboot(void) { return m5_corep4x_pm1_reboot(); }

/* An open-drain reset line released high, as init() leaves TP_RST and
 * LCD_RST. */
static esp_err_t release_reset(uint8_t pin) {
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_output(pin, true), TAG, "IO%u mode",
                      pin + 1);
  return m5_corep4x_ioe1_write(pin, true);
}

esp_err_t m5_corep4x_pulse_reset(uint8_t pin, uint32_t low_ms,
                                 uint32_t settle_ms) {
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_output(pin, true), TAG, "IO%u mode",
                      pin + 1);
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_write(pin, false), TAG, "IO%u low",
                      pin + 1);
  vTaskDelay(pdMS_TO_TICKS(low_ms));
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_write(pin, true), TAG, "IO%u high",
                      pin + 1);
  if (settle_ms) {
    vTaskDelay(pdMS_TO_TICKS(settle_ms));
  }
  return ESP_OK;
}

static void cycle(m5_corep4x_switch_t which) {
  if (m5_corep4x_set_switch(which, false) != ESP_OK) {
    ESP_LOGW(TAG, "switch %d off failed", which);
  }
  vTaskDelay(pdMS_TO_TICKS(200));
  if (m5_corep4x_set_switch(which, true) != ESP_OK) {
    ESP_LOGW(TAG, "switch %d on failed", which);
  }
}

esp_err_t m5_corep4x_init(void) {
  if (s_started) {
    return ESP_OK;
  }
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
    m5_corep4x_release_sleep_holds();
  }
  ESP_RETURN_ON_ERROR(bus_init(), TAG, "I2C bus");
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_begin(s_bus), TAG, "M5IOE1");
  ESP_RETURN_ON_ERROR(release_reset(M5_COREP4X_IOE1_TP_RST), TAG, "TP_RST");
  ESP_RETURN_ON_ERROR(release_reset(M5_COREP4X_IOE1_LCD_RST), TAG, "LCD_RST");
  ESP_RETURN_ON_ERROR(m5_corep4x_set_switch(M5_COREP4X_SWITCH_L3B, true), TAG,
                      "L3B");
  vTaskDelay(pdMS_TO_TICKS(50));
  /* The detect input of the microSD slot and the charger status. */
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_input(M5_COREP4X_IOE1_SD_DET, true), TAG,
                      "SD_DET");
  ESP_RETURN_ON_ERROR(m5_corep4x_ioe1_input(M5_COREP4X_IOE1_CHG_STAT, true),
                      TAG, "CHG_STAT");

  esp_err_t err = m5_corep4x_pm1_begin(s_bus);
  if (err == ESP_OK) {
    (void)m5_corep4x_pm1_wdt_set(0);
    /* setPowerConfig(LED_CTRL, 0): the PMIC LED stays off. */
    (void)m5_corep4x_pm1_update(M5_COREP4X_PM1_REG_PWR_CFG,
                                M5_COREP4X_PM1_PWR_CFG_LED_CTRL, 0);
  } else {
    ESP_LOGE(TAG, "M5PM1: %s", esp_err_to_name(err));
  }

  /* app_main's power sequence. The amplifier stays off until the audio
   * codec is set up. */
  cycle(M5_COREP4X_SWITCH_USB_A);
  (void)m5_corep4x_set_switch(M5_COREP4X_SWITCH_AUDIO, true);
  (void)m5_corep4x_set_switch(M5_COREP4X_SWITCH_SPEAKER, false);
  (void)m5_corep4x_set_switch(M5_COREP4X_SWITCH_LCD, true);
  (void)m5_corep4x_set_switch(M5_COREP4X_SWITCH_GROVE, true);
  cycle(M5_COREP4X_SWITCH_USB_C);
  if (m5_corep4x_pm1_ready() &&
      m5_corep4x_set_charge(M5_COREP4X_CHARGE_180MA) != ESP_OK) {
    ESP_LOGW(TAG, "charge current not set");
  }
  s_started = true;
  ESP_LOGI(TAG, "M5IOE1 at 0x%02X, M5PM1 %s", m5_corep4x_ioe1_address(),
           m5_corep4x_pm1_ready() ? "ready" : "missing");
  return ESP_OK;
}
