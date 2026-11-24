/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* REC display card renderer (audio_draw.h). Geometry from
 * ui_function_rec_window.cpp (build_content, apply_ref_visibility,
 * update_waveform_markers, update_rec_spectrogram) of M5CoreP4X-UserDemo
 * under LVGL 9.5's default theme at 130 dpi: an lv_chart takes the card
 * style (radius LV_DPX(8) = 7) with the border drawn before the division
 * lines and series; points sit at x = w * i / 199, y = h - v * h / 100 of
 * the content box (border 1 + pad 1); the division lines span the whole
 * chart at 0, 1/4 .. 4/4 of the content box. */
#include "audio_draw.h"

#include <math.h>
#include <string.h>

#define STRIDE REC_TEXTURE_SIZE

#define CARD_BG 0xF8FAFCu
#define CARD_BORDER 0xCBD5E1u
#define CARD_RADIUS 8
/* The window's content panel behind the card's rounded corners. */
#define PANEL_BG 0xFFFFFFu
#define CHART_BG 0x1E293Bu
#define CHART_RADIUS 7
#define CHART_DIV 0x334155u
#define CHART_DIV_OPA 102 /* LV_OPA_40 */
#define CHART_RMS 0xF8FAFCu
#define CHART_MARKER 0x22C55Eu
#define SPEC_BG 0x050816u
#define SPEC_BORDER 0x334155u

/* Chart and canvas positions in card coordinates (screen - (16,58)). */
#define CHART_X 55
#define CHART_WIDTH 332
#define SPEC_X 63

static const uint32_t WAVE_COLORS[AUDIO_REC_CHANNELS] = {
    0x4ADE80u, /* MIC1 */
    0x60A5FAu, /* MIC2 */
    0xFB923Cu, /* REF */
};

static inline uint16_t pack(uint32_t rgb) {
  const uint32_t r = (rgb >> 16) & 0xFFu;
  const uint32_t g = (rgb >> 8) & 0xFFu;
  const uint32_t b = rgb & 0xFFu;
  return (uint16_t)((b >> 3) << 11 | (g >> 2) << 5 | (r >> 3));
}

static inline uint32_t unpack(uint16_t p) {
  const uint32_t r5 = p & 0x1Fu;
  const uint32_t g6 = (p >> 5) & 0x3Fu;
  const uint32_t b5 = (p >> 11) & 0x1Fu;
  const uint32_t r = r5 << 3 | r5 >> 2;
  const uint32_t g = g6 << 2 | g6 >> 4;
  const uint32_t b = b5 << 3 | b5 >> 2;
  return r << 16 | g << 8 | b;
}

static inline uint32_t mix(uint32_t fg, uint32_t bg, uint32_t alpha) {
  uint32_t out = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    const uint32_t f = (fg >> shift) & 0xFFu;
    const uint32_t b = (bg >> shift) & 0xFFu;
    out |= ((f * alpha + b * (255u - alpha) + 127u) / 255u) << shift;
  }
  return out;
}

static inline void blend(uint16_t *p, uint32_t rgb, uint32_t alpha) {
  if (alpha >= 255u) {
    *p = pack(rgb);
  } else if (alpha > 0u) {
    *p = pack(mix(rgb, unpack(*p), alpha));
  }
}

