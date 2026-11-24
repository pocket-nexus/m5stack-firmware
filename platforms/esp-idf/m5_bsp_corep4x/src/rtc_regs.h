/*
 * SPDX-FileCopyrightText: 2025 ESP-IDF Community
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* RX8130CE time registers 0x10..0x16 as the factory's
 * otool_ic_rx8130/rx8130_tools.cpp (setTime/getTime) encodes them. No
 * ESP-IDF dependency, so the encoding is testable on a host. */
#pragma once

#include <stdint.h>
#include <time.h>

#define RX8130_REG_SEC 0x10
#define RX8130_REG_TCOUNT0 0x1A
#define RX8130_REG_EXT 0x1C
#define RX8130_REG_FLAG 0x1D
#define RX8130_REG_CTRL0 0x1E
#define RX8130_REG_CTRL1 0x1F

#define RX8130_EXT_TE 0x10        /* timer enable */
#define RX8130_EXT_TSEL_MASK 0x07 /* timer source clock */
#define RX8130_EXT_TSEL_1HZ 0x02
#define RX8130_CTRL0_TIE 0x10  /* timer interrupt enable */
#define RX8130_CTRL0_STOP 0x40 /* stop the clock while it is written */
#define RX8130_CTRL1_INIEN 0x10
#define RX8130_CTRL1_CHGEN 0x20

/* Number of time registers: sec, min, hour, weekday, day, month, year. */
#define RX8130_TIME_REGS 7

static inline uint8_t rx8130_to_bcd(int value) {
  const unsigned v = (unsigned)value & 0xFFu;
  return (uint8_t)(((v / 10u) << 4) + v % 10u);
}

static inline int rx8130_from_bcd(uint8_t bcd) {
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/* setTime: BCD of tm_sec, tm_min, tm_hour, tm_wday (0..6 as a number),
 * tm_mday, tm_mon (0..11, January = 0x00) and tm_year - 100 (years since
 * 2000). */
static inline void rx8130_encode(const struct tm *t,
                                 uint8_t regs[RX8130_TIME_REGS]) {
  regs[0] = rx8130_to_bcd(t->tm_sec);
  regs[1] = rx8130_to_bcd(t->tm_min);
  regs[2] = rx8130_to_bcd(t->tm_hour);
  regs[3] = rx8130_to_bcd(t->tm_wday);
  regs[4] = rx8130_to_bcd(t->tm_mday);
  regs[5] = rx8130_to_bcd(t->tm_mon);
  regs[6] = rx8130_to_bcd(((t->tm_year - 100) % 100 + 100) % 100);
}

/* getTime: the masks of the factory (sec and min 0x7F, hour 0x3F (24 h),
 * weekday 0x7F, day 0x3F, month 0x1F, year the whole byte); the month
 * register is taken as tm_mon unchanged and the year as years since
 * 2000. tm_yday and tm_isdst are not set (tm_isdst -1). */
static inline void rx8130_decode(const uint8_t regs[RX8130_TIME_REGS],
                                 struct tm *t) {
  t->tm_sec = rx8130_from_bcd(regs[0] & 0x7F);
  t->tm_min = rx8130_from_bcd(regs[1] & 0x7F);
  t->tm_hour = rx8130_from_bcd(regs[2] & 0x3F);
  t->tm_wday = rx8130_from_bcd(regs[3] & 0x7F);
  t->tm_mday = rx8130_from_bcd(regs[4] & 0x3F);
  t->tm_mon = rx8130_from_bcd(regs[5] & 0x1F);
  t->tm_year = rx8130_from_bcd(regs[6]) + 100;
  t->tm_yday = 0;
  t->tm_isdst = -1;
}

/* The timer counter of the factory's sleep path: capped at 0xFFFF s. */
static inline uint16_t rx8130_timer_count(uint32_t seconds) {
  return seconds > 0xFFFFu ? 0xFFFFu : (uint16_t)seconds;
}

/* setTimerIrq's EXT value: TE set, TSEL = 010 (1 Hz), the other bits as
 * read. */
static inline uint8_t rx8130_ext_timer_on(uint8_t ext) {
  return (uint8_t)((ext & ~RX8130_EXT_TSEL_MASK) | RX8130_EXT_TE |
                   RX8130_EXT_TSEL_1HZ);
}
