// SPDX-License-Identifier: MIT
//
// StickS3 speaker: ES8311 DAC on I2S0 and the AW8737 amplifier behind PMIC G3.
#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Creates the full-duplex I2S0 TX and RX channels (22050 Hz, 16-bit stereo),
/// programs the ES8311 playback and capture paths (M5Unified
/// _speaker_enabled_cb_sticks3 and _microphone_enabled_cb_sticks3), enables
/// the amplifier and starts the tone task. The RX channel stays disabled
/// until m5_sticks3_mic_start(). Requires m5_sticks3_power_init().
esp_err_t m5_sticks3_speaker_init(void);

/// Master volume 0..255 (M5Unified Speaker scale; UIFlow startup sets 100).
void m5_sticks3_speaker_set_volume(uint8_t volume);
uint8_t m5_sticks3_speaker_volume(void);

/// Queues a sine tone. A new tone replaces the one playing. Returns
/// ESP_ERR_INVALID_STATE before m5_sticks3_speaker_init().
esp_err_t m5_sticks3_speaker_tone(uint32_t frequency_hz, uint32_t duration_ms);

/// Queues a sine tone that starts at esp_timer time `start_us` (an
/// esp_timer one-shot fires it); a start time that has passed plays at once.
/// One start is pending at a time: a later call replaces it.
esp_err_t m5_sticks3_speaker_tone_at(uint32_t frequency_hz,
                                     uint32_t duration_ms, int64_t start_us);

/// Stops playback and disables the amplifier and the I2S channel.
void m5_sticks3_speaker_deinit(void);

#ifdef __cplusplus
}
#endif
