/*
 * SPDX-FileCopyrightText: 2025 exia
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* ES8311 DAC (0x18) and ES7210 ADC (0x40) on I2S0, as the factory's
 * otool_audio (audio_tools.cpp, audio_es_es8311.cpp, audio_es_es7210.cpp)
 * and app_main bring them up.
 *
 * I2S0 is the master of both codecs, full duplex, Philips standard mode in
 * both directions: 16 kHz, two 32-bit slots per frame, MCLK 4.096 MHz
 * (256 fs) on GPIO2, BCLK GPIO6, WS GPIO4, data out GPIO3 (ES8311 DSDIN),
 * data in GPIO5 (ES7210 SDOUT1). The DMA rings hold 6 x 256 frames per
 * direction (96 ms) in internal RAM.
 *
 * The ES7210 runs all four microphones in TDM with 16 bits per microphone,
 * so a received frame carries four samples:
 *   left 32-bit word:  bits 31..16 MIC1, bits 15..0 MIC3
 *   right 32-bit word: bits 31..16 MIC2, bits 15..0 MIC4
 * MIC3 is the hardware loopback of the speaker signal (the factory's "REF").
 * Gains after init: MIC1 27 dB, MIC2 27 dB, MIC3 33 dB, MIC4 27 dB, all
 * unmuted.
 *
 * A written frame is two 32-bit samples (left, right); the ES8311 is a mono
 * DAC and plays the left slot. The AW8737A amplifier is not driven here:
 * it is M5_COREP4X_SWITCH_SPEAKER. The codec supply (M5_COREP4X_SWITCH_AUDIO)
 * must be on before m5_corep4x_audio_init.
 *
 * The functions are thread-safe. m5_corep4x_audio_read and
 * m5_corep4x_audio_write may run on two tasks at once; deinit waits for a
 * read or write in progress to return. */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M5_COREP4X_AUDIO_SAMPLE_RATE 16000u
/* Bytes of one I2S frame in either direction: two 32-bit slots. */
#define M5_COREP4X_AUDIO_FRAME_BYTES 8u

/* ES7210 microphone gain codes (registers 0x43..0x46 bits 3:0): code n is
 * 3n dB up to 33 dB, then 34.5, 36 and 37.5 dB. */
#define M5_COREP4X_MIC_GAIN_27DB 9u
#define M5_COREP4X_MIC_GAIN_33DB 11u

/* Creates the I2S0 channels and the two codec devices and opens them at
 * 16 kHz, 2 x 32 bits: the ES8311 at `volume` (0..100), the ES7210 at 30 dB
 * for every microphone, then the per-microphone gains above. ESP_OK at once
 * when already initialised. On failure everything created is released. */
esp_err_t m5_corep4x_audio_init(int volume);
/* Closes and deletes both codec devices and releases I2S0, then sets the
 * I2S pins (GPIO 6, 2, 3, 5, 4) to inputs without pulls. `for_sleep` keeps
 * the I2S channels allocated and only disables them, as the factory's deep
 * sleep path does. Safe to call when the audio was never initialised. */
esp_err_t m5_corep4x_audio_deinit(bool for_sleep);
bool m5_corep4x_audio_ready(void);

/* Blocking transfers of whole frames (bytes a multiple of 8). */
esp_err_t m5_corep4x_audio_write(const void *frames, size_t bytes);
esp_err_t m5_corep4x_audio_read(void *frames, size_t bytes);

/* ES8311 output volume 0..100 (esp_codec_dev curve: -50 dB + 0.5 dB per
 * step, 0 = -96 dB). */
esp_err_t m5_corep4x_audio_set_volume(int volume);
/* One microphone (0..3 = MIC1..MIC4): gain code 0..14, unmuted. */
esp_err_t m5_corep4x_audio_set_mic_gain(unsigned mic, unsigned gain_code);

#ifdef __cplusplus
}
#endif
