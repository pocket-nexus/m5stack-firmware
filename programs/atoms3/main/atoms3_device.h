// SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
// SPDX-License-Identifier: MIT
//
// The AtomS3 side of the device bridge: the factory functions' hardware
// (apps/atoms3-factory/device.ts lists the values and requests).
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "m5_atoms3/button.h"
#include "m5_atoms3/imu.h"
#include "m5_pocket/bridge.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  ATOMS3_FUNC_NONE = -1,
  ATOMS3_FUNC_WIFI = 0,
  ATOMS3_FUNC_I2C,
  ATOMS3_FUNC_UART,
  ATOMS3_FUNC_PWM,
  ATOMS3_FUNC_ADC,
  ATOMS3_FUNC_IR,
  ATOMS3_FUNC_IMU,
} atoms3_function_t;

typedef struct {
  m5_atoms3_button_t button;
  i2c_master_bus_handle_t internal_i2c;
  m5_atoms3_imu_t imu;
  bool imu_ready;
  bool ir_ready;
  bool usb_ready;
  uint16_t ir_address;
  atoms3_function_t active;
  bool wifi_started;
  atomic_bool wifi_scan_done;
  bool uart_open;
  bool pwm_open;
  bool adc_open;
} atoms3_device_t;

// Board peripherals that live for the whole run: button, internal I2C and
// IMU, IR transmitter, USB Serial/JTAG driver. Publishes `attached` and
// `irAddress`.
esp_err_t atoms3_device_init(atoms3_device_t *device);

// m5_pocket_app input callback: the M5Unified click/hold state machine on
// GPIO41. A click is one frame of POCKET_BTN_CIRCLE, a hold (500 ms) one
// frame of POCKET_BTN_CROSS.
uint32_t atoms3_device_buttons(void *context);
// m5_pocket_app before_frame hook: IMU samples, UART/USB bytes, Wi-Fi scan
// results.
void atoms3_device_before_frame(void *context);
// m5_pocket_app request handler.
void atoms3_device_request(void *context, const m5_pocket_request *request);
