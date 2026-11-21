/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* ES8388 DAC and ES7210 ADC on one I2S port, from bsp_audio_init,
 * bsp_audio_codec_speaker_init, bsp_audio_codec_microphone_init and
 * bsp_codec_init in m5stack_tab5.c of M5Tab5-UserDemo. The amplifier enable
 * is expander 0x43 P1 (M5_TAB5_SWITCH_SPEAKER), not a codec pin. */
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "m5/tab5.h"

static const char *TAG = "m5_tab5_audio";

#define SAMPLE_RATE 48000
#define I2S_PORT I2S_NUM_1

static i2s_chan_handle_t tx_channel, rx_channel;
static esp_codec_dev_handle_t speaker, microphone;

static esp_err_t init_i2s(void) {
  i2s_chan_config_t channels =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
  channels.auto_clear = true;
  ESP_RETURN_ON_ERROR(i2s_new_channel(&channels, &tx_channel, &rx_channel), TAG,
                      "I2S channels");
  const i2s_std_gpio_config_t gpio = {
      .mclk = M5_TAB5_I2S_MCLK,
      .bclk = M5_TAB5_I2S_BCLK,
      .ws = M5_TAB5_I2S_WS,
      .dout = M5_TAB5_I2S_DOUT,
      .din = M5_TAB5_I2S_DIN,
  };
  const i2s_std_config_t std = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                      I2S_SLOT_MODE_MONO),
      .gpio_cfg = gpio,
  };
  ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_channel, &std), TAG,
                      "TX mode");
  ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_channel), TAG, "TX enable");
  const i2s_tdm_config_t tdm = {
      .clk_cfg =
          {
              .sample_rate_hz = SAMPLE_RATE,
              .clk_src = I2S_CLK_SRC_DEFAULT,
              .mclk_multiple = I2S_MCLK_MULTIPLE_256,
              .bclk_div = 8,
          },
      .slot_cfg =
          {
              .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
              .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
              .slot_mode = I2S_SLOT_MODE_STEREO,
              .slot_mask =
                  I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3,
              .ws_width = I2S_TDM_AUTO_WS_WIDTH,
              .ws_pol = false,
              .bit_shift = true,
              .left_align = false,
              .big_endian = false,
              .bit_order_lsb = false,
              .skip_mask = false,
              .total_slot = I2S_TDM_AUTO_SLOT_NUM,
          },
      .gpio_cfg =
          {
              .mclk = M5_TAB5_I2S_MCLK,
              .bclk = M5_TAB5_I2S_BCLK,
              .ws = M5_TAB5_I2S_WS,
              .dout = M5_TAB5_I2S_DOUT,
              .din = M5_TAB5_I2S_DIN,
          },
  };
  ESP_RETURN_ON_ERROR(i2s_channel_init_tdm_mode(rx_channel, &tdm), TAG,
                      "RX mode");
  return i2s_channel_enable(rx_channel);
}

