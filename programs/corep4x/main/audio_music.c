/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* MUSIC window, board side: refresh_music_status, play_task,
 * play_button_event_cb and compute_fft_band_heights of
 * ui_function_music_window.cpp (M5CoreP4X-UserDemo). Published every
 * 50 ms while the window is open: musicState (music.ts: 0 ready,
 * 1 preparing, 2 playing, 3 finished, 4 paused, 5 other audio, 6 not
 * ready), musicElapsedMs, musicDurationMs and, while playing, musicBars
 * (the 24 target heights; the app smooths them). */
#include "audio_player.h"
#include "audio_svc.h"
#include "esp_log.h"
#include "m5_pocket/bridge.h"

static const char *TAG = "audio_music";

#define MUSIC_DURATION_MS 45000u

enum {
  MUSIC_READY = 0,
  MUSIC_PREPARING = 1,
  MUSIC_PLAYING = 2,
  MUSIC_FINISHED = 3,
  MUSIC_PAUSED = 4,
  MUSIC_OTHER = 5,
  MUSIC_NOT_READY = 6,
};

/* s_music_page; guarded by audio_lock(). */
static struct {
  bool open;
  bool task_running;
  bool play_started_known;
  uint32_t play_started_ms;
  bool owns_playback;
  uint32_t generation;
  bool was_playing;
  bool stop_requested;
  bool paused_by_user;
  bool finished;
} s_music;

static struct {
  int state;
  int elapsed;
  int duration;
} s_published = {-1, -1, -1};

static void publish(int state, int elapsed, int duration) {
  if (state != s_published.state) {
    s_published.state = state;
    m5_pocket_bridge_set_number("musicState", state);
  }
  if (elapsed != s_published.elapsed) {
    s_published.elapsed = elapsed;
    m5_pocket_bridge_set_number("musicElapsedMs", elapsed);
  }
  if (duration != s_published.duration) {
    s_published.duration = duration;
    m5_pocket_bridge_set_number("musicDurationMs", duration);
  }
}

void audio_music_open(bool open) {
  audio_lock();
  s_music.open = open;
  s_music.owns_playback = false;
  if (open) {
    s_music.was_playing = false;
    s_music.stop_requested = false;
    s_music.paused_by_user = false;
    s_music.finished = false;
    s_music.play_started_known = false;
  }
  audio_unlock();
  if (open) {
    audio_ui_wake();
  }
}

bool audio_music_is_open(void) { return s_music.open; }

void audio_music_play(void) {
  audio_lock();
  /* One button: PLAY while the track plays is PAUSE. */
  const bool pause =
      s_music.owns_playback && audio_player_running_id(s_music.generation);
  audio_unlock();
  if (pause) {
    audio_music_stop();
    return;
  }
  audio_lock();
  const bool start = s_music.open && !s_music.task_running;
  if (start) {
    s_music.task_running = true;
    s_music.play_started_known = false;
    s_music.paused_by_user = false;
    s_music.finished = false;
  }
  audio_unlock();
  if (!start) {
    return;
  }
  if (!audio_post(AUDIO_CMD_MUSIC_PLAY)) {
    audio_lock();
    s_music.task_running = false;
    audio_unlock();
    return;
  }
  audio_ui_wake();
}

void audio_music_stop(void) {
  audio_lock();
  const bool stop = s_music.open && !s_music.task_running &&
                    s_music.owns_playback &&
                    audio_player_running_id(s_music.generation);
  if (stop) {
    s_music.stop_requested = true;
    s_music.finished = false;
  }
  audio_unlock();
  if (stop && !audio_post(AUDIO_CMD_MUSIC_STOP)) {
    audio_lock();
    s_music.stop_requested = false;
    audio_unlock();
  }
}

