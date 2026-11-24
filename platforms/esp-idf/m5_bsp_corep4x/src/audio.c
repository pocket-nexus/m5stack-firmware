/*
 * SPDX-FileCopyrightText: 2025 exia
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* ES8311 + ES7210 on I2S0 after audio_tools::audio_system_init,
 * i2s_channel_init, i2s_tx_init, i2s_rx_init, audio_system_deinit,
 * es8311_init, es7210_init, es7210_set_mic_channel_gain and
 * es7210_set_mic_channel_mute of M5CoreP4X-UserDemo (components/
 * otool_esp_component/otool_audio/audio_tools.cpp and audio_chip_device/),
 * in the order main/main.cpp calls them. */
#include "m5_corep4x/audio.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "m5_corep4x/bsp.h"

static const char *TAG = "m5p4x_audio";

#define AUDIO_I2S_PORT I2S_NUM_0
/* audio_tools.cpp: six descriptors of 256 frames per direction. */
#define AUDIO_DMA_DESC_NUM 6
#define AUDIO_DMA_FRAME_NUM 256
#define AUDIO_RECORD_GAIN_DB 30.0f
/* ES7210 registers (esp_codec_dev es7210_reg.h): MIC1..MIC4 gain, and the
 * ADC mute bits (bit 0 / bit 1) of ADC1/ADC2 and ADC3/ADC4. */
#define ES7210_REG_MIC1_GAIN 0x43
#define ES7210_REG_ADC12_MUTE 0x15
#define ES7210_REG_ADC34_MUTE 0x14

typedef struct {
  i2s_chan_handle_t tx;
  i2s_chan_handle_t rx;
  const audio_codec_data_if_t *data_if;
  const audio_codec_ctrl_if_t *dac_ctrl;
  const audio_codec_if_t *dac_if;
  esp_codec_dev_handle_t speaker;
  const audio_codec_ctrl_if_t *adc_ctrl;
  const audio_codec_if_t *adc_if;
  esp_codec_dev_handle_t microphone;
  /* A channel this file enabled, and whether a codec device was opened on
   * it (esp_codec_dev then disables it when the device closes). */
  bool tx_enabled;
  bool rx_enabled;
  bool speaker_opened;
  bool microphone_opened;
} audio_state_t;

static audio_state_t s_audio;
static volatile bool s_ready;

/* s_lock: init, deinit and the codec registers. s_rx_lock / s_tx_lock: one
 * transfer per direction; deinit takes both before it frees the devices. */
static SemaphoreHandle_t s_lock, s_rx_lock, s_tx_lock;
static StaticSemaphore_t s_lock_buffer, s_rx_lock_buffer, s_tx_lock_buffer;
static portMUX_TYPE s_lock_init = portMUX_INITIALIZER_UNLOCKED;

static const gpio_num_t AUDIO_PINS[] = {
    M5_COREP4X_I2S_BCLK, M5_COREP4X_I2S_MCLK, M5_COREP4X_I2S_DOUT,
    M5_COREP4X_I2S_DIN,  M5_COREP4X_I2S_WS,
};

static void create_locks(void) {
  if (s_lock) {
    return;
  }
  taskENTER_CRITICAL(&s_lock_init);
  if (!s_lock) {
    s_rx_lock = xSemaphoreCreateMutexStatic(&s_rx_lock_buffer);
    s_tx_lock = xSemaphoreCreateMutexStatic(&s_tx_lock_buffer);
    s_lock = xSemaphoreCreateMutexStatic(&s_lock_buffer);
  }
  taskEXIT_CRITICAL(&s_lock_init);
}