esp_err_t m5_tab5_audio_init(void) {
  if (speaker && microphone)
    return ESP_OK;
  ESP_RETURN_ON_ERROR(m5_tab5_init(), TAG, "board");
  ESP_RETURN_ON_ERROR(init_i2s(), TAG, "I2S");
  audio_codec_i2s_cfg_t i2s = {
      .port = I2S_PORT,
      .tx_handle = tx_channel,
      .rx_handle = rx_channel,
  };
  const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&i2s);
  ESP_RETURN_ON_FALSE(data, ESP_FAIL, TAG, "codec data interface");

  audio_codec_i2c_cfg_t es8388_i2c = {
      .port = M5_TAB5_I2C_PORT,
      .addr = ES8388_CODEC_DEFAULT_ADDR,
      .bus_handle = m5_tab5_i2c(),
  };
  const audio_codec_ctrl_if_t *es8388_ctrl =
      audio_codec_new_i2c_ctrl(&es8388_i2c);
  ESP_RETURN_ON_FALSE(es8388_ctrl, ESP_FAIL, TAG, "ES8388 control");
  es8388_codec_cfg_t es8388 = {
      .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
      .master_mode = false,
      .ctrl_if = es8388_ctrl,
      .pa_pin = -1,
  };
  const audio_codec_if_t *es8388_if = es8388_codec_new(&es8388);
  ESP_RETURN_ON_FALSE(es8388_if, ESP_FAIL, TAG, "ES8388");
  esp_codec_dev_cfg_t speaker_config = {
      .dev_type = ESP_CODEC_DEV_TYPE_OUT,
      .codec_if = es8388_if,
      .data_if = data,
  };
  speaker = esp_codec_dev_new(&speaker_config);
  ESP_RETURN_ON_FALSE(speaker, ESP_FAIL, TAG, "speaker");

  audio_codec_i2c_cfg_t es7210_i2c = {
      .port = M5_TAB5_I2C_PORT,
      .addr = ES7210_CODEC_DEFAULT_ADDR,
      .bus_handle = m5_tab5_i2c(),
  };
  const audio_codec_ctrl_if_t *es7210_ctrl =
      audio_codec_new_i2c_ctrl(&es7210_i2c);
  ESP_RETURN_ON_FALSE(es7210_ctrl, ESP_FAIL, TAG, "ES7210 control");
  es7210_codec_cfg_t es7210 = {
      .ctrl_if = es7210_ctrl,
      .mic_selected =
          ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4,
  };
  const audio_codec_if_t *es7210_if = es7210_codec_new(&es7210);
  ESP_RETURN_ON_FALSE(es7210_if, ESP_FAIL, TAG, "ES7210");
  esp_codec_dev_cfg_t microphone_config = {
      .dev_type = ESP_CODEC_DEV_TYPE_IN,
      .codec_if = es7210_if,
      .data_if = data,
  };
  microphone = esp_codec_dev_new(&microphone_config);
  ESP_RETURN_ON_FALSE(microphone, ESP_FAIL, TAG, "microphone");

  esp_codec_dev_sample_info_t input = {
      .sample_rate = SAMPLE_RATE,
      .channel = 4,
      .bits_per_sample = 16,
  };
  ESP_RETURN_ON_FALSE(esp_codec_dev_open(microphone, &input) ==
                          ESP_CODEC_DEV_OK,
                      ESP_FAIL, TAG, "microphone open");
  esp_codec_dev_sample_info_t output = {
      .sample_rate = SAMPLE_RATE,
      .channel = 2,
      .bits_per_sample = 16,
  };
  ESP_RETURN_ON_FALSE(esp_codec_dev_open(speaker, &output) == ESP_CODEC_DEV_OK,
                      ESP_FAIL, TAG, "speaker open");
  return m5_tab5_audio_set_volume(80);
}

esp_err_t m5_tab5_audio_write(const int16_t *samples, size_t bytes) {
  ESP_RETURN_ON_FALSE(speaker && samples, ESP_ERR_INVALID_STATE, TAG,
                      "speaker");
  return esp_codec_dev_write(speaker, (void *)samples, (int)bytes) ==
                 ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}

esp_err_t m5_tab5_audio_read(int16_t *frames, size_t bytes) {
  ESP_RETURN_ON_FALSE(microphone && frames, ESP_ERR_INVALID_STATE, TAG,
                      "microphone");
  return esp_codec_dev_read(microphone, frames, (int)bytes) == ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}

esp_err_t m5_tab5_audio_set_mute(bool mute) {
  ESP_RETURN_ON_FALSE(speaker, ESP_ERR_INVALID_STATE, TAG, "speaker");
  return esp_codec_dev_set_out_mute(speaker, mute) == ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}

/* bsp_codec_set_volume: 0 mutes, anything else unmutes and sets the level. */
esp_err_t m5_tab5_audio_set_volume(int percent) {
  ESP_RETURN_ON_FALSE(speaker, ESP_ERR_INVALID_STATE, TAG, "speaker");
  if (percent <= 0)
    return m5_tab5_audio_set_mute(true);
  if (percent > 100)
    percent = 100;
  ESP_RETURN_ON_ERROR(m5_tab5_audio_set_mute(false), TAG, "unmute");
  return esp_codec_dev_set_out_vol(speaker, percent) == ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}

esp_err_t m5_tab5_audio_set_in_gain(float db) {
  ESP_RETURN_ON_FALSE(microphone, ESP_ERR_INVALID_STATE, TAG, "microphone");
  return esp_codec_dev_set_in_gain(microphone, db) == ESP_CODEC_DEV_OK
             ? ESP_OK
             : ESP_FAIL;
}
