/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* Sound of the factory demo, after app/apps/utils/audio/audio.cpp (play_tone)
 * and platforms/tab5/main/hal/components/hal_audio.cpp (audioRecord, the
 * record test, the music test and the sound effects) of M5Tab5-UserDemo.
 * The factory plays its MP3 clips through esp-audio-player; here libhelix
 * decodes them and a linear resampler brings 44.1 kHz to the 48 kHz I2S
 * clock the ES7210 capture shares. */
#include "sound.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "m5/tab5.h"
#include "mp3dec.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "tab5_sound";

#define SAMPLE_RATE 48000
#define TONE_AMPLITUDE (32767.0f / 5.0f)
#define TONE_FADE 200
#define MAX_NOTES 8
#define RECORD_SECONDS 3
#define CHUNK_FRAMES 960 /* 20 ms */
#define MP3_INPUT 2048

extern const uint8_t startup_sfx_start[] asm("_binary_startup_sfx_mp3_start");
extern const uint8_t startup_sfx_end[] asm("_binary_startup_sfx_mp3_end");
extern const uint8_t shutdown_sfx_start[] asm("_binary_shutdown_sfx_mp3_start");
extern const uint8_t shutdown_sfx_end[] asm("_binary_shutdown_sfx_mp3_end");
extern const uint8_t canon_in_d_start[] asm("_binary_canon_in_d_mp3_start");
extern const uint8_t canon_in_d_end[] asm("_binary_canon_in_d_mp3_end");

typedef enum { JOB_TONES, JOB_CLIP, JOB_RECORD } job_kind_t;

typedef struct {
  job_kind_t kind;
  int notes[MAX_NOTES];
  size_t count;
  int ms;
  tab5_clip_t clip;
  int source;
} job_t;

static QueueHandle_t jobs;
static SemaphoreHandle_t microphone;
static atomic_int volume = 60;
static atomic_bool busy;
static atomic_bool music_playing;
static atomic_bool music_stop;
static atomic_int record_state;
static atomic_int capture_source;
static portMUX_TYPE columns_lock = portMUX_INITIALIZER_UNLOCKED;
static float columns_left[TAB5_SOUND_COLUMNS];
static float columns_right[TAB5_SOUND_COLUMNS];
static bool columns_changed;
static int16_t *pcm; /* stereo scratch, 1152 * 2 * 2 samples */

static void write_pcm(const int16_t *samples, size_t frames) {
  m5_tab5_audio_write(samples, frames * 2 * sizeof(int16_t));
}

static void apply_volume(void) {
  m5_tab5_audio_set_volume(atomic_load(&volume));
}