static esp_err_t codec_err(int ret) {
  return ret == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

/* _gpio_set_high_z: reset, input, no pulls. */
static void release_pins(void) {
  for (size_t i = 0; i < sizeof(AUDIO_PINS) / sizeof(AUDIO_PINS[0]); i++) {
    gpio_reset_pin(AUDIO_PINS[i]);
    gpio_set_direction(AUDIO_PINS[i], GPIO_MODE_INPUT);
    gpio_pullup_dis(AUDIO_PINS[i]);
    gpio_pulldown_dis(AUDIO_PINS[i]);
  }
}

/* i2s_channel_init: one TX + RX pair on I2S0, master. */
static esp_err_t init_i2s(void) {
  i2s_chan_config_t channels =
      I2S_CHANNEL_DEFAULT_CONFIG(AUDIO_I2S_PORT, I2S_ROLE_MASTER);
  channels.dma_desc_num = AUDIO_DMA_DESC_NUM;
  channels.dma_frame_num = AUDIO_DMA_FRAME_NUM;
  /* An underrun sends zeros instead of repeating the last DMA buffer. */
  channels.auto_clear = true;
  ESP_RETURN_ON_ERROR(i2s_new_channel(&channels, &s_audio.tx, &s_audio.rx), TAG,
                      "I2S channels");
  audio_codec_i2s_cfg_t data = {
      .port = AUDIO_I2S_PORT,
      .rx_handle = s_audio.rx,
      .tx_handle = s_audio.tx,
  };
  s_audio.data_if = audio_codec_new_i2s_data(&data);
  ESP_RETURN_ON_FALSE(s_audio.data_if, ESP_FAIL, TAG, "I2S data interface");
  return ESP_OK;
}

/* i2s_tx_init / i2s_rx_init: standard Philips mode in both directions
 * (RX too, although the ES7210 sends TDM), 32-bit stereo slots, the data
 * pins crossed as the factory's i2s_cross_data_pins. */
static esp_err_t start_channel(i2s_chan_handle_t channel, bool *enabled) {
  const i2s_std_config_t std = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(M5_COREP4X_AUDIO_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                      I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = M5_COREP4X_I2S_MCLK,
              .bclk = M5_COREP4X_I2S_BCLK,
              .ws = M5_COREP4X_I2S_WS,
              .dout = M5_COREP4X_I2S_DOUT,
              .din = M5_COREP4X_I2S_DIN,
          },
  };
  ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(channel, &std), TAG,
                      "I2S std mode");
  ESP_RETURN_ON_ERROR(i2s_channel_enable(channel), TAG, "I2S enable");
  *enabled = true;
  return ESP_OK;
}

/* es8311_init(STEREO, DAC): slave, MCLK from the pin, no PA pin. */
static esp_err_t init_dac(int volume) {
  ESP_RETURN_ON_ERROR(start_channel(s_audio.tx, &s_audio.tx_enabled), TAG,
                      "TX");
  audio_codec_i2c_cfg_t i2c = {
      .port = M5_COREP4X_I2C_PORT,
      .addr = ES8311_CODEC_DEFAULT_ADDR,
      .bus_handle = m5_corep4x_i2c(),
  };
  s_audio.dac_ctrl = audio_codec_new_i2c_ctrl(&i2c);
  ESP_RETURN_ON_FALSE(s_audio.dac_ctrl, ESP_FAIL, TAG, "ES8311 control");
  es8311_codec_cfg_t es8311 = {
      .ctrl_if = s_audio.dac_ctrl,
      .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
      .pa_pin = -1,
      .use_mclk = true,
  };
  s_audio.dac_if = es8311_codec_new(&es8311);
  ESP_RETURN_ON_FALSE(s_audio.dac_if, ESP_ERR_NOT_FOUND, TAG, "ES8311");
  esp_codec_dev_cfg_t device = {
      .dev_type = ESP_CODEC_DEV_TYPE_OUT,
      .codec_if = s_audio.dac_if,
      .data_if = s_audio.data_if,
  };
  s_audio.speaker = esp_codec_dev_new(&device);
  ESP_RETURN_ON_FALSE(s_audio.speaker, ESP_ERR_NO_MEM, TAG, "speaker");
  ESP_RETURN_ON_ERROR(
      codec_err(esp_codec_dev_set_out_vol(s_audio.speaker, volume)), TAG,
      "volume");
  esp_codec_dev_sample_info_t fs = {
      .sample_rate = M5_COREP4X_AUDIO_SAMPLE_RATE,
      .channel = 2,
      .bits_per_sample = 32,
  };
  s_audio.speaker_opened = true;
  return codec_err(esp_codec_dev_open(s_audio.speaker, &fs));
}

/* es7210_init(STEREO, MIC_ALL, TDM_ENABLED): slave, MCLK from the pad,
 * MCLK/LRCK 256, all four microphones (esp_codec_dev switches the ES7210 to
 * TDM with 16 bits per microphone when three or more are selected). */
