/*
 * SPDX-FileCopyrightText: 2025 exia
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Signal processing of the REC and MUSIC windows, without ESP-IDF
 * dependencies (tests build it on the host). The numbers follow
 * ui_function_rec_window.cpp, ui_function_music_window.cpp and
 * audio_playback.cpp of M5CoreP4X-UserDemo. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- REC ---------------------------------------------------------------- */

/* Received frames: 8 bytes, two little-endian 32-bit slots. */
#define AUDIO_FRAME_BYTES 8u
#define AUDIO_RATE 16000u
/* REC channels shown: MIC1, MIC2, REF (= MIC3). */
#define AUDIO_REC_CHANNELS 3
/* Waveform chart: 200 points, 4 per 50 ms tick, zero line at 33 of
 * 0..100. */
#define AUDIO_WAVE_POINTS 200
#define AUDIO_WAVE_PER_TICK 4
#define AUDIO_WAVE_ZERO 33
/* Recorded preview: 133 buckets drawn from point 67 on; point 66 is the
 * play head. */
#define AUDIO_PREVIEW_ANCHOR (AUDIO_WAVE_POINTS / 3)
#define AUDIO_PREVIEW_POINTS (AUDIO_WAVE_POINTS - AUDIO_PREVIEW_ANCHOR - 1)
/* Spectrogram: 24 bins of a 256-point DFT. */
#define AUDIO_SPEC_BINS 24
#define AUDIO_SPEC_DFT_N 256

/* MIC1, MIC2, MIC3 (REF) and MIC4 of one TDM frame (audio.h). */
static inline void audio_tdm_split(const uint8_t *frame, int16_t mics[4]) {
  const uint32_t left = (uint32_t)frame[0] | (uint32_t)frame[1] << 8 |
                        (uint32_t)frame[2] << 16 | (uint32_t)frame[3] << 24;
  const uint32_t right = (uint32_t)frame[4] | (uint32_t)frame[5] << 8 |
                         (uint32_t)frame[6] << 16 | (uint32_t)frame[7] << 24;
  mics[0] = (int16_t)(left >> 16);
  mics[1] = (int16_t)(right >> 16);
  mics[2] = (int16_t)(left & 0xFFFFu);
  mics[3] = (int16_t)(right & 0xFFFFu);
}

/* sample_to_wave_y: 33 + s * 48 * 4 / 32767, clamped to 0..100. */
int16_t audio_wave_y(int32_t sample);

/* The dB label: "-inf" for peak <= 1 (and below -60 dBFS with
 * `floor_60`), else "%+.0f" of 20 log10(peak / 32767). */
void audio_db_text(int32_t peak, bool floor_60, char out[8]);

/* The values one captured chunk gives each shown channel
 * (update_latest_audio_metrics_from_raw). */
typedef struct {
  int16_t peak[AUDIO_REC_CHANNELS];
  int16_t rms[AUDIO_REC_CHANNELS];
  /* Wave points: the samples at frames 0, (n-1)/3, 2(n-1)/3, n-1. */
  int16_t wave_y[AUDIO_REC_CHANNELS][AUDIO_WAVE_PER_TICK];
  /* The first 256 samples of each channel, for the DFT. */
  int16_t spectrum_samples[AUDIO_REC_CHANNELS][AUDIO_SPEC_DFT_N];
  size_t spectrum_count[AUDIO_REC_CHANNELS];
} audio_chunk_t;

/* `frames` whole frames; `spectrum` keeps DFT samples of MIC1 and MIC2,
 * and of REF too when `ref`. */
void audio_chunk_measure(const uint8_t *frames, size_t count, bool spectrum,
                         bool ref, audio_chunk_t *out);

/* DFT tables of the 24 bins at k = 1..63 (62.5 Hz .. 3937.5 Hz) and the
 * Hann window: 50,176 bytes. */
typedef struct {
  float cos_tbl[AUDIO_SPEC_BINS][AUDIO_SPEC_DFT_N];
  float sin_tbl[AUDIO_SPEC_BINS][AUDIO_SPEC_DFT_N];
  float window[AUDIO_SPEC_DFT_N];
} audio_dft_t;

void audio_dft_init(audio_dft_t *tables);
/* compute_spectrum_levels: 0 = -78 dBFS, 255 = -22 dBFS; all 0 with fewer
 * than 256 samples. */
void audio_spectrum_levels(const audio_dft_t *tables, const int16_t *samples,
                           size_t count, uint8_t out[AUDIO_SPEC_BINS]);

/* spec_color_from_level as 0xRRGGBB. */
uint32_t audio_spectrum_color(uint8_t level);

/* The recorded preview: each of 133 buckets keeps the signed sample of the
 * largest magnitude, and each channel its peak. */
typedef struct {
  int16_t sample[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS];
  int32_t magnitude[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS];
  int32_t peak[AUDIO_REC_CHANNELS];
  size_t offset;
  size_t total;
} audio_preview_acc_t;

void audio_preview_begin(audio_preview_acc_t *acc, size_t total_frames);
void audio_preview_feed(audio_preview_acc_t *acc, const uint8_t *frames,
                        size_t count);
/* Bucket samples as wave points and the per-channel peaks (<= 32767). */
void audio_preview_finish(
    const audio_preview_acc_t *acc,
    int16_t wave_y[AUDIO_REC_CHANNELS][AUDIO_PREVIEW_POINTS],
    int16_t peak[AUDIO_REC_CHANNELS]);

/* ---- playback ----------------------------------------------------------- */

/* A RIFF/WAVE file with 16-bit PCM (extract_wave_pcm_payload). */
typedef struct {
  const int16_t *samples;
  size_t frames;
  uint32_t rate;
  uint32_t channels;
} audio_wav_t;

bool audio_wav_parse(const uint8_t *data, size_t size, audio_wav_t *out);

/* write_stream_converted_i16 at 16 kHz stereo 32-bit: the output frame
 * count (rounded), and `count` output frames from `first` on, linearly
 * interpolated with a 32.32 phase step, mono duplicated, sample * 65536. */
uint64_t audio_convert_length(const audio_wav_t *wav);
void audio_convert(const audio_wav_t *wav, uint64_t first, size_t count,
                   int32_t *out);

/* ---- MUSIC -------------------------------------------------------------- */

#define AUDIO_MUSIC_BARS 24
#define AUDIO_MUSIC_FFT_N 128
#define AUDIO_MUSIC_BAR_MIN 18
#define AUDIO_MUSIC_BAR_MAX 122

/* compute_fft_band_heights: 128 frames from max(position - 64, 0) of the
 * track at `elapsed_ms`, frames past the end zero; heights 18..122, all 18
 * when nothing can be read. */
void audio_music_bars(const audio_wav_t *wav, uint32_t elapsed_ms,
                      int32_t out[AUDIO_MUSIC_BARS]);
