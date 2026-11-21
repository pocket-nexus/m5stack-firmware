// SPDX-License-Identifier: MIT
//
// StickS3 microphone level capture. The codec's ADC path (input, gain) is
// programmed by speaker.c from M5Unified's _microphone_enabled_cb_sticks3;
// this file reads the RX channel and keeps running sums for a level meter.

#include "m5_sticks3/mic.h"
#include "audio.h"

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "m5_sticks3_mic";

// 256 stereo frames: 11.6 ms at 22050 Hz.
#define MIC_BLOCK_FRAMES 256
#define MIC_READ_TIMEOUT_MS 50

typedef struct {
  double sum_squares[2]; // per slot
  int32_t peak[2];
  uint32_t frames;
} mic_window_t;

static TaskHandle_t s_task;
static SemaphoreHandle_t s_done;
static volatile bool s_running;
static mic_window_t s_window;
static portMUX_TYPE s_window_lock = portMUX_INITIALIZER_UNLOCKED;

static void accumulate(const int16_t *block, uint32_t frames) {
  double sums[2] = {0, 0};
  int32_t peaks[2] = {0, 0};
  for (uint32_t i = 0; i < frames; ++i) {
    for (int slot = 0; slot < 2; ++slot) {
      const int32_t sample = block[2 * i + slot];
      sums[slot] += (double)(sample * sample);
      const int32_t magnitude = abs(sample);
      if (magnitude > peaks[slot]) {
        peaks[slot] = magnitude;
      }
    }
  }
  portENTER_CRITICAL(&s_window_lock);
  for (int slot = 0; slot < 2; ++slot) {
    s_window.sum_squares[slot] += sums[slot];
    if (peaks[slot] > s_window.peak[slot]) {
      s_window.peak[slot] = peaks[slot];
    }
  }
  s_window.frames += frames;
  portEXIT_CRITICAL(&s_window_lock);
}

static void capture_task(void *arg) {
  (void)arg;
  static int16_t block[MIC_BLOCK_FRAMES * 2];
  const i2s_chan_handle_t rx = m5_sticks3_audio_rx();
  while (s_running) {
    size_t bytes = 0;
    const esp_err_t err =
        i2s_channel_read(rx, block, sizeof(block), &bytes, MIC_READ_TIMEOUT_MS);
    if (err == ESP_OK || err == ESP_ERR_TIMEOUT) {
      accumulate(block, (uint32_t)(bytes / (2 * sizeof(int16_t))));
    } else {
      ESP_LOGW(TAG, "read: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(MIC_READ_TIMEOUT_MS));
    }
  }
  xSemaphoreGive(s_done);
  vTaskDelete(NULL);
}

esp_err_t m5_sticks3_mic_start(void) {
  if (s_running) {
    return ESP_OK;
  }
  const i2s_chan_handle_t rx = m5_sticks3_audio_rx();
  ESP_RETURN_ON_FALSE(rx, ESP_ERR_INVALID_STATE, TAG, "speaker not started");
  if (!s_done) {
    s_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_done, ESP_ERR_NO_MEM, TAG, "semaphore");
  }
  ESP_RETURN_ON_ERROR(i2s_channel_enable(rx), TAG, "RX enable");
  portENTER_CRITICAL(&s_window_lock);
  s_window = (mic_window_t){0};
  portEXIT_CRITICAL(&s_window_lock);
  s_running = true;
  if (xTaskCreate(capture_task, "m5_sticks3_mic", 3072, NULL, 5, &s_task) !=
      pdPASS) {
    s_running = false;
    i2s_channel_disable(rx);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

void m5_sticks3_mic_stop(void) {
  if (!s_running) {
    return;
  }
  s_running = false;
  xSemaphoreTake(s_done, portMAX_DELAY);
  s_task = NULL;
  i2s_channel_disable(m5_sticks3_audio_rx());
}

bool m5_sticks3_mic_running(void) { return s_running; }

static float to_dbfs(double magnitude) {
  if (magnitude <= 0) {
    return M5_STICKS3_MIC_FLOOR_DBFS;
  }
  const float db = 20.0f * log10f((float)(magnitude / 32768.0));
  return db < M5_STICKS3_MIC_FLOOR_DBFS ? M5_STICKS3_MIC_FLOOR_DBFS : db;
}

esp_err_t m5_sticks3_mic_take_level(m5_sticks3_mic_level_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  if (!s_running) {
    return ESP_ERR_INVALID_STATE;
  }
  portENTER_CRITICAL(&s_window_lock);
  const mic_window_t window = s_window;
  s_window = (mic_window_t){0};
  portEXIT_CRITICAL(&s_window_lock);
  if (window.frames == 0) {
    return ESP_ERR_NOT_FOUND;
  }
  // The louder slot: the codec drives its mono ADC into one or both slots.
  const int slot = window.sum_squares[1] > window.sum_squares[0] ? 1 : 0;
  out->rms_dbfs = to_dbfs(sqrt(window.sum_squares[slot] / window.frames));
  out->peak_dbfs = to_dbfs((double)window.peak[slot]);
  out->frames = window.frames;
  return ESP_OK;
}
