/*
 * SPDX-FileCopyrightText: 2025 exia
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Signal processing of the REC and MUSIC windows (audio_dsp.h). */
#include "audio_dsp.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_PI 3.14159265358979323846

int16_t audio_wave_y(int32_t sample) {
  int32_t y = AUDIO_WAVE_ZERO + (sample * 48 * 4) / 32767;
  if (y < 0) {
    y = 0;
  } else if (y > 100) {
    y = 100;
  }
  return (int16_t)y;
}

void audio_db_text(int32_t peak, bool floor_60, char out[8]) {
  if (peak <= 1) {
    strcpy(out, "-inf");
    return;
  }
  const double db = 20.0 * log10((double)peak / 32767.0);
  if (floor_60 && db < -60.0) {
    strcpy(out, "-inf");
    return;
  }
  snprintf(out, 8, "%+.0f", db);
}

void audio_chunk_measure(const uint8_t *frames, size_t count, bool spectrum,
                         bool ref, audio_chunk_t *out) {
  int32_t peak[AUDIO_REC_CHANNELS] = {0};
  uint64_t sum_sq[AUDIO_REC_CHANNELS] = {0};
  size_t targets[AUDIO_WAVE_PER_TICK];
  for (int pt = 0; pt < AUDIO_WAVE_PER_TICK; pt++) {
    targets[pt] = count <= 1 ? 0
                             : (size_t)pt * (count - 1) /
                                   (size_t)(AUDIO_WAVE_PER_TICK - 1);
  }
  for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
    out->spectrum_count[ch] = 0;
    for (int pt = 0; pt < AUDIO_WAVE_PER_TICK; pt++) {
      out->wave_y[ch][pt] = AUDIO_WAVE_ZERO;
    }
  }
  for (size_t i = 0; i < count; i++) {
    int16_t mics[4];
    audio_tdm_split(frames + i * AUDIO_FRAME_BYTES, mics);
    for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
      const int32_t s = mics[ch];
      const int32_t magnitude = s < 0 ? -s : s;
      if (magnitude > peak[ch]) {
        peak[ch] = magnitude;
      }
      sum_sq[ch] += (uint64_t)((int64_t)s * s);
      for (int pt = 0; pt < AUDIO_WAVE_PER_TICK; pt++) {
        if (i == targets[pt]) {
          out->wave_y[ch][pt] = audio_wave_y(s);
        }
      }
      if (spectrum && (ch < 2 || ref) &&
          out->spectrum_count[ch] < AUDIO_SPEC_DFT_N) {
        out->spectrum_samples[ch][out->spectrum_count[ch]++] = (int16_t)s;
      }
    }
  }
  for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
    out->peak[ch] = (int16_t)(peak[ch] > 32767 ? 32767 : peak[ch]);
    const double rms = count ? sqrt((double)sum_sq[ch] / (double)count) : 0.0;
    out->rms[ch] = (int16_t)(rms > 32767.0 ? 32767 : rms);
  }
}

void audio_dft_init(audio_dft_t *tables) {
  static const uint8_t k_bins[AUDIO_SPEC_BINS] = {
      1,  2,  3,  4,  5,  6,  7,  8,  10, 12, 14, 17,
      20, 23, 27, 31, 35, 39, 44, 49, 54, 58, 61, 63,
  };
  for (int bin = 0; bin < AUDIO_SPEC_BINS; bin++) {
    for (int n = 0; n < AUDIO_SPEC_DFT_N; n++) {
      const double phase = 2.0 * AUDIO_PI * k_bins[bin] * n / AUDIO_SPEC_DFT_N;
      tables->cos_tbl[bin][n] = (float)cos(phase);
      tables->sin_tbl[bin][n] = (float)sin(phase);
    }
  }
  for (int n = 0; n < AUDIO_SPEC_DFT_N; n++) {
    tables->window[n] =
        (float)(0.5 - 0.5 * cos(2.0 * AUDIO_PI * n / (AUDIO_SPEC_DFT_N - 1)));
  }
}

