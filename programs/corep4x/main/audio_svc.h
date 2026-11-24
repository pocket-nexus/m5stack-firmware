/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Internals shared by the audio service modules: svc_audio.c (requests,
 * the control and UI tasks, start-up, rails, sleep), audio_player.c (the
 * one playback task), audio_music.c (MUSIC) and audio_rec.c (REC).
 *
 * Tasks, all on core 1 (the app task runs on core 0; the PPA lane helper
 * shares core 1 at priority 5):
 *   audio_ctl   5  commands that block: codec bring-up, PA and boot sound
 *                  timing, MUSIC play/stop, REC file restore and delete,
 *                  rail changes (internal stack: SPIFFS)
 *   audio_ui    3  every 50 ms while REC or MUSIC is open: REC charts,
 *                  spectrograms and the card picture, MUSIC state and bars
 *   audio_play  5  the playback stream (audio_player.c)
 *   rec_mon     4  the REC microphone monitor
 *   func_rec    5  a REC recording, then the SPIFFS write
 *   rec_play    5  a REC playback
 * State shared between tasks is guarded by audio_lock(); none of the
 * blocking work runs with it held. */
#pragma once

#include "audio_dsp.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>

#define AUDIO_CORE 1
#define AUDIO_PRIORITY 5
#define AUDIO_MONITOR_PRIORITY 4
#define AUDIO_UI_PRIORITY 3
/* The UI tick of the factory's LVGL timers. */
#define AUDIO_TICK_MS 50

/* svc_audio.c */
void audio_lock(void);
void audio_unlock(void);
uint32_t audio_now_ms(void);
/* The codec is up and no deep sleep preparation started. */
bool audio_ready(void);
/* The volume the user set (0..100); playback restores it after its fades. */
int audio_volume(void);
/* set_pa_power(true): the AW8737A on, published as railAudioPA. */
void audio_amplifier_on(void);
/* The embedded tracks (NULL samples when a WAV header did not parse). */
const audio_wav_t *audio_track_candy_wind(void);
const audio_wav_t *audio_track_startup(void);
/* Wakes the UI task for a tick now (a window opened, a mode changed). */
void audio_ui_wake(void);

typedef enum {
  AUDIO_CMD_MUSIC_PLAY,
  AUDIO_CMD_MUSIC_STOP,
  AUDIO_CMD_REC_RESTORE,
  AUDIO_CMD_REC_DELETE,
  AUDIO_CMD_RAILS,
} audio_cmd_kind_t;

typedef struct {
  audio_cmd_kind_t kind;
  bool codec_on;
  bool amplifier_on;
} audio_cmd_t;

/* Queues a command for audio_ctl; false when the queue is full. */
bool audio_post(audio_cmd_kind_t kind);

/* audio_music.c */
void audio_music_open(bool open);
bool audio_music_is_open(void);
void audio_music_play(void);
void audio_music_stop(void);
void audio_music_tick(void);
void audio_music_ctl_play(void);
void audio_music_ctl_stop(void);

/* audio_rec.c */
void audio_rec_open(bool open);
bool audio_rec_is_open(void);
void audio_rec_view(bool spectrum, bool show_ref);
void audio_rec_start(void);
void audio_rec_end(void);
void audio_rec_play(void);
void audio_rec_play_stop(void);
void audio_rec_tick(void);
void audio_rec_ctl_restore(void);
void audio_rec_ctl_delete(void);
/* Stops recording, playback and the monitor and waits up to timeout_ms for
 * their tasks to end (rails off, deep sleep). A recording is discarded. */
void audio_rec_halt(uint32_t timeout_ms);
/* The app task's texture upload (svc_audio frame()). */
void audio_rec_upload(void);
