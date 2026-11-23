/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Microphone spectrum as M5StopWatch-UserDemo computes it
 * (main/hal/hal_audio.cpp, Hal::updateAudioSpectrum): a 512-point Hann FFT
 * every 256 samples, 20 logarithmic bands from a mix of RMS and peak
 * magnitude, a learned noise floor, an adaptive level reference, a 0.55
 * power curve and attack/release smoothing. The radix-2 FFT here replaces
 * the factory's esp-dsp call.
 */
#include "spectrum.h"
#include <math.h>
#include <string.h>

#define FFT_N STOPWATCH_SPECTRUM_FFT
#define BANDS STOPWATCH_SPECTRUM_BANDS
#define SAMPLE_RATE 44100.0f

static const int band_edges[BANDS + 1] = {1,  2,  3,  4,   5,   6,   7,
                                          8,  9,  12, 16,  21,  28,  37,
                                          49, 64, 84, 111, 147, 194, 256};

static int16_t history[FFT_N];
static float re[FFT_N], im[FFT_N], window_table[FFT_N];
static float noise_floor[BANDS], smoothed[BANDS];
static float level = 0.03f;
static int window_ready;

void stopwatch_spectrum_reset(void) {
  memset(history, 0, sizeof(history));
  memset(noise_floor, 0, sizeof(noise_floor));
  memset(smoothed, 0, sizeof(smoothed));
  level = 0.03f;
}

static void fft(float *xr, float *xi) {
  for (int i = 1, j = 0; i < FFT_N; i++) {
    int bit = FFT_N >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      float t = xr[i];
      xr[i] = xr[j];
      xr[j] = t;
      t = xi[i];
      xi[i] = xi[j];
      xi[j] = t;
    }
  }
  for (int len = 2; len <= FFT_N; len <<= 1) {
    const float angle = -2.0f * (float)M_PI / (float)len;
    const float wr = cosf(angle), wi = sinf(angle);
    for (int i = 0; i < FFT_N; i += len) {
      float cr = 1.0f, ci = 0.0f;
      for (int k = 0; k < len / 2; k++) {
        const int a = i + k, b = i + k + len / 2;
        const float tr = xr[b] * cr - xi[b] * ci;
        const float ti = xr[b] * ci + xi[b] * cr;
        xr[b] = xr[a] - tr;
        xi[b] = xi[a] - ti;
        xr[a] += tr;
        xi[a] += ti;
        const float next = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = next;
      }
    }
  }
}

static float clampf(float v, float lo, float hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

void stopwatch_spectrum_push(const int16_t *samples,
                             stopwatch_spectrum_frame *out) {
  if (!window_ready) {
    for (int i = 0; i < FFT_N; i++) {
      window_table[i] =
          0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i / (FFT_N - 1));
    }
    window_ready = 1;
  }
  memmove(history, history + STOPWATCH_SPECTRUM_HOP,
          (FFT_N - STOPWATCH_SPECTRUM_HOP) * sizeof(int16_t));
  memcpy(history + FFT_N - STOPWATCH_SPECTRUM_HOP, samples,
         STOPWATCH_SPECTRUM_HOP * sizeof(int16_t));

  float mean = 0.0f;
  for (int i = 0; i < FFT_N; i++) {
    mean += history[i];
  }
  mean /= FFT_N;
  for (int i = 0; i < FFT_N; i++) {
    re[i] = ((float)history[i] - mean) / 32768.0f * window_table[i];
    im[i] = 0.0f;
  }
  fft(re, im);

  float raw[BANDS];
  for (int b = 0; b < BANDS; b++) {
    float sum = 0.0f, peak = 0.0f;
    const int count = band_edges[b + 1] - band_edges[b];
    for (int k = band_edges[b]; k < band_edges[b + 1]; k++) {
      const float mag = sqrtf(re[k] * re[k] + im[k] * im[k]) * 2.0f / FFT_N;
      sum += mag * mag;
      if (mag > peak) {
        peak = mag;
      }
    }
    const float rms = sqrtf(sum / (float)count);
    float value = (0.48f * rms + 0.52f * peak) * (1.12f - 0.22f * b / 19.0f);
    const float alpha = value < noise_floor[b] ? 0.45f : 0.004f;
    noise_floor[b] += (value - noise_floor[b]) * alpha;
    value -= 2.20f * noise_floor[b] + 0.0018f;
    raw[b] = value > 0.0f ? value : 0.0f;
  }

  float top[3] = {0.0f, 0.0f, 0.0f};
  for (int b = 0; b < BANDS; b++) {
    if (raw[b] > top[0]) {
      top[2] = top[1];
      top[1] = top[0];
      top[0] = raw[b];
    } else if (raw[b] > top[1]) {
      top[2] = top[1];
      top[1] = raw[b];
    } else if (raw[b] > top[2]) {
      top[2] = raw[b];
    }
  }
  float ref = 0.80f * top[0] + 0.14f * top[1] + 0.06f * top[2];
  if (ref < 0.0015f) {
    ref = 0.0015f;
  }
  level += (ref - level) * (ref > level ? 0.44f : 0.16f);
  level = clampf(level, 0.0015f, 1.0f);

  for (int b = 0; b < BANDS; b++) {
    float norm = clampf(powf(raw[b] / level, 0.55f), 0.0f, 1.0f);
    if (norm < 0.035f) {
      norm = 0.0f;
    }
    smoothed[b] += (norm - smoothed[b]) * (norm > smoothed[b] ? 0.82f : 0.40f);
    out->bands[b] = smoothed[b];
  }

  int best = 1;
  float best_power = 0.0f;
  for (int k = 1; k < FFT_N / 2; k++) {
    const float power = re[k] * re[k] + im[k] * im[k];
    if (power > best_power) {
      best_power = power;
      best = k;
    }
  }
  float bin = (float)best;
  if (best >= 2 && best <= FFT_N / 2 - 2) {
    const float a = re[best - 1] * re[best - 1] + im[best - 1] * im[best - 1];
    const float c = re[best + 1] * re[best + 1] + im[best + 1] * im[best + 1];
    const float denominator = a - 2.0f * best_power + c;
    if (denominator != 0.0f) {
      bin += clampf(0.5f * (a - c) / denominator, -0.5f, 0.5f);
    }
  }
  out->peak_hz = bin * SAMPLE_RATE / FFT_N;
}
