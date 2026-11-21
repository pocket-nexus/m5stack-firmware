// SPDX-License-Identifier: MIT
//
// Shared state of speaker.c and mic.c: the full-duplex I2S0 controller.
#pragma once

#include "driver/i2s_std.h"

// I2S frame rate of both directions (16-bit stereo, BCLK = 32 x fs; the
// codec derives its master clock from BCLK x 8).
#define M5_STICKS3_AUDIO_SAMPLE_RATE 22050

/// The RX channel speaker.c created next to the TX channel, or NULL before
/// m5_sticks3_speaker_init(). Initialised in standard mode, not enabled.
i2s_chan_handle_t m5_sticks3_audio_rx(void);
