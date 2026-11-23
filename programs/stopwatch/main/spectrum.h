/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define STOPWATCH_SPECTRUM_BANDS 20
#define STOPWATCH_SPECTRUM_FFT 512
#define STOPWATCH_SPECTRUM_HOP 256

typedef struct {
  float bands[STOPWATCH_SPECTRUM_BANDS]; /* normalised 0..1 */
  float peak_hz;
} stopwatch_spectrum_frame;

/* Resets the analyser state (noise floor, level, smoothing). */
void stopwatch_spectrum_reset(void);

/* Feeds one hop of STOPWATCH_SPECTRUM_HOP mono samples at 44.1 kHz and
 * computes the frame over the last STOPWATCH_SPECTRUM_FFT samples. */
void stopwatch_spectrum_push(const int16_t *samples,
                             stopwatch_spectrum_frame *out);
