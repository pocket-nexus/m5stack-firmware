// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
// SPDX-License-Identifier: MIT
//
// StickS3 speaker. The ES8311 register writes and the amplifier enable on PMIC
// G3 are M5Unified src/M5Unified.inl (_speaker_enabled_cb_sticks3); the I2S
// pins and port follow the board_M5StickS3 speaker configuration there
// (I2S0, 16-bit stereo, MCLK 18, BCLK 17, WS 15, DOUT 14). The tone generator
// replaces M5Unified's Speaker_Class mixer with one sine voice.

#include "m5_sticks3/speaker.h"
#include "m5_sticks3/board.h"
#include "m5_sticks3/pm1.h"

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "m5_sticks3_spk";

#define SPK_SAMPLE_RATE 22050
#define SPK_BLOCK_FRAMES 256
#define SPK_TABLE_SIZE 256
#define ES8311_TIMEOUT_MS 50

typedef struct {
  uint32_t frequency_hz;
  uint32_t duration_ms;
} tone_request_t;

static i2s_chan_handle_t s_tx;
static i2c_master_dev_handle_t s_codec;
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static volatile uint8_t s_volume = 100;
static int16_t s_sine[SPK_TABLE_SIZE];

// clang-format off
static const uint8_t k_es8311_playback[][2] = {
    {0x00, 0x80}, // RESET: CSM power on
    {0x01, 0xB5}, // CLOCK_MANAGER: MCLK from BCLK
    {0x02, 0x18}, // CLOCK_MANAGER: MULT_PRE = 3
    {0x0D, 0x01}, // SYSTEM: power up analog circuitry
    {0x12, 0x00}, // SYSTEM: power up DAC
    {0x13, 0x10}, // SYSTEM: enable output to HP drive
    {0x32, 0xBF}, // DAC volume, 0xBF = 0 dB
    {0x37, 0x08}, // DAC: bypass equalizer
};
// clang-format on

static esp_err_t codec_write(uint8_t reg, uint8_t value) {
  const uint8_t buffer[2] = {reg, value};
  return i2c_master_transmit(s_codec, buffer, sizeof(buffer),
                             ES8311_TIMEOUT_MS);
}

static esp_err_t codec_start(void) {
  if (!s_codec) {
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(m5_sticks3_i2c_bus(&bus), TAG, "I2C bus");
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = M5_STICKS3_ES8311_ADDR,
        .scl_speed_hz = M5_STICKS3_I2C_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &s_codec), TAG,
                        "ES8311 device");
  }
  for (size_t i = 0; i < sizeof(k_es8311_playback) / 2; ++i) {
    // M5Unified retries each bulk write up to three times.
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 3 && err != ESP_OK; ++attempt) {
      err = codec_write(k_es8311_playback[i][0], k_es8311_playback[i][1]);
    }
    ESP_RETURN_ON_ERROR(err, TAG, "ES8311 reg 0x%02x", k_es8311_playback[i][0]);
  }
  return ESP_OK;
}

static void play(const tone_request_t *request, int16_t *block) {
  // Volume follows M5Unified's quadratic master volume (volume^2 / 255^2).
  const uint32_t volume = s_volume;
  const int32_t gain = (int32_t)(volume * volume);
  const uint32_t step =
      (uint32_t)(((uint64_t)request->frequency_hz << 32) / SPK_SAMPLE_RATE);
  uint32_t phase = 0;
  uint32_t remaining =
      (uint32_t)((uint64_t)request->duration_ms * SPK_SAMPLE_RATE / 1000);
  while (remaining > 0) {
    const uint32_t frames =
        remaining < SPK_BLOCK_FRAMES ? remaining : SPK_BLOCK_FRAMES;
    for (uint32_t i = 0; i < frames; ++i) {
      const int32_t sample = (int32_t)s_sine[phase >> 24] * gain / (255 * 255);
      block[2 * i] = (int16_t)sample;
      block[2 * i + 1] = (int16_t)sample;
      phase += step;
    }
    size_t written = 0;
    if (i2s_channel_write(s_tx, block, frames * 2 * sizeof(int16_t), &written,
                          portMAX_DELAY) != ESP_OK) {
      return;
    }
    remaining -= frames;
    // A newer request replaces this tone.
    if (uxQueueMessagesWaiting(s_queue) > 0) {
      return;
    }
  }
}

static void tone_task(void *arg) {
  (void)arg;
  static int16_t block[SPK_BLOCK_FRAMES * 2];
  tone_request_t request;
  for (;;) {
    if (xQueueReceive(s_queue, &request, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (request.frequency_hz == 0) {
      continue;
    }
    play(&request, block);
  }
}

esp_err_t m5_sticks3_speaker_init(void) {
  if (s_tx) {
    return ESP_OK;
  }
  for (int i = 0; i < SPK_TABLE_SIZE; ++i) {
    s_sine[i] = (int16_t)(32767.0f * sinf(2.0f * (float)M_PI * (float)i /
                                          (float)SPK_TABLE_SIZE));
  }
  i2s_chan_config_t chan =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  // Underflow sends silence instead of repeating the last DMA buffer.
  chan.auto_clear_after_cb = true;
  ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, &s_tx, NULL), TAG, "I2S channel");
  const i2s_std_config_t std = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SPK_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                      I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = M5_STICKS3_I2S_MCLK,
              .bclk = M5_STICKS3_I2S_BCLK,
              .ws = M5_STICKS3_I2S_WS,
              .dout = M5_STICKS3_I2S_DOUT,
              .din = I2S_GPIO_UNUSED,
          },
  };
  esp_err_t err = i2s_channel_init_std_mode(s_tx, &std);
  if (err == ESP_OK) {
    err = i2s_channel_enable(s_tx);
  }
  // The codec takes its master clock from BCLK, so it is programmed while the
  // clock runs.
  if (err == ESP_OK) {
    err = codec_start();
  }
  if (err == ESP_OK) {
    err = m5_sticks3_pm1_set_gpio_output(M5_STICKS3_PM1_GPIO3, true);
  }
  if (err == ESP_OK) {
    s_queue = xQueueCreate(1, sizeof(tone_request_t));
    err = s_queue ? ESP_OK : ESP_ERR_NO_MEM;
  }
  if (err == ESP_OK && xTaskCreate(tone_task, "m5_sticks3_spk", 3072, NULL, 5,
                                   &s_task) != pdPASS) {
    err = ESP_ERR_NO_MEM;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "speaker init: %s", esp_err_to_name(err));
    m5_sticks3_speaker_deinit();
  }
  return err;
}

void m5_sticks3_speaker_set_volume(uint8_t volume) { s_volume = volume; }

uint8_t m5_sticks3_speaker_volume(void) { return s_volume; }

esp_err_t m5_sticks3_speaker_tone(uint32_t frequency_hz, uint32_t duration_ms) {
  ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_INVALID_STATE, TAG, "not started");
  const tone_request_t request = {frequency_hz, duration_ms};
  xQueueOverwrite(s_queue, &request);
  return ESP_OK;
}

void m5_sticks3_speaker_deinit(void) {
  if (s_task) {
    vTaskDelete(s_task);
    s_task = NULL;
  }
  if (s_queue) {
    vQueueDelete(s_queue);
    s_queue = NULL;
  }
  m5_sticks3_pm1_set_gpio_output(M5_STICKS3_PM1_GPIO3, false);
  if (s_tx) {
    i2s_channel_disable(s_tx);
    i2s_del_channel(s_tx);
    s_tx = NULL;
  }
  if (s_codec) {
    i2c_master_bus_rm_device(s_codec);
    s_codec = NULL;
  }
}
