/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Board bring-up and the small public functions of the BSP. The order and the
 * settings follow the factory HAL (M5StopWatch-UserDemo main/hal): Hal::init,
 * i2c_init (hal.cpp), pmic_init, setPowerLed, getBatteryLevel and
 * isBatteryCharging (hal_pmic.cpp), ioe_init and ioe_tp_reset (hal_ioe.cpp),
 * display_init, setBackLightBrightness and touchpad_init (hal_display.cpp),
 * imu_init and updateImuData (hal_imu.cpp), button_init (hal_button.cpp).
 */
#include "board.h"

#include "bmi270.h"
#include "co5300.h"
#include "cst820.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_io_spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ioe1.h"
#include "pm1.h"
#include "rx8130.h"

static const char *TAG = "m5_stopwatch";

#define LCD_SPI_HOST SPI2_HOST
#define LCD_PCLK_HZ (80 * 1000 * 1000)
#define LCD_DEFAULT_MAX_TRANSFER                                               \
  (M5_STOPWATCH_LCD_H_RES * 32 * sizeof(uint16_t)) /* one 32-row strip */
#define LCD_CLEAR_ROWS 16
#define L3B_EN_MAX_RETRIES 25 /* 80 ms each; the factory loop is unbounded */

#define BATTERY_MV_EMPTY 3300
#define BATTERY_MV_FULL 4200
#define BATTERY_FILTER_OLD 7
#define BATTERY_FILTER_NEW 1
#define VIN_PRESENT_MV 4000

static struct {
  bool started;
  i2c_master_bus_handle_t i2c;
  esp_lcd_panel_io_handle_t lcd_io;
  esp_lcd_panel_handle_t lcd;
  esp_lcd_touch_handle_t touch;
  SemaphoreHandle_t lock; /* lazy RTC start */
  bool rtc_started;
  uint16_t battery_filtered_mv;
} s_board;

static portMUX_TYPE s_battery_mux = portMUX_INITIALIZER_UNLOCKED;

#define WARN_ON_ERROR(expr, what)                                              \
  do {                                                                         \
    esp_err_t err_ = (expr);                                                   \
    if (err_ != ESP_OK) {                                                      \
      ESP_LOGW(TAG, "%s: %s", what, esp_err_to_name(err_));                    \
    }                                                                          \
  } while (0)

i2c_master_bus_handle_t m5_stopwatch_board_i2c(void) { return s_board.i2c; }

/* ------------------------------- Battery --------------------------------- */

static uint8_t battery_mv_to_percent(uint16_t mv) {
  if (mv <= BATTERY_MV_EMPTY) {
    return 0;
  }
  if (mv >= BATTERY_MV_FULL) {
    return 100;
  }
  return (uint8_t)((uint32_t)(mv - BATTERY_MV_EMPTY) * 100U /
                   (BATTERY_MV_FULL - BATTERY_MV_EMPTY));
}

/* update_bat_level_from_mv: 7:1 low-pass filter with rounding. */
static uint16_t battery_filter(uint16_t mv) {
  portENTER_CRITICAL(&s_battery_mux);
  if (s_board.battery_filtered_mv == 0) {
    s_board.battery_filtered_mv = mv;
  } else {
    const uint32_t sum = BATTERY_FILTER_OLD + BATTERY_FILTER_NEW;
    const uint32_t f =
        (uint32_t)s_board.battery_filtered_mv * BATTERY_FILTER_OLD +
        (uint32_t)mv * BATTERY_FILTER_NEW;
    s_board.battery_filtered_mv = (uint16_t)((f + sum / 2) / sum);
  }
  const uint16_t out = s_board.battery_filtered_mv;
  portEXIT_CRITICAL(&s_battery_mux);
  return out;
}

esp_err_t m5_stopwatch_battery_read(m5_stopwatch_battery_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(m5_stopwatch_pm1_ready(), ESP_ERR_INVALID_STATE, TAG,
                      "PM1 not available");
  uint16_t vbat = 0, vin = 0;
  uint8_t chg_stat = 1;
  ESP_RETURN_ON_ERROR(m5_stopwatch_pm1_read_vbat(&vbat), TAG, "VBAT");
  ESP_RETURN_ON_ERROR(m5_stopwatch_pm1_read_vin(&vin), TAG, "VIN");
  ESP_RETURN_ON_ERROR(m5_stopwatch_pm1_gpio_get_input(
                          M5_STOPWATCH_PM1_GPIO_CHG_STAT, &chg_stat),
                      TAG, "CHG_STAT");
  out->battery_mv = battery_filter(vbat);
  out->vin_mv = vin;
  out->level = battery_mv_to_percent(out->battery_mv);
  out->external_power = vin > VIN_PRESENT_MV;
  out->charging = out->external_power && chg_stat == 0;
  return ESP_OK;
}

