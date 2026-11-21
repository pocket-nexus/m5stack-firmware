/* Speaker and microphones of the factory demo: tones, the startup, shutdown
 * and music clips, the 3 s record-and-play test and the waveform columns of
 * the audio windows. Playback and capture run on tasks of their own. */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAB5_SOUND_COLUMNS 60

typedef enum {
  TAB5_CLIP_STARTUP,
  TAB5_CLIP_SHUTDOWN,
  TAB5_CLIP_MUSIC,
} tab5_clip_t;

esp_err_t tab5_sound_start(void);
/* 0 mutes; the level applies from the next sound. */
void tab5_sound_set_volume(int percent);
/* Notes (MIDI numbers) of `ms` each, as the factory audio::play_* helpers
 * synthesize them; dropped while another sound plays. */
void tab5_sound_tones(const int *notes, size_t count, int ms);
void tab5_sound_clip(tab5_clip_t clip);
void tab5_sound_stop_music(void);
bool tab5_sound_music_playing(void);
/* A clip or tone is playing. */
bool tab5_sound_busy(void);
/* 1 both microphones, 2 the headphone microphone. */
void tab5_sound_record_test(int source);
/* 0 idle, 1 recording, 2 playing back. */
int tab5_sound_record_state(void);
/* 0 off, 1 both microphones, 2 headphone microphone, 3 speaker loopback. */
void tab5_sound_capture(int source);
/* Copies the newest columns (0..100, oldest first); returns false when
 * nothing changed since the last call. */
bool tab5_sound_columns(float left[TAB5_SOUND_COLUMNS],
                        float right[TAB5_SOUND_COLUMNS]);