/* play_task: stop other playback, the amplifier on, CandyWind. */
void audio_music_ctl_play(void) {
  const audio_wav_t *track = audio_track_candy_wind();
  esp_err_t err = ESP_ERR_INVALID_STATE;
  uint32_t generation = 0;
  if (audio_ready() && track->samples) {
    err = ESP_OK;
    if (audio_player_running()) {
      err = audio_player_stop();
      audio_lock();
      s_music.owns_playback = false;
      audio_unlock();
    }
    if (err == ESP_OK) {
      audio_amplifier_on();
      err = audio_player_play_wav(track, &generation);
    }
  }
  audio_lock();
  const bool started = err == ESP_OK;
  s_music.owns_playback = started;
  s_music.generation = generation;
  if (started) {
    s_music.play_started_ms = audio_now_ms();
    s_music.play_started_known = true;
    s_music.stop_requested = false;
    s_music.paused_by_user = false;
    s_music.finished = false;
  }
  s_music.task_running = false;
  audio_unlock();
  if (!started) {
    ESP_LOGW(TAG, "CandyWind: %s", esp_err_to_name(err));
  }
  audio_ui_wake();
}

/* play_button_event_cb while playing: PAUSE stops the track. */
void audio_music_ctl_stop(void) {
  const esp_err_t err = audio_player_stop();
  audio_lock();
  if (err == ESP_OK) {
    s_music.owns_playback = false;
    s_music.play_started_known = false;
    s_music.paused_by_user = true;
    s_music.was_playing = false;
  } else {
    s_music.stop_requested = false;
  }
  audio_unlock();
  audio_ui_wake();
}

void audio_music_tick(void) {
  audio_lock();
  if (!s_music.open) {
    audio_unlock();
    return;
  }
  const bool ready = audio_ready() && audio_track_candy_wind()->samples;
  const bool running = audio_player_running();
  const bool playing =
      s_music.owns_playback && audio_player_running_id(s_music.generation);
  const bool other = running && !playing;
  const uint32_t now = audio_now_ms();
  if (playing && !s_music.was_playing) {
    if (!s_music.play_started_known) {
      s_music.play_started_ms = now;
      s_music.play_started_known = true;
    }
    s_music.stop_requested = false;
    s_music.paused_by_user = false;
    s_music.finished = false;
  }
  uint32_t duration = MUSIC_DURATION_MS;
  uint32_t elapsed = 0;
  uint32_t progress_elapsed = 0;
  uint32_t progress_duration = 0;
  const bool progress_valid =
      audio_player_progress(&progress_elapsed, &progress_duration);
  if (progress_valid) {
    duration = progress_duration;
    elapsed = progress_elapsed;
  } else if (s_music.play_started_known) {
    elapsed = now - s_music.play_started_ms;
    elapsed = elapsed > duration ? duration : elapsed;
  }
  if (!playing && s_music.was_playing) {
    const bool natural = !s_music.stop_requested &&
                         (progress_valid || s_music.play_started_known) &&
                         elapsed + AUDIO_TICK_MS >= duration;
    s_music.finished = natural;
    s_music.paused_by_user = !natural;
    s_music.play_started_known = natural;
    elapsed = natural ? duration : 0;
    s_music.stop_requested = false;
  }
  if (!running && s_music.owns_playback) {
    s_music.owns_playback = false;
  }
  if (!playing && !s_music.finished && !s_music.paused_by_user) {
    elapsed = 0;
  }
  int state = MUSIC_READY;
  if (!ready) {
    state = MUSIC_NOT_READY;
  } else if (s_music.task_running) {
    state = MUSIC_PREPARING;
  } else if (other) {
    state = MUSIC_OTHER;
  } else if (playing) {
    state = MUSIC_PLAYING;
  } else if (s_music.finished) {
    state = MUSIC_FINISHED;
  } else if (s_music.paused_by_user) {
    state = MUSIC_PAUSED;
  }
  s_music.was_playing = playing;
  audio_unlock();

  publish(state, (int)elapsed, (int)duration);
  if (playing) {
    int32_t heights[AUDIO_MUSIC_BARS];
    audio_music_bars(audio_track_candy_wind(), elapsed, heights);
    float values[AUDIO_MUSIC_BARS];
    for (int i = 0; i < AUDIO_MUSIC_BARS; i++) {
      values[i] = (float)heights[i];
    }
    m5_pocket_bridge_set_array("musicBars", values, AUDIO_MUSIC_BARS);
  }
}
