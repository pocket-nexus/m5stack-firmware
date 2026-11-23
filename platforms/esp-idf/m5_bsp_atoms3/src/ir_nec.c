// SPDX-FileCopyrightText: 2019 Espressif Systems (Shanghai) PTE LTD
// SPDX-License-Identifier: Apache-2.0
//
// Derived from M5AtomS3-UserDemo lib/infrared_tools/src/ir_builder_rmt_nec.c
// and include/ir_timings.h (Espressif infrared_tools), and from the
// factory's ir_tx_init/ir_tx_send (src/main.cpp). The legacy rmt_item32_t
// builder is replaced by rmt_symbol_word_t frames sent with a copy encoder.
// The LED is on GPIO4 (AtomS3) or GPIO47 (AtomS3R).
#include "m5_atoms3/ir.h"

#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "m5_atoms3/board.h"

static const char *TAG = "m5_atoms3_ir";

#define NEC_LEADING_CODE_HIGH_US 9000
#define NEC_LEADING_CODE_LOW_US 4500
#define NEC_PAYLOAD_ONE_HIGH_US 560
#define NEC_PAYLOAD_ONE_LOW_US 1690
#define NEC_PAYLOAD_ZERO_HIGH_US 560
#define NEC_PAYLOAD_ZERO_LOW_US 560
#define NEC_REPEAT_CODE_HIGH_US 9000
#define NEC_REPEAT_CODE_LOW_US 2250
#define NEC_ENDING_CODE_HIGH_US 560
// The builder's ending low period: 0x7FFF ticks at 1 MHz.
#define NEC_ENDING_CODE_LOW_US 0x7FFF

static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
static rmt_symbol_word_t frame[34];
static rmt_symbol_word_t repeat[2];

static rmt_symbol_word_t symbol(uint32_t high_us, uint32_t low_us) {
  return (rmt_symbol_word_t){
      .level0 = 1, .duration0 = high_us, .level1 = 0, .duration1 = low_us};
}

esp_err_t m5_atoms3_ir_init(void) {
  if (channel) {
    return ESP_OK;
  }
  const rmt_tx_channel_config_t config = {
      .gpio_num = m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3R
                      ? M5_ATOMS3R_IR_PIN
                      : M5_ATOMS3_IR_PIN,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000,
      .mem_block_symbols = 64,
      .trans_queue_depth = 4,
  };
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&config, &channel), TAG, "channel");
  const rmt_carrier_config_t carrier = {
      .frequency_hz = 38000,
      .duty_cycle = 0.33f,
  };
  esp_err_t err = rmt_apply_carrier(channel, &carrier);
  if (err == ESP_OK) {
    const rmt_copy_encoder_config_t copy = {};
    err = rmt_new_copy_encoder(&copy, &encoder);
  }
  if (err == ESP_OK) {
    err = rmt_enable(channel);
  }
  if (err != ESP_OK) {
    m5_atoms3_ir_deinit();
  }
  return err;
}

esp_err_t m5_atoms3_ir_send_nec(uint16_t address, uint16_t command) {
  ESP_RETURN_ON_FALSE(channel, ESP_ERR_INVALID_STATE, TAG, "not initialized");
  // A queued transfer still reads the symbol buffers.
  ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(channel, 1000), TAG, "busy");
  size_t n = 0;
  frame[n++] = symbol(NEC_LEADING_CODE_HIGH_US, NEC_LEADING_CODE_LOW_US);
  for (int i = 0; i < 16; i++) {
    frame[n++] =
        (address & (1u << i))
            ? symbol(NEC_PAYLOAD_ONE_HIGH_US, NEC_PAYLOAD_ONE_LOW_US)
            : symbol(NEC_PAYLOAD_ZERO_HIGH_US, NEC_PAYLOAD_ZERO_LOW_US);
  }
  for (int i = 0; i < 16; i++) {
    frame[n++] =
        (command & (1u << i))
            ? symbol(NEC_PAYLOAD_ONE_HIGH_US, NEC_PAYLOAD_ONE_LOW_US)
            : symbol(NEC_PAYLOAD_ZERO_HIGH_US, NEC_PAYLOAD_ZERO_LOW_US);
  }
  frame[n++] = symbol(NEC_ENDING_CODE_HIGH_US, NEC_ENDING_CODE_LOW_US);
  repeat[0] = symbol(NEC_REPEAT_CODE_HIGH_US, NEC_REPEAT_CODE_LOW_US);
  repeat[1] = symbol(NEC_ENDING_CODE_HIGH_US, NEC_ENDING_CODE_LOW_US);
  const rmt_transmit_config_t once = {.loop_count = 0};
  ESP_RETURN_ON_ERROR(
      rmt_transmit(channel, encoder, frame, n * sizeof(frame[0]), &once), TAG,
      "frame");
  return rmt_transmit(channel, encoder, repeat, sizeof(repeat), &once);
}

void m5_atoms3_ir_deinit(void) {
  if (channel) {
    rmt_disable(channel);
  }
  if (encoder) {
    rmt_del_encoder(encoder);
    encoder = NULL;
  }
  if (channel) {
    rmt_del_channel(channel);
    channel = NULL;
  }
}
