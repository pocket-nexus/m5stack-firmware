/*
 * SPDX-FileCopyrightText: 2025 exia
 *
 * SPDX-License-Identifier: MIT
 */
/* NEC frames as the factory's otool_ir codec builds and parses them
 * (format/nec/nec_encoder.cpp, format/nec/nec_decoder.cpp), at an RMT
 * resolution of 1 MHz (1 tick = 1 us). A symbol is one 32-bit word:
 * duration0 in bits 0-14, level0 in bit 15, duration1 in bits 16-30,
 * level1 in bit 31. No ESP-IDF dependency, so the codec is testable on a
 * host. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Leading code, 16 address bits, 16 command bits, ending code. */
#define IR_NEC_FRAME_SYMBOLS 34
/* IR_MAX_PAYLOAD_BYTES of ir_codec_types.h. */
#define IR_NEC_MAX_DATA_BYTES 32

#define IR_NEC_LEADING_MARK_US 9000
#define IR_NEC_LEADING_SPACE_US 4500
#define IR_NEC_BIT_MARK_US 560
#define IR_NEC_ZERO_SPACE_US 560
#define IR_NEC_ONE_SPACE_US 1690
#define IR_NEC_REPEAT_MARK_US 9000
#define IR_NEC_REPEAT_SPACE_US 2250
#define IR_NEC_ENDING_SPACE_TICKS 0x7FFF
/* The decoder accepts spec - 200 < duration < spec + 200 (us). */
#define IR_NEC_MARGIN_US 200

static inline uint32_t ir_symbol(uint32_t duration0, uint32_t level0,
                                 uint32_t duration1, uint32_t level1) {
  return (duration0 & 0x7FFFu) | ((level0 & 1u) << 15) |
         ((duration1 & 0x7FFFu) << 16) | ((level1 & 1u) << 31);
}

static inline uint32_t ir_symbol_duration0(uint32_t symbol) {
  return symbol & 0x7FFFu;
}

static inline uint32_t ir_symbol_duration1(uint32_t symbol) {
  return (symbol >> 16) & 0x7FFFu;
}

/* The transmitted frame: leading 9000 us mark + 4500 us space; the 16-bit
 * address and then the 16-bit command, each least significant bit first
 * (bit 0: 560 + 560 us, bit 1: 560 + 1690 us); ending 560 us mark + 0x7FFF
 * ticks space. Marks are level 1. */
void ir_nec_encode(uint16_t address, uint16_t command,
                   uint32_t symbols[IR_NEC_FRAME_SYMBOLS]);

/* The decoder's memory: the last frame, for repeat codes. */
typedef struct {
  uint16_t address;
  uint16_t command;
} ir_nec_state_t;

typedef struct {
  uint16_t address;
  uint16_t command;
  bool repeat;
} ir_nec_frame_t;

/* NecDecoder::decode: 2 symbols starting with 9000 + 2250 us are a repeat
 * code (the last address and command); otherwise at least 17 symbols with
 * a 9000 + 4500 us leading code, a 16-bit address and (count - 17) / 8
 * data bytes (1..32). Exactly 34 symbols give the 16-bit command data[0] |
 * data[1] << 8; other lengths give data[0]. */
bool ir_nec_decode(ir_nec_state_t *state, const uint32_t *symbols, size_t count,
                   ir_nec_frame_t *out);