void audio_spectrum_levels(const audio_dft_t *tables, const int16_t *samples,
                           size_t count, uint8_t out[AUDIO_SPEC_BINS]) {
  if (!tables || !samples || count < AUDIO_SPEC_DFT_N) {
    memset(out, 0, AUDIO_SPEC_BINS);
    return;
  }
  int32_t sum = 0;
  for (int n = 0; n < AUDIO_SPEC_DFT_N; n++) {
    sum += samples[n];
  }
  /* The factory computes in double; float keeps the levels (8-bit) and
   * runs on the ESP32-P4's single-precision FPU. */
  const float mean = (float)sum / (float)AUDIO_SPEC_DFT_N;
  float v[AUDIO_SPEC_DFT_N];
  for (int n = 0; n < AUDIO_SPEC_DFT_N; n++) {
    v[n] = ((float)samples[n] - mean) * tables->window[n];
  }
  for (int bin = 0; bin < AUDIO_SPEC_BINS; bin++) {
    float re = 0.0f;
    float im = 0.0f;
    for (int n = 0; n < AUDIO_SPEC_DFT_N; n++) {
      re += v[n] * tables->cos_tbl[bin][n];
      im -= v[n] * tables->sin_tbl[bin][n];
    }
    const float mag =
        2.0f * sqrtf(re * re + im * im) / ((float)AUDIO_SPEC_DFT_N * 32767.0f);
    const float db = 20.0f * log10f(mag > 1e-8f ? mag : 1e-8f);
    float normalized = (db + 78.0f) / 56.0f;
    if (normalized < 0.0f) {
      normalized = 0.0f;
    } else if (normalized > 1.0f) {
      normalized = 1.0f;
    }
    out[bin] = (uint8_t)(normalized * 255.0f);
  }
}

uint32_t audio_spectrum_color(uint8_t level) {
  const unsigned x = level;
  unsigned r, g, b;
  if (x < 64) {
    r = 5;
    g = x / 4;
    b = 32 + x;
  } else if (x < 128) {
    r = (x - 64) * 2;
    g = 16 + (x - 64) / 2;
    b = 96 + (x - 64);
  } else if (x < 192) {
    r = 128 + (x - 128) * 2;
    g = 48 + (x - 128);
    b = 160 - (x - 128) * 2;
  } else {
    r = 255;
    g = 112 + (x - 192) * 2;
    b = x > 224 ? (x - 224) * 4 : 0;
  }
  return (r & 0xFFu) << 16 | (g & 0xFFu) << 8 | (b & 0xFFu);
}

void audio_preview_begin(audio_preview_acc_t *acc, size_t total_frames) {
  memset(acc, 0, sizeof(*acc));
  acc->total = total_frames;
}

void audio_preview_feed(audio_preview_acc_t *acc, const uint8_t *frames,
                        size_t count) {
  if (acc->total == 0) {
    return;
  }
  for (size_t i = 0; i < count; i++) {
    uint64_t bucket = (uint64_t)(acc->offset + i) * AUDIO_PREVIEW_POINTS /
                      (uint64_t)acc->total;
    if (bucket >= AUDIO_PREVIEW_POINTS) {
      bucket = AUDIO_PREVIEW_POINTS - 1;
    }
    int16_t mics[4];
    audio_tdm_split(frames + i * AUDIO_FRAME_BYTES, mics);
    for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
      const int32_t s = mics[ch];
      const int32_t magnitude = s < 0 ? -s : s;
      if (magnitude > acc->magnitude[ch][bucket]) {
        acc->magnitude[ch][bucket] = magnitude;
        acc->sample[ch][bucket] = (int16_t)s;
      }
      if (magnitude > acc->peak[ch]) {
        acc->peak[ch] = magnitude;
      }
    }
  }
  acc->offset += count;
}

