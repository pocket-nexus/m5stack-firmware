// SPDX-License-Identifier: MIT
//
// StickS3 microphone: the ES8311 ADC on I2S DIN (GPIO 16), captured on the
// RX side of the speaker's full-duplex I2S0 channel.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Level reported for digital silence.
#define M5_STICKS3_MIC_FLOOR_DBFS (-96.0f)

typedef struct {
  float rms_dbfs;  ///< RMS of the window, dB relative to full scale
  float peak_dbfs; ///< Largest sample magnitude of the window, dBFS
  uint32_t frames; ///< Sample frames in the window
} m5_sticks3_mic_level_t;

/// Enables the I2S RX channel and starts the capture task, which accumulates
/// the level of every block. Requires m5_sticks3_speaker_init() (codec and
/// I2S). Calling it while running does nothing.
esp_err_t m5_sticks3_mic_start(void);

/// Stops the capture task (waits for its current block, at most about 12 ms)
/// and disables the RX channel. Calling it while stopped does nothing.
void m5_sticks3_mic_stop(void);

bool m5_sticks3_mic_running(void);

/// Level of the samples captured since the previous call. Both I2S slots are
/// measured; the louder one is reported. Returns ESP_ERR_NOT_FOUND when no
/// block arrived since the previous call and ESP_ERR_INVALID_STATE when the
/// microphone is stopped.
esp_err_t m5_sticks3_mic_take_level(m5_sticks3_mic_level_t *out);

#ifdef __cplusplus
}
#endif