/* audio::play_tone: a sine at 1/5 full scale with a 200-sample fade-out. */
static void play_tones(const job_t *job) {
  const size_t frames = (size_t)SAMPLE_RATE * (size_t)job->ms / 1000;
  int16_t *buffer =
      heap_caps_malloc(frames * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  if (!buffer)
    return;
  for (size_t n = 0; n < job->count; n++) {
    const double frequency = 440.0 * pow(2.0, (job->notes[n] - 69) / 12.0);
    for (size_t i = 0; i < frames; i++) {
      float amplitude = TONE_AMPLITUDE;
      if (i >= frames - TONE_FADE)
        amplitude *= (float)(frames - i) / TONE_FADE;
      const int16_t value =
          (int16_t)(amplitude * sinf((float)(2.0 * M_PI * frequency *
                                             (double)i / SAMPLE_RATE)));
      buffer[i * 2] = value;
      buffer[i * 2 + 1] = value;
    }
    write_pcm(buffer, frames);
  }
  heap_caps_free(buffer);
}

/* Decodes an MP3 clip and resamples it linearly to 48 kHz. */
static void play_clip(tab5_clip_t clip) {
  const uint8_t *data = startup_sfx_start;
  const uint8_t *end = startup_sfx_end;
  if (clip == TAB5_CLIP_SHUTDOWN) {
    data = shutdown_sfx_start;
    end = shutdown_sfx_end;
  } else if (clip == TAB5_CLIP_MUSIC) {
    data = canon_in_d_start;
    end = canon_in_d_end;
  }
  HMP3Decoder decoder = MP3InitDecoder();
  int16_t *decoded =
      heap_caps_malloc(1152 * 2 * sizeof(int16_t), MALLOC_CAP_INTERNAL);
  if (!decoder || !decoded) {
    ESP_LOGE(TAG, "MP3 decoder");
    if (decoder)
      MP3FreeDecoder(decoder);
    heap_caps_free(decoded);
    return;
  }
  if (clip == TAB5_CLIP_MUSIC)
    atomic_store(&music_playing, true);
  int left = (int)(end - data);
  unsigned char *input = (unsigned char *)data;
  double position = 0.0; /* source frame index of the next output frame */
  int16_t last[2] = {0, 0};
  while (left > 0 && !(clip == TAB5_CLIP_MUSIC && atomic_load(&music_stop))) {
    const int offset = MP3FindSyncWord(input, left);
    if (offset < 0)
      break;
    input += offset;
    left -= offset;
    if (MP3Decode(decoder, &input, &left, decoded, 0) != ERR_MP3_NONE) {
      if (left > 0) {
        input++;
        left--;
      }
      continue;
    }
    MP3FrameInfo info;
    MP3GetLastFrameInfo(decoder, &info);
    const int channels = info.nChans > 0 ? info.nChans : 1;
    const size_t source_frames = (size_t)info.outputSamps / (size_t)channels;
    const double step = (double)info.samprate / SAMPLE_RATE;
    size_t out = 0;
    while (position < (double)source_frames) {
      const size_t index = (size_t)position;
      const double t = position - (double)index;
      for (int c = 0; c < 2; c++) {
        const int channel = c < channels ? c : 0;
        const int16_t a =
            index == 0 ? last[c] : decoded[(index - 1) * channels + channel];
        const int16_t b = decoded[index * channels + channel];
        pcm[out * 2 + c] = (int16_t)(a + (b - a) * t);
      }
      out++;
      position += step;
    }
    position -= (double)source_frames;
    for (int c = 0; c < 2; c++)
      last[c] =
          decoded[(source_frames - 1) * channels + (c < channels ? c : 0)];
    write_pcm(pcm, out);
  }
  MP3FreeDecoder(decoder);
  heap_caps_free(decoded);
  if (clip == TAB5_CLIP_MUSIC) {
    atomic_store(&music_playing, false);
    atomic_store(&music_stop, false);
  }
}

/* hal_audio.cpp _rec_test_task: 3 s of the four ES7210 channels at input
 * gain 240, then [MIC-L, MIC-R] or the headphone microphone on both sides. */
static void record_test(int source) {
  const size_t frames = (size_t)SAMPLE_RATE * RECORD_SECONDS;
  int16_t *recording =
      heap_caps_malloc(frames * 4 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  int16_t *playback =
      heap_caps_malloc(frames * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  if (!recording || !playback) {
    heap_caps_free(recording);
    heap_caps_free(playback);
    return;
  }
  atomic_store(&record_state, 1);
  xSemaphoreTake(microphone, portMAX_DELAY);
  m5_tab5_audio_set_in_gain(240.0f);
  for (size_t done = 0; done < frames;) {
    const size_t count =
        frames - done < CHUNK_FRAMES * 4 ? frames - done : CHUNK_FRAMES * 4;
    m5_tab5_audio_read(recording + done * 4, count * 4 * sizeof(int16_t));
    done += count;
  }
  xSemaphoreGive(microphone);
  for (size_t i = 0; i < frames; i++) {
    playback[i * 2] = source == 2 ? recording[i * 4 + 3] : recording[i * 4];
    playback[i * 2 + 1] =
        source == 2 ? recording[i * 4 + 3] : recording[i * 4 + 2];
  }
  atomic_store(&record_state, 2);
  apply_volume();
  write_pcm(playback, frames);
  atomic_store(&record_state, 0);
  heap_caps_free(recording);
  heap_caps_free(playback);
}

static void player_task(void *unused) {
  (void)unused;
  job_t job;
  for (;;) {
    if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE)
      continue;
    atomic_store(&busy, true);
    apply_volume();
    if (job.kind == JOB_TONES)
      play_tones(&job);
    else if (job.kind == JOB_CLIP)
      play_clip(job.clip);
    else
      record_test(job.source);
    atomic_store(&busy, false);
  }
}

/* audioRecord: 20 ms chunks of the four channels; each becomes one column,
 * the chunk's peak in percent of full scale. */
static void capture_task(void *unused) {
  (void)unused;
  int16_t *chunk = heap_caps_malloc(CHUNK_FRAMES * 4 * sizeof(int16_t),
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  int gain_source = -1;
  for (;;) {
    const int source = atomic_load(&capture_source);
    if (!chunk || source == 0) {
      gain_source = -1;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    xSemaphoreTake(microphone, portMAX_DELAY);
    if (gain_source != source) {
      /* The factory reads the microphones at gain 80, the loopback at 20. */
      m5_tab5_audio_set_in_gain(source == 3 ? 20.0f : 80.0f);
      gain_source = source;
    }
    const esp_err_t read =
        m5_tab5_audio_read(chunk, CHUNK_FRAMES * 4 * sizeof(int16_t));
    xSemaphoreGive(microphone);
    if (read != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    const int first = source == 1 ? 0 : source == 2 ? 3 : 1;
    int peak_left = 0, peak_right = 0;
    for (size_t i = 0; i < CHUNK_FRAMES; i++) {
      const int a = abs(chunk[i * 4 + first]);
      const int b = abs(chunk[i * 4 + 2]);
      if (a > peak_left)
        peak_left = a;
      if (b > peak_right)
        peak_right = b;
    }
    taskENTER_CRITICAL(&columns_lock);
    memmove(columns_left, columns_left + 1,
            (TAB5_SOUND_COLUMNS - 1) * sizeof(float));
    memmove(columns_right, columns_right + 1,
            (TAB5_SOUND_COLUMNS - 1) * sizeof(float));
    columns_left[TAB5_SOUND_COLUMNS - 1] = peak_left * 100.0f / 32768.0f;
    columns_right[TAB5_SOUND_COLUMNS - 1] = peak_right * 100.0f / 32768.0f;
    columns_changed = true;
    taskEXIT_CRITICAL(&columns_lock);
  }
}

esp_err_t tab5_sound_start(void) {
  const esp_err_t ready = m5_tab5_audio_init();
  if (ready != ESP_OK)
    return ready;
  jobs = xQueueCreate(4, sizeof(job_t));
  microphone = xSemaphoreCreateMutex();
  pcm = heap_caps_malloc(1400 * 2 * sizeof(int16_t), MALLOC_CAP_INTERNAL);
  if (!jobs || !microphone || !pcm)
    return ESP_ERR_NO_MEM;
  if (xTaskCreatePinnedToCore(player_task, "tab5_sound", 8192, NULL, 6, NULL,
                              1) != pdPASS ||
      xTaskCreatePinnedToCore(capture_task, "tab5_capture", 4096, NULL, 4, NULL,
                              1) != pdPASS)
    return ESP_ERR_NO_MEM;
  return ESP_OK;
}

void tab5_sound_set_volume(int percent) { atomic_store(&volume, percent); }

void tab5_sound_tones(const int *notes, size_t count, int ms) {
  if (atomic_load(&volume) <= 0 || count == 0 || ms <= 0 ||
      atomic_load(&busy) || uxQueueMessagesWaiting(jobs) > 0)
    return;
  job_t job = {.kind = JOB_TONES,
               .count = count < MAX_NOTES ? count : MAX_NOTES,
               .ms = ms};
  memcpy(job.notes, notes, job.count * sizeof(int));
  xQueueSend(jobs, &job, 0);
}

void tab5_sound_clip(tab5_clip_t clip) {
  if (clip == TAB5_CLIP_MUSIC && atomic_load(&music_playing))
    return;
  const job_t job = {.kind = JOB_CLIP, .clip = clip};
  if (clip == TAB5_CLIP_MUSIC)
    atomic_store(&music_playing, true);
  xQueueSend(jobs, &job, 0);
}

void tab5_sound_stop_music(void) {
  if (atomic_load(&music_playing))
    atomic_store(&music_stop, true);
}

bool tab5_sound_music_playing(void) { return atomic_load(&music_playing); }

bool tab5_sound_busy(void) {
  return atomic_load(&busy) || uxQueueMessagesWaiting(jobs) > 0;
}

void tab5_sound_record_test(int source) {
  if (atomic_load(&record_state) != 0)
    return;
  const job_t job = {.kind = JOB_RECORD, .source = source};
  atomic_store(&record_state, 1);
  xQueueSend(jobs, &job, 0);
}

int tab5_sound_record_state(void) { return atomic_load(&record_state); }

void tab5_sound_capture(int source) {
  atomic_store(&capture_source, source);
  taskENTER_CRITICAL(&columns_lock);
  memset(columns_left, 0, sizeof(columns_left));
  memset(columns_right, 0, sizeof(columns_right));
  columns_changed = true;
  taskEXIT_CRITICAL(&columns_lock);
}

bool tab5_sound_columns(float left[TAB5_SOUND_COLUMNS],
                        float right[TAB5_SOUND_COLUMNS]) {
  taskENTER_CRITICAL(&columns_lock);
  const bool changed = columns_changed;
  if (changed) {
    memcpy(left, columns_left, sizeof(columns_left));
    memcpy(right, columns_right, sizeof(columns_right));
    columns_changed = false;
  }
  taskEXIT_CRITICAL(&columns_lock);
  return changed;
}