static void fill(uint16_t *px, int x, int y, int w, int h, uint32_t rgb) {
  const uint16_t c = pack(rgb);
  for (int j = 0; j < h; j++) {
    uint16_t *row = px + (size_t)(y + j) * STRIDE + x;
    for (int i = 0; i < w; i++) {
      row[i] = c;
    }
  }
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* A rounded rectangle with a border, anti-aliased corners. */
static void rounded_rect(uint16_t *px, int x, int y, int w, int h, int radius,
                         uint32_t bg, uint32_t border) {
  const float r = (float)radius;
  const float inner = r - 1.0f;
  for (int j = 0; j < h; j++) {
    uint16_t *row = px + (size_t)(y + j) * STRIDE + x;
    const bool corner_row = j < radius || j >= h - radius;
    if (!corner_row) {
      row[0] = pack(border);
      row[w - 1] = pack(border);
      const uint16_t c = pack(j == 0 || j == h - 1 ? border : bg);
      for (int i = 1; i < w - 1; i++) {
        row[i] = c;
      }
      continue;
    }
    const float cy = j < radius ? r : (float)(h - radius);
    for (int i = 0; i < w; i++) {
      float outer_cov;
      float inner_cov;
      if (i < radius || i >= w - radius) {
        const float cx = i < radius ? r : (float)(w - radius);
        const float dx = (float)i + 0.5f - cx;
        const float dy = (float)j + 0.5f - cy;
        const float d = sqrtf(dx * dx + dy * dy);
        outer_cov = clamp01(r - d + 0.5f);
        inner_cov = clamp01(inner - d + 0.5f);
      } else {
        outer_cov = 1.0f;
        inner_cov = (j == 0 || j == h - 1) ? 0.0f : 1.0f;
      }
      if (outer_cov <= 0.0f) {
        continue;
      }
      /* dest * (1 - outer) + border * (outer - inner) + bg * inner */
      const uint32_t under = unpack(row[i]);
      uint32_t out = 0;
      for (int shift = 0; shift <= 16; shift += 8) {
        const float d = (float)((under >> shift) & 0xFFu);
        const float b = (float)((border >> shift) & 0xFFu);
        const float f = (float)((bg >> shift) & 0xFFu);
        const float v = d * (1.0f - outer_cov) + b * (outer_cov - inner_cov) +
                        f * inner_cov;
        out |= (uint32_t)(v + 0.5f) << shift;
      }
      row[i] = pack(out);
    }
  }
}

/* A polyline of width 2 through pixel centres (xs strictly increasing),
 * swept column by column: each column covers the line's vertical extent
 * there plus one pixel above and below, anti-aliased at the ends. Clipped
 * to the w x h box at (ox, oy). */
static void polyline(uint16_t *px, int ox, int oy, int w, int h,
                     const float *xs, const float *ys, int n, uint32_t rgb) {
  int seg = 0;
  const int first = (int)floorf(xs[0]);
  const int last = (int)floorf(xs[n - 1]);
  for (int col = first; col <= last; col++) {
    if (col < 0 || col >= w) {
      continue;
    }
    const float xa = (float)col > xs[0] ? (float)col : xs[0];
    const float xb =
        (float)(col + 1) < xs[n - 1] ? (float)(col + 1) : xs[n - 1];
    while (seg < n - 2 && xs[seg + 1] <= xa) {
      seg++;
    }
    float lo = 1e9f;
    float hi = -1e9f;
    for (int k = seg; k < n - 1 && xs[k] <= xb; k++) {
      const float x0 = xs[k];
      const float x1 = xs[k + 1];
      const float sa = xa > x0 ? xa : x0;
      const float sb = xb < x1 ? xb : x1;
      if (sa > sb) {
        continue;
      }
      const float slope = (ys[k + 1] - ys[k]) / (x1 - x0);
      const float ya = ys[k] + (sa - x0) * slope;
      const float yb = ys[k] + (sb - x0) * slope;
      lo = fminf(lo, fminf(ya, yb));
      hi = fmaxf(hi, fmaxf(ya, yb));
    }
    if (lo > hi) {
      continue;
    }
    const float top = lo - 1.0f;
    const float bottom = hi + 1.0f;
    int row0 = (int)floorf(top);
    int row1 = (int)ceilf(bottom);
    row0 = row0 < 0 ? 0 : row0;
    row1 = row1 > h ? h : row1;
    for (int row = row0; row < row1; row++) {
      const float a = fmaxf((float)row, top);
      const float b = fminf((float)(row + 1), bottom);
      const float cov = clamp01(b - a);
      blend(px + (size_t)(oy + row) * STRIDE + ox + col, rgb,
            (uint32_t)(cov * 255.0f + 0.5f));
    }
  }
}

/* One lv_chart: x, y in card coordinates, w x h outside size. */
static void chart(uint16_t *px, int x, int y, int w, int h, const int16_t *wave,
                  const int16_t *rms, uint32_t color, bool marker) {
  rounded_rect(px, x, y, w, h, CHART_RADIUS, CHART_BG, CARD_BORDER);
  /* Content box: border 1 + pad 1 on each side. */
  const int cw = w - 4;
  const int ch = h - 4;
  for (int i = 0; i <= 4; i++) {
    const int ly = y + 2 + ch * i / 4;
    uint16_t *row = px + (size_t)ly * STRIDE + x;
    for (int k = 0; k < w; k++) {
      blend(row + k, CHART_DIV, CHART_DIV_OPA);
    }
    const int lx = x + 2 + cw * i / 4;
    for (int k = 0; k < h; k++) {
      blend(px + (size_t)(y + k) * STRIDE + lx, CHART_DIV, CHART_DIV_OPA);
    }
  }
  float xs[AUDIO_WAVE_POINTS];
  float ys[AUDIO_WAVE_POINTS];
  for (int i = 0; i < AUDIO_WAVE_POINTS; i++) {
    xs[i] = (float)(2 + cw * i / (AUDIO_WAVE_POINTS - 1)) + 0.5f;
  }
  /* LVGL draws the series added last first: the RMS line under the
   * waveform. */
  for (int i = 0; i < AUDIO_WAVE_POINTS; i++) {
    ys[i] = (float)(2 + ch - rms[i] * ch / 100) + 0.5f;
  }
  polyline(px, x, y, w, h, xs, ys, AUDIO_WAVE_POINTS, CHART_RMS);
  for (int i = 0; i < AUDIO_WAVE_POINTS; i++) {
    ys[i] = (float)(2 + ch - wave[i] * ch / 100) + 0.5f;
  }
  polyline(px, x, y, w, h, xs, ys, AUDIO_WAVE_POINTS, color);
  if (marker) {
    /* 3 x (h - 8) at (w * 66 / 200, 4) of the content box. */
    fill(px, x + 2 + w * AUDIO_PREVIEW_ANCHOR / AUDIO_WAVE_POINTS, y + 2 + 4, 3,
         h - 8, CHART_MARKER);
  }
}

static void spectrogram(uint16_t *px, int x, int y,
                        const rec_spectrogram_t *s) {
  static uint16_t colors[256];
  static bool colors_ready;
  if (!colors_ready) {
    for (int level = 0; level < 256; level++) {
      colors[level] = pack(audio_spectrum_color((uint8_t)level));
    }
    colors_ready = true;
  }
  /* Bin b fills rows v * 112 / 24 .. (v + 1) * 112 / 24 - 1, v = 23 - b. */
  uint8_t row_bin[REC_SPEC_HEIGHT];
  for (int v = 0; v < AUDIO_SPEC_BINS; v++) {
    const int y0 = v * REC_SPEC_HEIGHT / AUDIO_SPEC_BINS;
    const int y1 = (v + 1) * REC_SPEC_HEIGHT / AUDIO_SPEC_BINS;
    for (int row = y0; row < y1; row++) {
      row_bin[row] = (uint8_t)(AUDIO_SPEC_BINS - 1 - v);
    }
  }
  /* Ring column of each canvas column, -1 for the background. */
  int16_t source[REC_SPEC_WIDTH];
  for (int col = 0; col < REC_SPEC_WIDTH; col++) {
    const int age = REC_SPEC_WIDTH - 1 - col;
    source[col] = (int16_t)(age < s->filled ? (s->head + REC_SPEC_WIDTH - age) %
                                                  REC_SPEC_WIDTH
                                            : -1);
  }
  const uint16_t bg = pack(SPEC_BG);
  for (int row = 0; row < REC_SPEC_HEIGHT; row++) {
    uint16_t *out = px + (size_t)(y + row) * STRIDE + x;
    const int bin = row_bin[row];
    for (int col = 0; col < REC_SPEC_WIDTH; col++) {
      out[col] = source[col] < 0 ? bg : colors[s->levels[source[col]][bin]];
    }
  }
  /* The canvas border covers its outermost pixels. */
  fill(px, x, y, REC_SPEC_WIDTH, 1, SPEC_BORDER);
  fill(px, x, y + REC_SPEC_HEIGHT - 1, REC_SPEC_WIDTH, 1, SPEC_BORDER);
  fill(px, x, y, 1, REC_SPEC_HEIGHT, SPEC_BORDER);
  fill(px, x + REC_SPEC_WIDTH - 1, y, 1, REC_SPEC_HEIGHT, SPEC_BORDER);
}

void rec_spectrogram_clear(rec_spectrogram_t *s) {
  s->head = REC_SPEC_WIDTH - 1;
  s->filled = 0;
}

void rec_spectrogram_push(rec_spectrogram_t *s,
                          const uint8_t levels[AUDIO_SPEC_BINS]) {
  s->head = (uint16_t)((s->head + 1) % REC_SPEC_WIDTH);
  memcpy(s->levels[s->head], levels, AUDIO_SPEC_BINS);
  if (s->filled < REC_SPEC_WIDTH) {
    s->filled++;
  }
}

/* The card with the panel colour behind its rounded corners. */
static void card_frame(uint16_t *px, int h) {
  const int w = REC_CARD_WIDTH;
  const int r = CARD_RADIUS;
  fill(px, 0, 0, r, r, PANEL_BG);
  fill(px, w - r, 0, r, r, PANEL_BG);
  fill(px, 0, h - r, r, r, PANEL_BG);
  fill(px, w - r, h - r, r, r, PANEL_BG);
  rounded_rect(px, 0, 0, w, h, r, CARD_BG, CARD_BORDER);
}

void rec_card_draw(uint16_t *pixels, const rec_card_t *card) {
  const int rows = card->show_ref ? 3 : 2;
  if (card->spectrum) {
    /* Rows of 182 (120 with REF) px from y 33, the canvas centred. */
    static const int ys_two[2] = {68, 254};
    static const int ys_three[3] = {37, 161, 285};
    card_frame(pixels, REC_CARD_HEIGHT_SPECTRUM);
    for (int ch = 0; ch < rows; ch++) {
      if (card->spectrogram[ch]) {
        spectrogram(pixels, SPEC_X, card->show_ref ? ys_three[ch] : ys_two[ch],
                    card->spectrogram[ch]);
      }
    }
    return;
  }
  /* Rows of 128 (84 with REF) px from y 33, the chart 10 px shorter and
   * centred. */
  static const int ys_two[2] = {38, 170};
  static const int ys_three[3] = {38, 126, 214};
  const int h = card->show_ref ? 74 : 118;
  card_frame(pixels, REC_CARD_HEIGHT_WAVEFORM);
  /* Below the Waveform card the view shows the button card; the panel
   * colour there covers a Spectrum picture of an earlier upload while the
   * view changes mode. */
  fill(pixels, 0, REC_CARD_HEIGHT_WAVEFORM, REC_CARD_WIDTH,
       REC_CARD_HEIGHT_SPECTRUM - REC_CARD_HEIGHT_WAVEFORM, PANEL_BG);
  for (int ch = 0; ch < rows; ch++) {
    chart(pixels, CHART_X, card->show_ref ? ys_three[ch] : ys_two[ch],
          CHART_WIDTH, h, card->wave[ch], card->rms[ch], WAVE_COLORS[ch],
          card->markers);
  }
}
