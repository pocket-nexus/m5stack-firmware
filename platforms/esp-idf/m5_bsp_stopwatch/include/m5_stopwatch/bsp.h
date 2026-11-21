/*
 * SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Board support for the M5Stack StopWatch (C152). The pin map, power-up
 * sequence and driver settings follow the M5StopWatch-UserDemo factory HAL;
 * see the component README for the source revisions.
 */
#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Visible area of the 1.75" round AMOLED (CO5300). */
#define M5_STOPWATCH_LCD_H_RES 466
#define M5_STOPWATCH_LCD_V_RES 466

/* Internal I2C bus: PM1, IOE1, CST820, BMI270, RX8130CE, ES8311. */
#define M5_STOPWATCH_I2C_PORT I2C_NUM_0
#define M5_STOPWATCH_PIN_I2C_SDA GPIO_NUM_47
#define M5_STOPWATCH_PIN_I2C_SCL GPIO_NUM_48

/* CO5300 on QSPI (SPI2), 80 MHz. */
#define M5_STOPWATCH_PIN_LCD_SCLK GPIO_NUM_40
#define M5_STOPWATCH_PIN_LCD_D0 GPIO_NUM_41
#define M5_STOPWATCH_PIN_LCD_D1 GPIO_NUM_42
#define M5_STOPWATCH_PIN_LCD_D2 GPIO_NUM_46
#define M5_STOPWATCH_PIN_LCD_D3 GPIO_NUM_45
#define M5_STOPWATCH_PIN_LCD_CS GPIO_NUM_39
#define M5_STOPWATCH_PIN_LCD_TE GPIO_NUM_38

#define M5_STOPWATCH_PIN_TOUCH_INT GPIO_NUM_13

/* Front keys, active low with internal pull-ups. */
#define M5_STOPWATCH_PIN_BUTTON_A GPIO_NUM_2 /* KEYA, yellow */
#define M5_STOPWATCH_PIN_BUTTON_B GPIO_NUM_1 /* KEYB, blue */

/* ES8311 I2S. */
#define M5_STOPWATCH_PIN_I2S_MCLK GPIO_NUM_18
#define M5_STOPWATCH_PIN_I2S_BCLK GPIO_NUM_17
#define M5_STOPWATCH_PIN_I2S_LRCK GPIO_NUM_15
#define M5_STOPWATCH_PIN_I2S_DOUT GPIO_NUM_21
#define M5_STOPWATCH_PIN_I2S_DIN GPIO_NUM_16
#define M5_STOPWATCH_PIN_SPEAKER_PA GPIO_NUM_14

#define M5_STOPWATCH_AUDIO_SAMPLE_RATE 44100

/* Handles returned by m5_stopwatch_init. The BSP keeps ownership. */
typedef struct {
  i2c_master_bus_handle_t i2c;
  esp_lcd_panel_io_handle_t lcd_io;
  esp_lcd_panel_handle_t lcd;
  esp_lcd_touch_handle_t touch; /* NULL when the controller did not answer */
} m5_stopwatch_handles_t;

typedef struct {
  /* QSPI pixel transfer size limit in bytes; 0 selects one 32-row strip. */
  size_t lcd_max_transfer_bytes;
  /* Initial panel brightness in percent (0..100). */
  uint8_t brightness;
} m5_stopwatch_config_t;

#define M5_STOPWATCH_CONFIG_DEFAULT()                                          \
  {.lcd_max_transfer_bytes = 0, .brightness = 80}

/*
 * Bring up the board in the factory order: I2C bus, PM1 PMIC (charging,
 * power hold, watchdog off), IOE1 expander (display and touch power and
 * reset, audio enable, speaker amplifier off), CO5300 panel (cleared to
 * black, display on), CST820 touch and the A/B key GPIOs.
 */
esp_err_t m5_stopwatch_init(const m5_stopwatch_config_t *config,
                            m5_stopwatch_handles_t *out);

/* ------------------------------- Display -------------------------------- */

/* Panel brightness through CO5300 command 0x51; 0..100 percent. The PM1
 * power LED is lit only at 0 %, as in the factory HAL. */
esp_err_t m5_stopwatch_display_set_brightness(uint8_t percent);

/* ------------------------------- Power ---------------------------------- */

typedef struct {
  uint16_t battery_mv; /* filtered battery voltage */
  uint16_t vin_mv;     /* USB input voltage */
  uint8_t level;       /* 0..100, linear 3300..4200 mV */
  bool external_power; /* VIN above 4000 mV */
  bool charging;       /* external power and CHG_STAT low */
} m5_stopwatch_battery_t;

/* Reads the PM1 ADC. Applies the factory 7:1 low-pass filter to the battery
 * voltage across calls. */
esp_err_t m5_stopwatch_battery_read(m5_stopwatch_battery_t *out);
esp_err_t m5_stopwatch_power_led(bool on);
/* PM1 power-off; the RTC and the LDO power hold stay on. */
esp_err_t m5_stopwatch_power_off(void);

/* ---------------------------- Vibration motor --------------------------- */

/* IOE1 PWM channel 0 on pin 9. strength 1..100 maps to a 25..100 % duty.
 * The motor stops after duration_ms (esp_timer). */
esp_err_t m5_stopwatch_vibrate(uint16_t duration_ms, uint8_t strength);
esp_err_t m5_stopwatch_vibrate_stop(void);

/* --------------------------------- RTC ---------------------------------- */

/* RX8130CE. Fields follow struct tm (tm_year since 1900, tm_mon 0..11). */
esp_err_t m5_stopwatch_rtc_get(struct tm *out);
esp_err_t m5_stopwatch_rtc_set(const struct tm *time);

/* --------------------------------- IMU ---------------------------------- */

typedef struct {
  float accel_x, accel_y, accel_z; /* g */
  float gyro_x, gyro_y, gyro_z;    /* degrees per second */
} m5_stopwatch_imu_sample_t;

/* BMI270: uploads the Bosch configuration blob, enables the accelerometer
 * (±4 g, 100 Hz) and gyroscope (±2000 dps, 100 Hz), the settings of the
 * factory IMU driver. */
esp_err_t m5_stopwatch_imu_init(void);
/* Axes are mapped to the factory convention (X and Y swapped). */
esp_err_t m5_stopwatch_imu_read(m5_stopwatch_imu_sample_t *out);

/* -------------------------------- Audio --------------------------------- */

/* ES8311 through esp_codec_dev, 44.1 kHz, 16-bit mono, full duplex. */
esp_err_t m5_stopwatch_audio_init(void);
/* 0..100. */
esp_err_t m5_stopwatch_audio_set_volume(uint8_t volume);
/* Microphone gain in dB (the factory records at 30 dB). */
esp_err_t m5_stopwatch_audio_set_mic_gain(float db);
/* Enables the speaker amplifier (IOE1 pin 10 and GPIO14). */
esp_err_t m5_stopwatch_audio_speaker_enable(bool enable);
/* Blocking PCM output; samples are mono s16 at 44.1 kHz. */
esp_err_t m5_stopwatch_audio_write(const int16_t *samples, size_t count);
/* Blocking microphone capture into samples. */
esp_err_t m5_stopwatch_audio_read(int16_t *samples, size_t count);

#ifdef __cplusplus
}
#endif