esp_err_t m5_stopwatch_power_led(bool on) {
  return m5_stopwatch_pm1_set_led_en_level(on);
}

esp_err_t m5_stopwatch_power_off(void) { return m5_stopwatch_pm1_shutdown(); }

/* ------------------------------- Display --------------------------------- */

esp_err_t m5_stopwatch_display_set_brightness(uint8_t percent) {
  ESP_RETURN_ON_FALSE(s_board.lcd, ESP_ERR_INVALID_STATE, TAG,
                      "display not started");
  if (percent > 100) {
    percent = 100;
  }
  /* The power LED is lit only while the panel brightness is 0. */
  if (percent > 0 && m5_stopwatch_pm1_ready()) {
    WARN_ON_ERROR(m5_stopwatch_pm1_set_led_en_level(false), "power LED");
  }
  ESP_RETURN_ON_ERROR(
      esp_lcd_panel_set_brightness(s_board.lcd, (int)percent * 255 / 100), TAG,
      "brightness");
  if (percent == 0 && m5_stopwatch_pm1_ready()) {
    WARN_ON_ERROR(m5_stopwatch_pm1_set_led_en_level(true), "power LED");
  }
  return ESP_OK;
}

/* --------------------------------- RTC ----------------------------------- */

static esp_err_t rtc_start(void) {
  ESP_RETURN_ON_FALSE(s_board.started, ESP_ERR_INVALID_STATE, TAG,
                      "board not started");
  xSemaphoreTake(s_board.lock, portMAX_DELAY);
  esp_err_t err = ESP_OK;
  if (!s_board.rtc_started) {
    err = m5_stopwatch_rx8130_begin(s_board.i2c);
    s_board.rtc_started = err == ESP_OK;
  }
  xSemaphoreGive(s_board.lock);
  return err;
}

esp_err_t m5_stopwatch_rtc_get(struct tm *out) {
  ESP_RETURN_ON_ERROR(rtc_start(), TAG, "RTC");
  return m5_stopwatch_rx8130_get(out);
}

esp_err_t m5_stopwatch_rtc_set(const struct tm *time) {
  ESP_RETURN_ON_ERROR(rtc_start(), TAG, "RTC");
  return m5_stopwatch_rx8130_set(time);
}

/* --------------------------------- IMU ----------------------------------- */

esp_err_t m5_stopwatch_imu_init(void) {
  ESP_RETURN_ON_FALSE(s_board.started, ESP_ERR_INVALID_STATE, TAG,
                      "board not started");
  return m5_stopwatch_bmi270_begin(s_board.i2c);
}

esp_err_t m5_stopwatch_imu_read(m5_stopwatch_imu_sample_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  m5_stopwatch_bmi270_raw_t raw;
  ESP_RETURN_ON_ERROR(m5_stopwatch_bmi270_read(&raw), TAG, "IMU");
  /* ±4 g: 8192 LSB/g; ±2000 dps: 16.384 LSB/dps. Hal::updateImuData passes
   * (y, x, z) to the sensor reads, so X and Y are swapped. */
  out->accel_x = raw.acc[1] / 8192.0f;
  out->accel_y = raw.acc[0] / 8192.0f;
  out->accel_z = raw.acc[2] / 8192.0f;
  out->gyro_x = raw.gyr[1] / 16.384f;
  out->gyro_y = raw.gyr[0] / 16.384f;
  out->gyro_z = raw.gyr[2] / 16.384f;
  return ESP_OK;
}

/* ------------------------------- Bring-up -------------------------------- */

static esp_err_t i2c_init(void) {
  const i2c_master_bus_config_t cfg = {
      .i2c_port = M5_STOPWATCH_I2C_PORT,
      .sda_io_num = M5_STOPWATCH_PIN_I2C_SDA,
      .scl_io_num = M5_STOPWATCH_PIN_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  return i2c_new_master_bus(&cfg, &s_board.i2c);
}

/* Hal::pmic_init. A missing PM1 is logged and bring-up continues. */
static void pmic_init(void) {
  esp_err_t err = m5_stopwatch_pm1_begin(s_board.i2c);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "PM1 init failed: %s", esp_err_to_name(err));
    return;
  }
  WARN_ON_ERROR(m5_stopwatch_pm1_set_i2c_sleep_time(0), "PM1 I2C sleep");
  WARN_ON_ERROR(m5_stopwatch_pm1_btn_click_delay_1s(), "PM1 button");
  WARN_ON_ERROR(m5_stopwatch_pm1_wdt_set(0), "PM1 watchdog");
  /* Keeps the LDO (RTC supply) on after power-off. */
  WARN_ON_ERROR(m5_stopwatch_pm1_ldo_set_power_hold(true), "PM1 power hold");
  WARN_ON_ERROR(m5_stopwatch_pm1_set_charge_enable(true), "PM1 charge");
  /* CHG_PROG low forces active charge programming. */
  WARN_ON_ERROR(
      m5_stopwatch_pm1_gpio_output(M5_STOPWATCH_PM1_GPIO_CHG_PROG, false),
      "PM1 CHG_PROG");
  WARN_ON_ERROR(m5_stopwatch_pm1_gpio_input(M5_STOPWATCH_PM1_GPIO_CHG_STAT),
                "PM1 CHG_STAT");
  WARN_ON_ERROR(m5_stopwatch_pm1_set_single_reset_disable(true),
                "PM1 single reset");
  uint16_t vbat = 0;
  if (m5_stopwatch_pm1_read_vbat(&vbat) == ESP_OK) {
    (void)battery_filter(vbat);
  }
}

/* Hal::ioe_init. */
static esp_err_t ioe_init(void) {
  esp_err_t err =
      m5_stopwatch_ioe1_begin(s_board.i2c, M5_STOPWATCH_IOE1_ADDR_PRIMARY);
  if (err != ESP_OK) {
    err =
        m5_stopwatch_ioe1_begin(s_board.i2c, M5_STOPWATCH_IOE1_ADDR_SECONDARY);
  }
  ESP_RETURN_ON_ERROR(err, TAG, "IOE1 not found at 0x%02X or 0x%02X",
                      M5_STOPWATCH_IOE1_ADDR_PRIMARY,
                      M5_STOPWATCH_IOE1_ADDR_SECONDARY);

  static const uint8_t outputs[] = {
      M5_STOPWATCH_IOE1_MOTOR_EN, M5_STOPWATCH_IOE1_L3B_EN,
      M5_STOPWATCH_IOE1_SPK_PA,   M5_STOPWATCH_IOE1_TP_RST,
      M5_STOPWATCH_IOE1_OLED_RST, M5_STOPWATCH_IOE1_MUX_CTR,
      M5_STOPWATCH_IOE1_AU_EN,
  };
  for (size_t i = 0; i < sizeof(outputs); i++) {
    ESP_RETURN_ON_ERROR(m5_stopwatch_ioe1_pin_mode_output(outputs[i]), TAG,
                        "IOE1 pin %u mode", outputs[i]);
  }
  static const struct {
    uint8_t pin;
    bool level;
  } levels[] = {
      {M5_STOPWATCH_IOE1_L3B_EN, true},   {M5_STOPWATCH_IOE1_TP_RST, true},
      {M5_STOPWATCH_IOE1_SPK_PA, false},  {M5_STOPWATCH_IOE1_OLED_RST, true},
      {M5_STOPWATCH_IOE1_MUX_CTR, false}, {M5_STOPWATCH_IOE1_AU_EN, true},
  };
  for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
    ESP_RETURN_ON_ERROR(
        m5_stopwatch_ioe1_digital_write(levels[i].pin, levels[i].level), TAG,
        "IOE1 pin %u write", levels[i].pin);
  }
  WARN_ON_ERROR(m5_stopwatch_ioe1_set_pwm_frequency(5000), "IOE1 PWM freq");

  const gpio_config_t pa = {
      .pin_bit_mask = 1ULL << M5_STOPWATCH_PIN_SPEAKER_PA,
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&pa), TAG, "GPIO14");
  ESP_RETURN_ON_ERROR(gpio_set_level(M5_STOPWATCH_PIN_SPEAKER_PA, 0), TAG,
                      "GPIO14");

  WARN_ON_ERROR(m5_stopwatch_vibrate_init(), "vibrator");

  /* Make sure L3B_EN (display power) reads back high. */
  for (int retry = 0;; retry++) {
    vTaskDelay(pdMS_TO_TICKS(80));
    bool level = false;
    if (m5_stopwatch_ioe1_digital_read(M5_STOPWATCH_IOE1_L3B_EN, &level) ==
            ESP_OK &&
        level) {
      break;
    }
    ESP_RETURN_ON_FALSE(retry < L3B_EN_MAX_RETRIES, ESP_ERR_TIMEOUT, TAG,
                        "L3B_EN stays low");
    ESP_LOGW(TAG, "set L3B_EN high, retry %d", retry + 1);
    (void)m5_stopwatch_ioe1_digital_write(M5_STOPWATCH_IOE1_L3B_EN, true);
  }
  return ESP_OK;
}