static esp_err_t init_adc(void) {
  ESP_RETURN_ON_ERROR(start_channel(s_audio.rx, &s_audio.rx_enabled), TAG,
                      "RX");
  audio_codec_i2c_cfg_t i2c = {
      .port = M5_COREP4X_I2C_PORT,
      .addr = ES7210_CODEC_DEFAULT_ADDR,
      .bus_handle = m5_corep4x_i2c(),
  };
  s_audio.adc_ctrl = audio_codec_new_i2c_ctrl(&i2c);
  ESP_RETURN_ON_FALSE(s_audio.adc_ctrl, ESP_FAIL, TAG, "ES7210 control");
  es7210_codec_cfg_t es7210 = {
      .ctrl_if = s_audio.adc_ctrl,
      .mic_selected =
          ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4,
      .mclk_src = ES7210_MCLK_FROM_PAD,
      .mclk_div = 256,
  };
  s_audio.adc_if = es7210_codec_new(&es7210);
  ESP_RETURN_ON_FALSE(s_audio.adc_if, ESP_ERR_NOT_FOUND, TAG, "ES7210");
  esp_codec_dev_cfg_t device = {
      .dev_type = ESP_CODEC_DEV_TYPE_IN,
      .codec_if = s_audio.adc_if,
      .data_if = s_audio.data_if,
  };
  s_audio.microphone = esp_codec_dev_new(&device);
  ESP_RETURN_ON_FALSE(s_audio.microphone, ESP_ERR_NO_MEM, TAG, "microphone");
  esp_codec_dev_sample_info_t fs = {
      .sample_rate = M5_COREP4X_AUDIO_SAMPLE_RATE,
      .channel = 2,
      .bits_per_sample = 32,
  };
  s_audio.microphone_opened = true;
  ESP_RETURN_ON_ERROR(codec_err(esp_codec_dev_open(s_audio.microphone, &fs)),
                      TAG, "microphone open");
  /* set_record_gain(30): every microphone. */
  return codec_err(
      esp_codec_dev_set_in_gain(s_audio.microphone, AUDIO_RECORD_GAIN_DB));
}

/* es7210_set_mic_channel_gain: gain bits 3:0 of 0x43 + mic, the other bits
 * kept; es7210_set_mic_channel_mute(false): clear the ADC mute bit. */
static esp_err_t set_mic_gain_locked(unsigned mic, unsigned gain_code) {
  esp_codec_dev_handle_t dev = s_audio.microphone;
  int value = 0;
  ESP_RETURN_ON_ERROR(codec_err(esp_codec_dev_read_reg(
                          dev, ES7210_REG_MIC1_GAIN + (int)mic, &value)),
                      TAG, "gain read");
  ESP_RETURN_ON_ERROR(
      codec_err(esp_codec_dev_write_reg(dev, ES7210_REG_MIC1_GAIN + (int)mic,
                                        (value & 0xF0) | (int)gain_code)),
      TAG, "gain write");
  const int mute_reg = mic < 2 ? ES7210_REG_ADC12_MUTE : ES7210_REG_ADC34_MUTE;
  const int mute_bit = (mic % 2) == 0 ? 0x01 : 0x02;
  ESP_RETURN_ON_ERROR(codec_err(esp_codec_dev_read_reg(dev, mute_reg, &value)),
                      TAG, "mute read");
  if (value & mute_bit) {
    ESP_RETURN_ON_ERROR(
        codec_err(esp_codec_dev_write_reg(dev, mute_reg, value & ~mute_bit)),
        TAG, "mute write");
  }
  return ESP_OK;
}

/* Releases what init created, newest first. Called with s_lock, s_rx_lock
 * and s_tx_lock held. */
static void release_locked(bool for_sleep) {
  s_ready = false;
  if (s_audio.speaker) {
    esp_codec_dev_delete(s_audio.speaker);
  }
  if (s_audio.dac_if) {
    audio_codec_delete_codec_if(s_audio.dac_if);
  }
  if (s_audio.dac_ctrl) {
    audio_codec_delete_ctrl_if(s_audio.dac_ctrl);
  }
  if (s_audio.microphone) {
    esp_codec_dev_delete(s_audio.microphone);
  }
  if (s_audio.adc_if) {
    audio_codec_delete_codec_if(s_audio.adc_if);
  }
  if (s_audio.adc_ctrl) {
    audio_codec_delete_ctrl_if(s_audio.adc_ctrl);
  }
  if (s_audio.data_if) {
    audio_codec_delete_data_if(s_audio.data_if);
  }
  /* esp_codec_dev disabled a channel whose device it opened (the TX
   * disable waits for the RX one). The deep sleep path keeps the channels:
   * the HP domain powers off with their DMA descriptors
   * (audio_system_deinit). */
  i2s_chan_handle_t tx = s_audio.tx;
  i2s_chan_handle_t rx = s_audio.rx;
  if (s_audio.tx_enabled && !s_audio.speaker_opened) {
    (void)i2s_channel_disable(tx);
  }
  if (s_audio.rx_enabled && !s_audio.microphone_opened) {
    (void)i2s_channel_disable(rx);
  }
  s_audio = (audio_state_t){0};
  if (for_sleep) {
    s_audio.tx = tx;
    s_audio.rx = rx;
  } else {
    if (tx && i2s_del_channel(tx) != ESP_OK) {
      ESP_LOGW(TAG, "I2S TX delete failed");
    }
    if (rx && i2s_del_channel(rx) != ESP_OK) {
      ESP_LOGW(TAG, "I2S RX delete failed");
    }
  }
  release_pins();
}

