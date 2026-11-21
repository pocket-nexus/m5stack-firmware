// SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
// SPDX-License-Identifier: MIT
//
// Port A (Grove, G1 = GPIO1, G2 = GPIO2) functions of the AtomS3 factory
// firmware (M5AtomS3-UserDemo src/main.cpp): I2C scan, UART bridge, PWM and
// ADC. One function owns the two pins at a time; each `close` releases them.
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2C scan as func_i2c_t: SDA G2, SCL G1, addresses 1..126, stops after the
// sixth device. Writes up to `capacity` addresses and their count.
esp_err_t m5_atoms3_port_a_i2c_scan(uint8_t *addresses, size_t capacity,
                                    size_t *count);

// UART bridge as func_uart_t: 8N1 at `baud`, RX and TX on the given pins
// (GPIO1 or GPIO2). UART1 with a 256-byte RX buffer.
esp_err_t m5_atoms3_port_a_uart_open(uint32_t baud, int rx_pin, int tx_pin);
// Non-blocking; returns the number of bytes read.
int m5_atoms3_port_a_uart_read(uint8_t *data, size_t capacity);
int m5_atoms3_port_a_uart_write(const uint8_t *data, size_t length);
void m5_atoms3_port_a_uart_close(void);

// PWM as func_pwm_t: 1 kHz, 8-bit duty on G1 (channel 0) and G2 (channel 1).
esp_err_t m5_atoms3_port_a_pwm_open(uint8_t duty_g1, uint8_t duty_g2);
esp_err_t m5_atoms3_port_a_pwm_set(uint8_t duty_g1, uint8_t duty_g2);
void m5_atoms3_port_a_pwm_close(void);

// ADC as func_adc_t: 12-bit one-shot reads of G1 (ADC1 channel 0) and G2
// (ADC1 channel 1), 12 dB attenuation.
esp_err_t m5_atoms3_port_a_adc_open(void);
// Mean of `samples` raw reads per pin (0..4095).
esp_err_t m5_atoms3_port_a_adc_read(int samples, int *raw_g1, int *raw_g2);
void m5_atoms3_port_a_adc_close(void);

// USB Serial/JTAG (the factory's USBSerial). Installs the driver once and
// routes the console through it; console output and bridged bytes share the
// port.
esp_err_t m5_atoms3_usb_serial_init(void);
int m5_atoms3_usb_serial_read(uint8_t *data, size_t capacity);
int m5_atoms3_usb_serial_write(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif
