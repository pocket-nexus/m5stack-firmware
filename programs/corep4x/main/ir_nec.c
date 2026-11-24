/*
 * SPDX-FileCopyrightText: 2025 exia
 *
 * SPDX-License-Identifier: MIT
 */
/* NEC codec (ir_nec.h), after otool_ir format/nec of M5CoreP4X-UserDemo. */
#include "ir_nec.h"

static bool in_range(uint32_t duration, uint32_t spec) {
  return duration < spec + IR_NEC_MARGIN_US &&
         duration > spec - IR_NEC_MARGIN_US;
}

static bool is_pair(uint32_t symbol, uint32_t spec0, uint32_t spec1) {
  return in_range(ir_symbol_duration0(symbol), spec0) &&
         in_range(ir_symbol_duration1(symbol), spec1);
}

static bool is_one(uint32_t symbol) {
  return is_pair(symbol, IR_NEC_BIT_MARK_US, IR_NEC_ONE_SPACE_US);
}

static bool is_zero(uint32_t symbol) {
  return is_pair(symbol, IR_NEC_BIT_MARK_US, IR_NEC_ZERO_SPACE_US);
}

static void encode_bits(uint16_t value, uint32_t *symbols) {
  for (int i = 0; i < 16; i++) {
    const uint32_t space =
        (value >> i) & 1u ? IR_NEC_ONE_SPACE_US : IR_NEC_ZERO_SPACE_US;
    symbols[i] = ir_symbol(IR_NEC_BIT_MARK_US, 1, space, 0);
  }
}

void ir_nec_encode(uint16_t address, uint16_t command,
                   uint32_t symbols[IR_NEC_FRAME_SYMBOLS]) {
  symbols[0] = ir_symbol(IR_NEC_LEADING_MARK_US, 1, IR_NEC_LEADING_SPACE_US, 0);
  encode_bits(address, symbols + 1);
  encode_bits(command, symbols + 17);
  symbols[33] = ir_symbol(IR_NEC_BIT_MARK_US, 1, IR_NEC_ENDING_SPACE_TICKS, 0);
}

/* Bits LSB first; false on a symbol that is neither 0 nor 1. */
static bool decode_bits(const uint32_t *symbols, int bits, uint32_t *out) {
  uint32_t value = 0;
  for (int i = 0; i < bits; i++) {
    if (is_one(symbols[i])) {
      value |= 1u << i;
    } else if (!is_zero(symbols[i])) {
      return false;
    }
  }
  *out = value;
  return true;
}

bool ir_nec_decode(ir_nec_state_t *state, const uint32_t *symbols, size_t count,
                   ir_nec_frame_t *out) {
  if (!symbols || count == 0) {
    return false;
  }
  if (count == 2) {
    if (!is_pair(symbols[0], IR_NEC_REPEAT_MARK_US, IR_NEC_REPEAT_SPACE_US)) {
      return false;
    }
    *out = (ir_nec_frame_t){
        .address = state->address,
        .command = state->command,
        .repeat = true,
    };
    return true;
  }
  if (count < 17 ||
      !is_pair(symbols[0], IR_NEC_LEADING_MARK_US, IR_NEC_LEADING_SPACE_US)) {
    return false;
  }
  uint32_t address = 0;
  if (!decode_bits(symbols + 1, 16, &address)) {
    return false;
  }
  const size_t data_bytes = (count - 17) / 8;
  if (data_bytes == 0 || data_bytes > IR_NEC_MAX_DATA_BYTES) {
    return false;
  }
  uint8_t data[IR_NEC_MAX_DATA_BYTES] = {0};
  for (size_t i = 0; i < data_bytes; i++) {
    uint32_t byte = 0;
    if (!decode_bits(symbols + 17 + i * 8, 8, &byte)) {
      return false;
    }
    data[i] = (uint8_t)byte;
  }
  state->address = (uint16_t)address;
  state->command = data[0];
  if (count == IR_NEC_FRAME_SYMBOLS && data_bytes == 2) {
    state->command = (uint16_t)(data[0] | data[1] << 8);
  }
  *out = (ir_nec_frame_t){
      .address = state->address,
      .command = state->command,
      .repeat = false,
  };
  return true;
}
