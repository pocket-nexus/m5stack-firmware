/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * ES8311 audio through esp_codec_dev, ported from AudioCodec::init and
 * _i2s_init in the factory firmware (M5StopWatch-UserDemo
 * main/hal/hal_audio.cpp): I2S0 standard Philips mode, 16-bit, full duplex
 * with MCLK, codec opened at 44.1 kHz mono, input gain 30 dB. The speaker
 * amplifier follows Hal::ioe_speaker_enable (main/hal/hal_ioe.cpp).
 */
#include "board.h"

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ioe1.h"

static const char *TAG = "m5sw_audio";

static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static esp_codec_dev_handle_t s_codec;

static esp_err_t i2s_init(void) {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, &s_rx), TAG,
                      "i2s channel");
  const i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(M5_STOPWATCH_AUDIO_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                      I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = M5_STOPWATCH_PIN_I2S_MCLK,
              .bclk = M5_STOPWATCH_PIN_I2S_BCLK,
              .ws = M5_STOPWATCH_PIN_I2S_LRCK,
              .dout = M5_STOPWATCH_PIN_I2S_DOUT,
              .din = M5_STOPWATCH_PIN_I2S_DIN,
          },
  };
  ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), TAG, "tx");
  ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std_cfg), TAG, "rx");
  ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "tx enable");
  ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "rx enable");
  return ESP_OK;
}

static void i2s_deinit(void) {
  if (s_tx) {
    i2s_channel_disable(s_tx);
    i2s_del_channel(s_tx);
    s_tx = NULL;
  }
  if (s_rx) {
    i2s_channel_disable(s_rx);
    i2s_del_channel(s_rx);
    s_rx = NULL;
  }
}

esp_err_t m5_stopwatch_audio_init(void) {
  i2c_master_bus_handle_t bus = m5_stopwatch_board_i2c();
  ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_STATE, TAG, "board not started");
  ESP_RETURN_ON_FALSE(!s_codec, ESP_ERR_INVALID_STATE, TAG, "already started");

  esp_err_t err = i2s_init();
  if (err != ESP_OK) {
    i2s_deinit();
    return err;
  }

  audio_codec_i2s_cfg_t i2s_cfg = {
      .port = I2S_NUM_0,
      .rx_handle = s_rx,
      .tx_handle = s_tx,
  };
  const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
  audio_codec_i2c_cfg_t i2c_cfg = {
      .port = M5_STOPWATCH_I2C_PORT,
      .addr = ES8311_CODEC_DEFAULT_ADDR,
      .bus_handle = bus,
  };
  const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
  const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
  const audio_codec_if_t *codec_if = NULL;
  if (data_if && ctrl_if && gpio_if) {
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = -1, /* the amplifier is switched through IOE1 and GPIO14 */
        .pa_reverted = false,
        .use_mclk = true,
    };
    codec_if = es8311_codec_new(&es8311_cfg);
  }
  if (codec_if) {
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_codec = esp_codec_dev_new(&dev_cfg);
  }
  if (s_codec) {
    /* The factory firmware records at 30 dB input gain. */
    (void)esp_codec_dev_set_in_gain(s_codec, 30.0f);
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .sample_rate = M5_STOPWATCH_AUDIO_SAMPLE_RATE,
    };
    if (esp_codec_dev_open(s_codec, &fs) == ESP_CODEC_DEV_OK) {
      return ESP_OK;
    }
    esp_codec_dev_delete(s_codec);
    s_codec = NULL;
  }

  ESP_LOGE(TAG, "ES8311 codec setup failed");
  if (codec_if) {
    audio_codec_delete_codec_if(codec_if);
  }
  if (ctrl_if) {
    audio_codec_delete_ctrl_if(ctrl_if);
  }
  if (gpio_if) {
    audio_codec_delete_gpio_if(gpio_if);
  }
  if (data_if) {
    audio_codec_delete_data_if(data_if);
  }
  i2s_deinit();
  return ESP_FAIL;
}

esp_err_t m5_stopwatch_audio_set_volume(uint8_t volume) {
  ESP_RETURN_ON_FALSE(s_codec, ESP_ERR_INVALID_STATE, TAG, "not started");
  if (volume > 100) {
    volume = 100;
  }
  return esp_codec_dev_set_out_vol(s_codec, volume) == ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}

esp_err_t m5_stopwatch_audio_set_mic_gain(float db) {
  ESP_RETURN_ON_FALSE(s_codec, ESP_ERR_INVALID_STATE, TAG, "not started");
  return esp_codec_dev_set_in_gain(s_codec, db) == ESP_CODEC_DEV_OK ? ESP_OK
                                                                    : ESP_FAIL;
}

esp_err_t m5_stopwatch_audio_speaker_enable(bool enable) {
  ESP_RETURN_ON_ERROR(
      m5_stopwatch_ioe1_digital_write(M5_STOPWATCH_IOE1_SPK_PA, enable), TAG,
      "IOE1 SPK_PA");
  ESP_RETURN_ON_ERROR(gpio_set_level(M5_STOPWATCH_PIN_SPEAKER_PA, enable), TAG,
                      "GPIO14");
  vTaskDelay(pdMS_TO_TICKS(10));
  return ESP_OK;
}

esp_err_t m5_stopwatch_audio_write(const int16_t *samples, size_t count) {
  ESP_RETURN_ON_FALSE(s_codec, ESP_ERR_INVALID_STATE, TAG, "not started");
  ESP_RETURN_ON_FALSE(samples || count == 0, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(count <= INT32_MAX / sizeof(int16_t),
                      ESP_ERR_INVALID_SIZE, TAG, "too many samples");
  if (count == 0) {
    return ESP_OK;
  }
  return esp_codec_dev_write(s_codec, (void *)samples,
                             (int)(count * sizeof(int16_t))) == ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}

esp_err_t m5_stopwatch_audio_read(int16_t *samples, size_t count) {
  ESP_RETURN_ON_FALSE(s_codec, ESP_ERR_INVALID_STATE, TAG, "not started");
  ESP_RETURN_ON_FALSE(samples || count == 0, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(count <= INT32_MAX / sizeof(int16_t),
                      ESP_ERR_INVALID_SIZE, TAG, "too many samples");
  if (count == 0) {
    return ESP_OK;
  }
  return esp_codec_dev_read(s_codec, samples, (int)(count * sizeof(int16_t))) ==
                 ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}