void audio_preview_finish(
    const audio_preview_acc_t *acc,
    int16_t wave_y[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS],
    int16_t peak[AUDIO_REC_CHANNELS]) {
  for (int ch = 0; ch < AUDIO_REC_CHANNELS; ch++) {
    peak[ch] = (int16_t)(acc->peak[ch] > 32767 ? 32767 : acc->peak[ch]);
    for (int pt = 0; pt < AUDIO_PREVIEW_POINTS; pt++) {
      wave_y[ch][pt] = audio_wave_y(acc->sample[ch][pt]);
    }
  }
}

static uint32_t read_u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}

static uint16_t read_u16(const uint8_t *p) {
  return (uint16_t)(p[0] | p[1] << 8);
}

bool audio_wav_parse(const uint8_t *data, size_t size, audio_wav_t *out) {
  memset(out, 0, sizeof(*out));
  if (!data || size < 12 || memcmp(data, "RIFF", 4) != 0 ||
      memcmp(data + 8, "WAVE", 4) != 0) {
    return false;
  }
  uint32_t bits = 0;
  size_t pos = 12;
  while (pos + 8 <= size) {
    const uint32_t length = read_u32(data + pos + 4);
    const uint8_t *body = data + pos + 8;
    if (length > size - pos - 8) {
      return false;
    }
    if (memcmp(data + pos, "fmt ", 4) == 0 && length >= 16) {
      if (read_u16(body) != 1) {
        return false;
      }
      out->channels = read_u16(body + 2);
      out->rate = read_u32(body + 4);
      bits = read_u16(body + 14);
    } else if (memcmp(data + pos, "data", 4) == 0) {
      if (bits != 16 || out->channels < 1 || out->channels > 2 ||
          out->rate == 0 || ((uintptr_t)body & 1u) != 0) {
        return false;
      }
      out->samples = (const int16_t *)(const void *)body;
      out->frames = length / (2u * out->channels);
      return out->frames > 0;
    }
    pos += 8 + (size_t)length + (length & 1u);
  }
  return false;
}

uint64_t audio_convert_length(const audio_wav_t *wav) {
  uint64_t frames =
      ((uint64_t)wav->frames * AUDIO_RATE + wav->rate / 2) / wav->rate;
  return frames ? frames : 1;
}

static int32_t wav_sample(const audio_wav_t *wav, size_t frame,
                          uint32_t channel) {
  if (wav->channels == 1) {
    return wav->samples[frame];
  }
  return wav->samples[frame * 2 + channel];
}

void audio_convert(const audio_wav_t *wav, uint64_t first, size_t count,
                   int32_t *out) {
  const uint64_t step = ((uint64_t)wav->rate << 32) / AUDIO_RATE;
  for (size_t i = 0; i < count; i++) {
    const uint64_t phase = (first + i) * step;
    size_t frame0 = (size_t)(phase >> 32);
    if (frame0 >= wav->frames) {
      frame0 = wav->frames - 1;
    }
    const size_t frame1 = frame0 + 1 < wav->frames ? frame0 + 1 : frame0;
    const int64_t frac = (int64_t)(phase & 0xFFFFFFFFu);
    for (uint32_t ch = 0; ch < 2; ch++) {
      const int64_t s0 = wav_sample(wav, frame0, ch);
      const int64_t s1 = wav_sample(wav, frame1, ch);
      /* interpolate_i16_sample: (s0 << 32) + (s1 - s0) * frac, >> 32. */
      const int64_t mixed = s0 * 4294967296LL + (s1 - s0) * frac;
      const int32_t sample = (int32_t)(mixed >> 32);
      out[i * 2 + ch] = (int32_t)((int64_t)sample * 65536);
    }
  }
}

/* fft_radix2 of ui_function_music_window.cpp. */
static void fft_radix2(float *re, float *im, int n) {
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      const float tr = re[i];
      re[i] = re[j];
      re[j] = tr;
      const float ti = im[i];
      im[i] = im[j];
      im[j] = ti;
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float angle = -2.0f * (float)AUDIO_PI / (float)len;
    const float wlen_re = cosf(angle);
    const float wlen_im = sinf(angle);
    for (int i = 0; i < n; i += len) {
      float w_re = 1.0f;
      float w_im = 0.0f;
      for (int j = 0; j < len / 2; ++j) {
        const int even = i + j;
        const int odd = even + len / 2;
        const float odd_re = re[odd] * w_re - im[odd] * w_im;
        const float odd_im = re[odd] * w_im + im[odd] * w_re;
        re[odd] = re[even] - odd_re;
        im[odd] = im[even] - odd_im;
        re[even] += odd_re;
        im[even] += odd_im;
        const float next_w_re = w_re * wlen_re - w_im * wlen_im;
        w_im = w_re * wlen_im + w_im * wlen_re;
        w_re = next_w_re;
      }
    }
  }
}