static void lock_all(void) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  xSemaphoreTake(s_rx_lock, portMAX_DELAY);
  xSemaphoreTake(s_tx_lock, portMAX_DELAY);
}

static void unlock_all(void) {
  xSemaphoreGive(s_tx_lock);
  xSemaphoreGive(s_rx_lock);
  xSemaphoreGive(s_lock);
}

esp_err_t m5_corep4x_audio_init(int volume) {
  ESP_RETURN_ON_FALSE(volume >= 0 && volume <= 100, ESP_ERR_INVALID_ARG, TAG,
                      "volume");
  ESP_RETURN_ON_FALSE(m5_corep4x_i2c(), ESP_ERR_INVALID_STATE, TAG, "board");
  create_locks();
  lock_all();
  if (s_ready) {
    unlock_all();
    return ESP_OK;
  }
  /* A deep sleep deinit kept the channels; start from scratch. */
  if (s_audio.tx || s_audio.rx) {
    release_locked(false);
  }
  esp_err_t err = init_i2s();
  if (err == ESP_OK) {
    err = init_dac(volume);
  }
  if (err == ESP_OK) {
    err = init_adc();
  }
  /* main.cpp: MIC1 27 dB, MIC2 27 dB, MIC3 33 dB, MIC4 27 dB, unmuted. */
  static const uint8_t gains[4] = {
      M5_COREP4X_MIC_GAIN_27DB,
      M5_COREP4X_MIC_GAIN_27DB,
      M5_COREP4X_MIC_GAIN_33DB,
      M5_COREP4X_MIC_GAIN_27DB,
  };
  for (unsigned mic = 0; err == ESP_OK && mic < 4; mic++) {
    err = set_mic_gain_locked(mic, gains[mic]);
  }
  if (err == ESP_OK) {
    s_ready = true;
    ESP_LOGI(TAG, "ES8311 + ES7210 on I2S0: %u Hz, 2 x 32-bit, volume %d",
             (unsigned)M5_COREP4X_AUDIO_SAMPLE_RATE, volume);
  } else {
    ESP_LOGE(TAG, "audio init: %s", esp_err_to_name(err));
    release_locked(false);
  }
  unlock_all();
  return err;
}

esp_err_t m5_corep4x_audio_deinit(bool for_sleep) {
  create_locks();
  lock_all();
  release_locked(for_sleep);
  unlock_all();
  return ESP_OK;
}

bool m5_corep4x_audio_ready(void) { return s_ready; }

esp_err_t m5_corep4x_audio_write(const void *frames, size_t bytes) {
  ESP_RETURN_ON_FALSE(frames && bytes % M5_COREP4X_AUDIO_FRAME_BYTES == 0,
                      ESP_ERR_INVALID_ARG, TAG, "frames");
  if (!s_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_tx_lock, portMAX_DELAY);
  esp_err_t err = ESP_ERR_INVALID_STATE;
  if (s_ready && s_audio.speaker) {
    err = codec_err(
        esp_codec_dev_write(s_audio.speaker, (void *)frames, (int)bytes));
  }
  xSemaphoreGive(s_tx_lock);
  return err;
}

esp_err_t m5_corep4x_audio_read(void *frames, size_t bytes) {
  ESP_RETURN_ON_FALSE(frames && bytes % M5_COREP4X_AUDIO_FRAME_BYTES == 0,
                      ESP_ERR_INVALID_ARG, TAG, "frames");
  if (!s_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_rx_lock, portMAX_DELAY);
  esp_err_t err = ESP_ERR_INVALID_STATE;
  if (s_ready && s_audio.microphone) {
    err = codec_err(esp_codec_dev_read(s_audio.microphone, frames, (int)bytes));
  }
  xSemaphoreGive(s_rx_lock);
  return err;
}

esp_err_t m5_corep4x_audio_set_volume(int volume) {
  ESP_RETURN_ON_FALSE(volume >= 0 && volume <= 100, ESP_ERR_INVALID_ARG, TAG,
                      "volume");
  if (!s_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = ESP_ERR_INVALID_STATE;
  if (s_ready) {
    err = codec_err(esp_codec_dev_set_out_vol(s_audio.speaker, volume));
  }
  xSemaphoreGive(s_lock);
  return err;
}

esp_err_t m5_corep4x_audio_set_mic_gain(unsigned mic, unsigned gain_code) {
  ESP_RETURN_ON_FALSE(mic < 4 && gain_code <= 14, ESP_ERR_INVALID_ARG, TAG,
                      "gain");
  if (!s_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  esp_err_t err = ESP_ERR_INVALID_STATE;
  if (s_ready) {
    err = set_mic_gain_locked(mic, gain_code);
  }
  xSemaphoreGive(s_lock);
  return err;
}
