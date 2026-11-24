/*
 * SPDX-FileCopyrightText: 2025 ESP-IDF Community
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* RX8130CE real-time clock at 0x32 on the internal I2C bus (400 kHz), as
 * the factory's otool_ic_rx8130 driver (rx8130_tools.cpp) uses it.
 *
 * Register encoding: this driver keeps the factory's encoding, which
 * differs from the RX8130CE datasheet, so that a board moved between this
 * firmware and the factory firmware keeps its date:
 *   - the month register (0x15) holds tm_mon, 0..11 in BCD (January is
 *     0x00; the datasheet uses 01..12);
 *   - the weekday register (0x13) holds tm_wday, 0..6 as a BCD number (the
 *     datasheet uses one bit per day);
 *   - the year register (0x16) holds the year - 2000 in BCD, so the clock
 *     covers 2000..2099.
 * The clock holds local time; the factory firmware uses UTC+8 ("CST-8").
 *
 * The functions are thread-safe; each holds one mutex for its whole
 * register sequence. */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M5_COREP4X_RTC_ADDRESS 0x32

/* Probes 0x32 and, when it answers, runs the factory's start-up: CTRL1
 * (0x1F) INIEN and CHGEN set (backup battery switch-over and charging),
 * FLAG (0x1D) cleared. ESP_ERR_NOT_FOUND when the device does not answer.
 * Later calls return ESP_OK without repeating it. */
esp_err_t m5_corep4x_rtc_init(void);
bool m5_corep4x_rtc_ready(void);

/* Reads registers 0x10..0x16. Sets tm_sec, tm_min, tm_hour, tm_mday,
 * tm_mon, tm_year and tm_wday as the registers hold them (no range check),
 * tm_isdst to -1. */
esp_err_t m5_corep4x_rtc_get(struct tm *out);

/* setTime: CTRL0 STOP set, registers 0x10..0x16 written (tm_wday is
 * written as given), STOP cleared; when the time changed, CTRL0 is then
 * written 0x00 (timer, alarm and update interrupts off), as the factory's
 * task scheduler does with no task pending. tm_year must be 100..199. */
esp_err_t m5_corep4x_rtc_set(const struct tm *time);

/* The factory's RTC wake set-up before deep sleep. seconds > 0: FLAG = 0;
 * EXT (0x1C) TE cleared; the timer counter (0x1A low, 0x1B high) set to
 * seconds, capped at 0xFFFF; EXT TE set with TSEL = 010 (1 Hz); CTRL0 TIE
 * set. /INT then falls when the timer expires (on the board it goes to
 * M5PM1 GPIO4). seconds == 0: CTRL0 = 0x00 and FLAG = 0x00 (no RTC
 * interrupt). */
esp_err_t m5_corep4x_rtc_wake_timer(uint32_t seconds);

#ifdef __cplusplus
}
#endif