void audio_music_bars(const audio_wav_t *wav, uint32_t elapsed_ms,
                      int32_t out[AUDIO_MUSIC_BARS]) {
  static const int edges[AUDIO_MUSIC_BARS + 1] = {
      1,  2,  3,  4,  5,
      6,  7,  8,  10, 12,
      14, 16, 19, 22, 25,
      29, 33, 37, 42, 47,
      52, 57, 60, 62, AUDIO_MUSIC_FFT_N / 2,
  };
  const uint64_t position = (uint64_t)elapsed_ms * wav->rate / 1000u;
  const uint64_t start =
      position > AUDIO_MUSIC_FFT_N / 2 ? position - AUDIO_MUSIC_FFT_N / 2 : 0;
  size_t frames = 0;
  if (wav->samples && start < wav->frames) {
    frames = wav->frames - (size_t)start;
    if (frames > AUDIO_MUSIC_FFT_N) {
      frames = AUDIO_MUSIC_FFT_N;
    }
  }
  if (frames == 0) {
    for (int i = 0; i < AUDIO_MUSIC_BARS; i++) {
      out[i] = AUDIO_MUSIC_BAR_MIN;
    }
    return;
  }
  float re[AUDIO_MUSIC_FFT_N];
  float im[AUDIO_MUSIC_FFT_N];
  float window[AUDIO_MUSIC_FFT_N];
  for (int i = 0; i < AUDIO_MUSIC_FFT_N; ++i) {
    window[i] = 0.5f - 0.5f * cosf((2.0f * (float)AUDIO_PI * (float)i) /
                                   (AUDIO_MUSIC_FFT_N - 1));
  }
  float mean = 0.0f;
  for (int i = 0; i < AUDIO_MUSIC_FFT_N; ++i) {
    float sample = 0.0f;
    if ((size_t)i < frames) {
      int32_t mixed = 0;
      for (uint32_t ch = 0; ch < wav->channels; ++ch) {
        mixed += wav_sample(wav, (size_t)start + (size_t)i, ch);
      }
      sample = (float)mixed / (32768.0f * (float)wav->channels);
    }
    re[i] = sample;
    mean += sample;
  }
  mean /= (float)AUDIO_MUSIC_FFT_N;
  for (int i = 0; i < AUDIO_MUSIC_FFT_N; ++i) {
    re[i] = (re[i] - mean) * window[i];
    im[i] = 0.0f;
  }
  fft_radix2(re, im, AUDIO_MUSIC_FFT_N);
  for (int band = 0; band < AUDIO_MUSIC_BARS; ++band) {
    float energy = 0.0f;
    int count = 0;
    for (int bin = edges[band]; bin < edges[band + 1]; ++bin) {
      energy += re[bin] * re[bin] + im[bin] * im[bin];
      ++count;
    }
    const float rms = sqrtf(energy / (float)(count > 0 ? count : 1)) /
                      ((float)AUDIO_MUSIC_FFT_N * 0.5f);
    const float db = 20.0f * log10f(rms + 0.000001f);
    float norm = (db + 68.0f) / 48.0f;
    if (norm < 0.0f) {
      norm = 0.0f;
    } else if (norm > 1.0f) {
      norm = 1.0f;
    }
    out[band] =
        AUDIO_MUSIC_BAR_MIN +
        (int32_t)(norm * (float)(AUDIO_MUSIC_BAR_MAX - AUDIO_MUSIC_BAR_MIN));
  }
}
