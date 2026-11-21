/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* CPU temperature, I2C scan, Port A I2C and RS485, from
 * main/hal/hal_esp32.cpp (getCpuTemp, i2cScan, initPortAI2c) and
 * main/hal/components/hal_rs485.cpp of M5Tab5-UserDemo, and bsp_ext_i2c_init
 * in components/m5stack_tab5/m5stack_tab5.c. */
#include "driver/temperature_sensor.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "m5/tab5.h"

static const char *TAG = "m5_tab5_io";

#define RS485_UART UART_NUM_1
#define RS485_BUFFER 128
/* TOUT of 3 symbols: 3.5 characters at 8 bits. */
#define RS485_READ_TOUT 3

static temperature_sensor_handle_t temperature;
static i2c_master_bus_handle_t port_a;
static bool rs485_ready;

esp_err_t m5_tab5_cpu_temperature(float *celsius) {
  ESP_RETURN_ON_FALSE(celsius, ESP_ERR_INVALID_ARG, TAG, "out");
  if (!temperature) {
    const temperature_sensor_config_t config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    ESP_RETURN_ON_ERROR(temperature_sensor_install(&config, &temperature), TAG,
                        "sensor");
    ESP_RETURN_ON_ERROR(temperature_sensor_enable(temperature), TAG, "enable");
  }
  return temperature_sensor_get_celsius(temperature, celsius);
}

esp_err_t m5_tab5_i2c_scan(i2c_master_bus_handle_t bus, uint8_t *addresses,
                           size_t capacity, size_t *count) {
  ESP_RETURN_ON_FALSE(bus && count && (addresses || capacity == 0),
                      ESP_ERR_INVALID_ARG, TAG, "arguments");
  *count = 0;
  for (uint8_t address = 0x10; address < 0x80; address++)
    if (i2c_master_probe(bus, address, 50) == ESP_OK && *count < capacity)
      addresses[(*count)++] = address;
  return ESP_OK;
}

esp_err_t m5_tab5_port_a_i2c_init(i2c_master_bus_handle_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out");
  if (!port_a) {
    const i2c_master_bus_config_t config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = M5_TAB5_PORT_A_I2C_PORT,
        .scl_io_num = M5_TAB5_PORT_A_SCL,
        .sda_io_num = M5_TAB5_PORT_A_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &port_a), TAG, "Port A");
  }
  *out = port_a;
  return ESP_OK;
}

esp_err_t m5_tab5_port_a_i2c_deinit(void) {
  if (!port_a)
    return ESP_OK;
  const esp_err_t ret = i2c_del_master_bus(port_a);
  port_a = NULL;
  return ret;
}

esp_err_t m5_tab5_rs485_init(uint32_t baud) {
  if (rs485_ready)
    return ESP_OK;
  const uart_config_t config = {
      .baud_rate = (int)baud,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_RETURN_ON_ERROR(
      uart_driver_install(RS485_UART, RS485_BUFFER * 2, 0, 0, NULL, 0), TAG,
      "driver");
  ESP_RETURN_ON_ERROR(uart_param_config(RS485_UART, &config), TAG, "config");
  ESP_RETURN_ON_ERROR(uart_set_pin(RS485_UART, M5_TAB5_RS485_TX,
                                   M5_TAB5_RS485_RX, M5_TAB5_RS485_DE,
                                   UART_PIN_NO_CHANGE),
                      TAG, "pins");
  ESP_RETURN_ON_ERROR(uart_set_mode(RS485_UART, UART_MODE_RS485_HALF_DUPLEX),
                      TAG, "half duplex");
  ESP_RETURN_ON_ERROR(uart_set_rx_timeout(RS485_UART, RS485_READ_TOUT), TAG,
                      "timeout");
  rs485_ready = true;
  return ESP_OK;
}

esp_err_t m5_tab5_rs485_write(const uint8_t *data, size_t length) {
  ESP_RETURN_ON_FALSE(rs485_ready && data, ESP_ERR_INVALID_STATE, TAG,
                      "arguments");
  return uart_write_bytes(RS485_UART, data, length) == (int)length ? ESP_OK
                                                                   : ESP_FAIL;
}

esp_err_t m5_tab5_rs485_read(uint8_t *data, size_t capacity, size_t *received,
                             uint32_t timeout_ms) {
  ESP_RETURN_ON_FALSE(rs485_ready && data && received, ESP_ERR_INVALID_STATE,
                      TAG, "arguments");
  const int length =
      uart_read_bytes(RS485_UART, data, capacity, pdMS_TO_TICKS(timeout_ms));
  *received = length > 0 ? (size_t)length : 0;
  return length < 0 ? ESP_FAIL : ESP_OK;
}
