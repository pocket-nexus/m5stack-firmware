/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* The REC window's display card as the board draws it into the texture
 * "assets/rec-canvas.png" (512x512 PSM_5650: red in the low five bits).
 * Texture (0,0) is the card's top-left corner, screen (16,58). Drawn here:
 * the card (448x298 Waveform, 448x406 Spectrum, radius 8, bg 0xF8FAFC,
 * border 0xCBD5E1), the lv_chart waveform charts with their division
 * lines, series and play-head markers, and the spectrogram canvases with
 * their borders. RecView.tsx draws the labels over it. No ESP-IDF
 * dependencies (tests build it on the host). */
#pragma once

#include "audio_dsp.h"
#include <stdbool.h>
#include <stdint.h>

#define REC_TEXTURE_SIZE 512
#define REC_CARD_WIDTH 448
#define REC_CARD_HEIGHT_WAVEFORM 298
#define REC_CARD_HEIGHT_SPECTRUM 406
#define REC_SPEC_WIDTH 330
#define REC_SPEC_HEIGHT 112

/* One spectrogram canvas: a ring of 330 columns of bin levels; columns not
 * written since the last clear show the background 0x050816. */
typedef struct {
  uint8_t levels[REC_SPEC_WIDTH][AUDIO_SPEC_BINS];
  uint16_t head;   /* the column written last */
  uint16_t filled; /* columns written since the clear, <= 330 */
} rec_spectrogram_t;

void rec_spectrogram_clear(rec_spectrogram_t *s);
/* update_rec_spectrogram: scroll one pixel left, the new column right. */
void rec_spectrogram_push(rec_spectrogram_t *s,
                          const uint8_t levels[AUDIO_SPEC_BINS]);

typedef struct {
  bool spectrum; /* Spectrum mode, else Waveform */
  bool show_ref; /* the REF row */
  bool markers;  /* recorded preview play-head markers */
  /* Chart values 0..100 from the oldest (left) to the newest point. */
  int16_t wave[AUDIO_REC_CHANNELS][AUDIO_WAVE_POINTS];
  int16_t rms[AUDIO_REC_CHANNELS][AUDIO_WAVE_POINTS];
  const rec_spectrogram_t *spectrogram[AUDIO_REC_CHANNELS];
} rec_card_t;

/* Draws the card into `pixels` (512 x 512, stride 512). Pixels outside the
 * card are not written. */
void rec_card_draw(uint16_t *pixels, const rec_card_t *card);