/* Hal::display_init with the M5StopWatch bus and panel configuration. */
static esp_err_t display_init(const m5_stopwatch_config_t *cfg) {
  const gpio_config_t te = {
      .pin_bit_mask = 1ULL << M5_STOPWATCH_PIN_LCD_TE,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  ESP_RETURN_ON_ERROR(gpio_config(&te), TAG, "TE pin");
  WARN_ON_ERROR(m5_stopwatch_refresh_init(), "TE timing");

  /* The AMOLED is the power indication while it is on. */
  if (m5_stopwatch_pm1_ready()) {
    WARN_ON_ERROR(m5_stopwatch_pm1_set_led_en_level(false), "power LED");
  }

  const size_t max_transfer = cfg->lcd_max_transfer_bytes
                                  ? cfg->lcd_max_transfer_bytes
                                  : LCD_DEFAULT_MAX_TRANSFER;
  const spi_bus_config_t bus = {
      .data0_io_num = M5_STOPWATCH_PIN_LCD_D0,
      .data1_io_num = M5_STOPWATCH_PIN_LCD_D1,
      .sclk_io_num = M5_STOPWATCH_PIN_LCD_SCLK,
      .data2_io_num = M5_STOPWATCH_PIN_LCD_D2,
      .data3_io_num = M5_STOPWATCH_PIN_LCD_D3,
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,
      .max_transfer_sz = (int)max_transfer,
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD,
  };
  ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO),
                      TAG, "SPI bus");

  /* on_color_trans_done stays unregistered; the display driver sets it. */
  const esp_lcd_panel_io_spi_config_t io = {
      .cs_gpio_num = M5_STOPWATCH_PIN_LCD_CS,
      .dc_gpio_num = GPIO_NUM_NC,
      .spi_mode = 0,
      .pclk_hz = LCD_PCLK_HZ,
      .trans_queue_depth = 10,
      .lcd_cmd_bits = 32,
      .lcd_param_bits = 8,
      .flags.quad_mode = true,
  };
  ESP_RETURN_ON_ERROR(
      esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io,
                               &s_board.lcd_io),
      TAG, "panel IO");
  ESP_RETURN_ON_ERROR(m5_stopwatch_co5300_new(s_board.lcd_io, &s_board.lcd),
                      TAG, "panel");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_board.lcd), TAG, "panel init");
  /* Columns outside the 466-wide frame are never written afterwards. */
  ESP_RETURN_ON_ERROR(
      m5_stopwatch_co5300_clear_memory(s_board.lcd, LCD_CLEAR_ROWS), TAG,
      "panel clear");
  return m5_stopwatch_display_set_brightness(cfg->brightness);
}

/* Hal::ioe_tp_reset and Hal::touchpad_init. A missing controller is logged
 * and leaves the touch handle NULL. */
static void touch_init(void) {
  if (m5_stopwatch_ioe1_digital_write(M5_STOPWATCH_IOE1_TP_RST, false) ==
      ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(10));
    (void)m5_stopwatch_ioe1_digital_write(M5_STOPWATCH_IOE1_TP_RST, true);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  const esp_lcd_touch_config_t cfg = {
      .x_max = M5_STOPWATCH_LCD_H_RES,
      .y_max = M5_STOPWATCH_LCD_V_RES,
      .rst_gpio_num = GPIO_NUM_NC,
      .int_gpio_num = M5_STOPWATCH_PIN_TOUCH_INT,
  };
  esp_err_t err = m5_stopwatch_cst820_new(s_board.i2c, &cfg, &s_board.touch);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(err));
    s_board.touch = NULL;
  }
}

/* Hal::button_init. */
static esp_err_t button_init(void) {
  const gpio_config_t cfg = {
      .pin_bit_mask = (1ULL << M5_STOPWATCH_PIN_BUTTON_A) |
                      (1ULL << M5_STOPWATCH_PIN_BUTTON_B),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  return gpio_config(&cfg);
}

esp_err_t m5_stopwatch_init(const m5_stopwatch_config_t *config,
                            m5_stopwatch_handles_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "arg");
  ESP_RETURN_ON_FALSE(!s_board.started && !s_board.i2c, ESP_ERR_INVALID_STATE,
                      TAG, "already initialized");
  m5_stopwatch_config_t cfg = M5_STOPWATCH_CONFIG_DEFAULT();
  if (config) {
    cfg = *config;
  }
  if (!s_board.lock) {
    s_board.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_board.lock, ESP_ERR_NO_MEM, TAG, "no mutex");
  }

  ESP_RETURN_ON_ERROR(i2c_init(), TAG, "I2C bus");
  pmic_init();
  ESP_RETURN_ON_ERROR(ioe_init(), TAG, "IOE1");
  vTaskDelay(pdMS_TO_TICKS(50));
  ESP_RETURN_ON_ERROR(display_init(&cfg), TAG, "display");
  touch_init();
  ESP_RETURN_ON_ERROR(button_init(), TAG, "buttons");

  s_board.started = true;
  out->i2c = s_board.i2c;
  out->lcd_io = s_board.lcd_io;
  out->lcd = s_board.lcd;
  out->touch = s_board.touch;
  return ESP_OK;
}
