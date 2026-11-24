/*
 * SPDX-FileCopyrightText: 2025 exia
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* The one asynchronous player of the factory's audio_playback: a task
 * ("audio_play") streams one item to the ES8311 and ends; a second start
 * while it runs fails with ESP_ERR_INVALID_STATE.
 *
 * End of an item (audio_playback.cpp): volume 10, 20 ms, 200 ms of silence
 * written, 250 ms wait, the user volume back. A stop: volume 0, 20 ms,
 * 150 ms of silence, 200 ms, then volume 0, 30 ms, 200 ms of silence,
 * 250 ms, 30 ms, 100 ms of silence, 150 ms and the volume back (about 1 s).
 *
 * Progress counts the frames written to I2S (ahead of the speaker by up to
 * the 96 ms DMA ring) and stays valid after the item ended, until the next
 * start. */
#pragma once

#include "audio_dsp.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A 16-bit WAV converted on the fly to 16 kHz, 2 x 32 bits
 * (write_stream_converted_i16). `generation` (optional) receives the id of
 * this playback. */
esp_err_t audio_player_play_wav(const audio_wav_t *wav, uint32_t *generation);
/* Raw I2S frames (16 kHz, 2 x 32 bits) in 4096-byte writes. On ESP_OK the
 * player owns `frames` (heap_caps_malloc) and frees it. */
esp_err_t audio_player_play_frames(uint8_t *frames, size_t bytes,
                                   uint32_t *generation);
/* Asks the task to stop and waits for it to end, up to 3 s
 * (ESP_ERR_TIMEOUT). ESP_OK when nothing plays. */
esp_err_t audio_player_stop(void);
bool audio_player_running(void);
/* The playback `generation` is the one running. */
bool audio_player_running_id(uint32_t generation);
/* get_playback_progress: false before the first item. */
bool audio_player_progress(uint32_t *elapsed_ms, uint32_t *duration_ms);
