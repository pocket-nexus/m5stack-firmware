// SPDX-FileCopyrightText: 2019 Espressif Systems (Shanghai) PTE LTD
// SPDX-License-Identifier: Apache-2.0
//
// NEC transmitter on the AtomS3 IR LED (GPIO4). The frame layout and
// timings are the infrared_tools NEC builder of M5AtomS3-UserDemo
// (lib/infrared_tools/src/ir_builder_rmt_nec.c, include/ir_timings.h),
// moved from the legacy RMT driver to the RMT TX channel API.
#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// RMT TX channel at 1 MHz with a 38 kHz, 33 % carrier, idle low.
esp_err_t m5_atoms3_ir_init(void);
// Queues one extended-NEC frame (16-bit address and 16-bit command, LSB
// first, no inverted bytes) followed by one repeat code, as the factory
// firmware's ir_tx_send does. Returns without waiting for the transfer.
esp_err_t m5_atoms3_ir_send_nec(uint16_t address, uint16_t command);
void m5_atoms3_ir_deinit(void);

#ifdef __cplusplus
}
#endif
