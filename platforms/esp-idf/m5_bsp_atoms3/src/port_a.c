// SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
// SPDX-License-Identifier: MIT
//
// ESP-IDF versions of the Arduino calls in M5AtomS3-UserDemo src/main.cpp:
// Wire.begin(2, 1) + beginTransmission scan, Serial.begin on G1/G2,
// ledcSetup(0/1, 1000, 8), analogRead(1/2) and USBSerial.
#include "m5_atoms3/port_a.h"

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "m5_atoms3/board.h"

static const char *TAG = "m5_atoms3_port_a";

#define PORT_A_UART UART_NUM_1
#define PWM_TIMER LEDC_TIMER_0

esp_err_t m5_atoms3_port_a_i2c_scan(uint8_t *addresses, size_t capacity,
                                    size_t *count) {
  ESP_RETURN_ON_FALSE(count && (addresses || !capacity), ESP_ERR_INVALID_ARG,
                      TAG, "invalid arg");
  *count = 0;
  const i2c_master_bus_config_t config = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = M5_ATOMS3_PORT_A_PIN_G2,
      .scl_io_num = M5_ATOMS3_PORT_A_PIN_G1,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  i2c_master_bus_handle_t bus = NULL;
  ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &bus), TAG, "bus");
  size_t found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    if (i2c_master_probe(bus, address, 10) == ESP_OK) {
      if (found < capacity) {
        addresses[found] = address;
      }
      found++;
      if (found > 5) {
        break;
      }
    }
  }
  i2c_del_master_bus(bus);
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G1);
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G2);
  *count = found < capacity ? found : capacity;
  return ESP_OK;
}

esp_err_t m5_atoms3_port_a_uart_open(uint32_t baud, int rx_pin, int tx_pin) {
  const uart_config_t config = {
      .baud_rate = (int)baud,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_RETURN_ON_ERROR(uart_driver_install(PORT_A_UART, 256, 0, 0, NULL, 0), TAG,
                      "uart driver");
  esp_err_t err = uart_param_config(PORT_A_UART, &config);
  if (err == ESP_OK) {
    err = uart_set_pin(PORT_A_UART, tx_pin, rx_pin, UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
  }
  if (err != ESP_OK) {
    uart_driver_delete(PORT_A_UART);
  }
  return err;
}

int m5_atoms3_port_a_uart_read(uint8_t *data, size_t capacity) {
  return uart_read_bytes(PORT_A_UART, data, capacity, 0);
}

int m5_atoms3_port_a_uart_write(const uint8_t *data, size_t length) {
  return uart_write_bytes(PORT_A_UART, data, length);
}

void m5_atoms3_port_a_uart_close(void) {
  if (uart_is_driver_installed(PORT_A_UART)) {
    uart_driver_delete(PORT_A_UART);
  }
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G1);
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G2);
}

esp_err_t m5_atoms3_port_a_pwm_open(uint8_t duty_g1, uint8_t duty_g2) {
  const ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_8_BIT,
      .timer_num = PWM_TIMER,
      .freq_hz = 1000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "pwm timer");
  const int pins[2] = {M5_ATOMS3_PORT_A_PIN_G1, M5_ATOMS3_PORT_A_PIN_G2};
  const uint8_t duty[2] = {duty_g1, duty_g2};
  for (int i = 0; i < 2; i++) {
    const ledc_channel_config_t channel = {
        .gpio_num = pins[i],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)i,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = duty[i],
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "pwm channel");
  }
  return ESP_OK;
}

esp_err_t m5_atoms3_port_a_pwm_set(uint8_t duty_g1, uint8_t duty_g2) {
  ESP_RETURN_ON_ERROR(
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_g1), TAG, "duty");
  ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0),
                      TAG, "duty");
  ESP_RETURN_ON_ERROR(
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_g2), TAG, "duty");
  return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void m5_atoms3_port_a_pwm_close(void) {
  ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
  ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G1);
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G2);
}

static adc_oneshot_unit_handle_t adc_unit;

esp_err_t m5_atoms3_port_a_adc_open(void) {
  if (adc_unit) {
    return ESP_OK;
  }
  const adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
  ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit, &adc_unit), TAG, "adc unit");
  const adc_oneshot_chan_cfg_t channel = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_12,
  };
  esp_err_t err = adc_oneshot_config_channel(adc_unit, ADC_CHANNEL_0, &channel);
  if (err == ESP_OK) {
    err = adc_oneshot_config_channel(adc_unit, ADC_CHANNEL_1, &channel);
  }
  if (err != ESP_OK) {
    m5_atoms3_port_a_adc_close();
  }
  return err;
}

esp_err_t m5_atoms3_port_a_adc_read(int samples, int *raw_g1, int *raw_g2) {
  ESP_RETURN_ON_FALSE(adc_unit && samples > 0 && raw_g1 && raw_g2,
                      ESP_ERR_INVALID_ARG, TAG, "invalid arg");
  int sum1 = 0, sum2 = 0;
  for (int i = 0; i < samples; i++) {
    int raw = 0;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(adc_unit, ADC_CHANNEL_0, &raw), TAG,
                        "g1");
    sum1 += raw;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(adc_unit, ADC_CHANNEL_1, &raw), TAG,
                        "g2");
    sum2 += raw;
  }
  *raw_g1 = sum1 / samples;
  *raw_g2 = sum2 / samples;
  return ESP_OK;
}

void m5_atoms3_port_a_adc_close(void) {
  if (adc_unit) {
    adc_oneshot_del_unit(adc_unit);
    adc_unit = NULL;
  }
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G1);
  gpio_reset_pin(M5_ATOMS3_PORT_A_PIN_G2);
}

esp_err_t m5_atoms3_usb_serial_init(void) {
  if (usb_serial_jtag_is_driver_installed()) {
    return ESP_OK;
  }
  usb_serial_jtag_driver_config_t config =
      USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  return usb_serial_jtag_driver_install(&config);
}

int m5_atoms3_usb_serial_read(uint8_t *data, size_t capacity) {
  return usb_serial_jtag_read_bytes(data, capacity, 0);
}

int m5_atoms3_usb_serial_write(const uint8_t *data, size_t length) {
  return usb_serial_jtag_write_bytes(data, length, 0);
}
