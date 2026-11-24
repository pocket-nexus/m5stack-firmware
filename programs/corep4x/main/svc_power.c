/* POWER, SLEEP and OFF windows: rails, backlight, charge current, the VBAT
 * filter, the M5-Bus GPIOs, power-off and restart, the PM1 button and the
 * deep-sleep sequence of the factory sleep_before_work/enter_deep_sleep. */
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_corep4x/bsp.h"
#include "m5_corep4x/imu.h"
#include "m5_corep4x/rtc.h"
#include "services.h"
#include "svc.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "svc_power";

#define TICK_MS 250
#define VBAT_SAMPLES 14
#define GPIO_COUNT 55
#define LCD_OFF_RESTART_US (10 * 1000 * 1000)
#define BACKLIGHT_SETTLE_MS 120
/* m5pm1_err_t values the OFF window prints: M5PM1_ERR_I2C_COMM,
 * M5PM1_ERR_NOT_INIT. */
#define PM1_ERR_I2C_COMM (-7)
#define PM1_ERR_NOT_INIT (-8)
#define PM1_ERR_INTERNAL (-9)

/* device.ts rail order. */
static const struct {
  const char *name;
  const char *value;
  m5_corep4x_switch_t which;
} RAILS[] = {
    {"L3B", "railL3B", M5_COREP4X_SWITCH_L3B},
    {"LCD", "railLCD", M5_COREP4X_SWITCH_LCD},
    {"AudioIC", "railAudioIC", M5_COREP4X_SWITCH_AUDIO},
    {"AudioPA", "railAudioPA", M5_COREP4X_SWITCH_SPEAKER},
    {"USBA", "railUSBA", M5_COREP4X_SWITCH_USB_A},
    {"P4USBC", "railP4USBC", M5_COREP4X_SWITCH_USB_C},
    {"Grove", "railGrove", M5_COREP4X_SWITCH_GROVE},
    {"M5BUS", "railM5BUS", M5_COREP4X_SWITCH_M5BUS_5V},
};
#define RAIL_COUNT (sizeof(RAILS) / sizeof(RAILS[0]))

typedef struct {
  uint32_t rtc_seconds; /* 0: no RTC wake */
  bool touch;
  bool imu;
} wake_options_t;

static TaskHandle_t s_task;
static esp_timer_handle_t s_lcd_restart;
static volatile bool s_page;
static volatile bool s_vbat_reset;
static volatile bool s_button_sleep = true;
static volatile bool s_busy; /* power-off, restart or sleep under way */
/* The sleep sequence owns the I2C bus: the power task stops its reads. */
static volatile bool s_sleeping;
static wake_options_t s_wake;
static uint8_t s_backlight = 100;

/* ---- deep sleep ------------------------------------------------------------
 */

static void sleep_sequence(const wake_options_t *wake) {
  s_sleeping = true;
  ESP_LOGI(TAG, "deep sleep: rtc %" PRIu32 " s, touch %d, imu %d",
           wake->rtc_seconds, wake->touch, wake->imu);
  /* The application stops drawing first (the factory stops its LVGL demo
   * task); its frame hooks no longer run after this. */
  m5_pocket_app *app = svc_app();
  if (app) {
    services_set_app(NULL);
    m5_pocket_app_stop(app);
  }
  rtc_svc_sleep();
  imu_svc_sleep();
  esp_err_t err = m5_corep4x_imu_prepare_sleep(wake->imu);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "IMU sleep preparation: %s", esp_err_to_name(err));
  }
  net_svc_sleep();
  audio_svc_sleep();
  storage_svc_sleep();
  m5_corep4x_backlight_set(0);
  services_display_off(!wake->touch);
  err = m5_corep4x_rtc_wake_timer(wake->rtc_seconds);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "RTC wake timer: %s", esp_err_to_name(err));
  }
  err = m5_corep4x_sleep_rails_off();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "rails off: %s", esp_err_to_name(err));
  }
  const m5_corep4x_wake_t sources = {
      .rtc = wake->rtc_seconds > 0,
      .touch = wake->touch,
      .imu = wake->imu,
  };
  err = m5_corep4x_deep_sleep(&sources);
  /* Only on failure. The display is gone: restart instead of leaving a
   * dark board. */
  ESP_LOGE(TAG, "deep sleep failed: %s; restarting", esp_err_to_name(err));
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_restart();
}

static void sleep_task(void *arg) {
  wake_options_t wake = *(wake_options_t *)arg;
  free(arg);
  sleep_sequence(&wake);
  vTaskDelete(NULL);
}

static void start_sleep(const wake_options_t *wake) {
  wake_options_t *copy = malloc(sizeof(*copy));
  if (copy) {
    *copy = *wake;
  }
  if (!copy || xTaskCreate(sleep_task, "func_sleep", 6144, copy,
                           configMAX_PRIORITIES - 4, NULL) != pdPASS) {
    free(copy);
    s_busy = false;
    m5_pocket_bridge_set_text("sleepError", "cannot create worker task");
  }
}

/* ---- power-off and restart --------------------------------------------------
 */

static void off_task(void *arg) {
  const bool power_off = arg != NULL;
  /* The OFF window: backlight 0 and 120 ms, then the PM1 command or 200 ms
   * and esp_restart. */
  m5_corep4x_backlight_set(0);
  vTaskDelay(pdMS_TO_TICKS(BACKLIGHT_SETTLE_MS));
  if (power_off) {
    int code = 0;
    if (!m5_corep4x_pmic_ready()) {
      code = PM1_ERR_NOT_INIT;
    } else if (m5_corep4x_power_off() != ESP_OK) {
      code = PM1_ERR_I2C_COMM;
    }
    s_busy = false;
    m5_pocket_bridge_post_event("powerOff", code);
  } else {
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
  }
  vTaskDelete(NULL);
}

static void lcd_restart(void *arg) {
  (void)arg;
  esp_restart();
}

/* ---- sampling task --------------------------------------------------------
 */

static int compare_u16(const void *a, const void *b) {
  return (int)*(const uint16_t *)a - (int)*(const uint16_t *)b;
}

/* sample_vbat_once: 14 samples, sorted, the 2 lowest and 2 highest dropped,
 * the other 10 averaged with rounding. */
static void sample_vbat(uint16_t *ring, size_t *count, size_t *next) {
  m5_corep4x_power_t power;
  const uint16_t mv =
      m5_corep4x_power_read(&power) == ESP_OK ? power.battery_mv : 0;
  if (mv == 0) {
    if (*count < VBAT_SAMPLES) {
      m5_pocket_bridge_set_number("vbatFiltered", 0);
    }
    return;
  }
  ring[*next] = mv;
  *next = (*next + 1) % VBAT_SAMPLES;
  if (*count < VBAT_SAMPLES) {
    (*count)++;
  }
  if (*count < VBAT_SAMPLES) {
    m5_pocket_bridge_set_number("vbatFiltered", 0);
    return;
  }
  uint16_t sorted[VBAT_SAMPLES];
  memcpy(sorted, ring, sizeof(sorted));
  qsort(sorted, VBAT_SAMPLES, sizeof(sorted[0]), compare_u16);
  uint32_t sum = 0;
  for (size_t i = 2; i < VBAT_SAMPLES - 2; i++) {
    sum += sorted[i];
  }
  m5_pocket_bridge_set_number("vbatFiltered", (sum + 5) / 10);
}

static void publish_rails(void) {
  for (size_t i = 0; i < RAIL_COUNT; i++) {
    bool on = false;
    if (m5_corep4x_get_switch(RAILS[i].which, &on) == ESP_OK) {
      m5_pocket_bridge_set_bool(RAILS[i].value, on);
    }
  }
}

static void publish_gpio_levels(void) {
  float levels[GPIO_COUNT];
  for (int i = 0; i < GPIO_COUNT; i++) {
    levels[i] = (float)gpio_get_level((gpio_num_t)i);
  }
  m5_pocket_bridge_set_array("gpioLevels", levels, GPIO_COUNT);
}

/* The PM1 button: a single click puts the board to sleep after 1 s when
 * BTN sleep is on (p4_pm1_irq_task). */
static void handle_pmic_irq(void) {
  uint8_t button = 0;
  uint8_t gpio = 0;
  if (m5_corep4x_pmic_irq_take(&button, &gpio) != ESP_OK) {
    return;
  }
  if (button) {
    ESP_LOGI(TAG, "PM1 button IRQ 0x%02X", button);
  }
  if ((button & M5_COREP4X_BUTTON_CLICK) && s_button_sleep && !s_busy) {
    s_busy = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    const wake_options_t wake = s_wake;
    sleep_sequence(&wake);
  }
}

static void IRAM_ATTR on_pmic_irq(void *context) {
  (void)context;
  BaseType_t woken = pdFALSE;
  if (s_task) {
    vTaskNotifyGiveFromISR(s_task, &woken);
  }
  portYIELD_FROM_ISR(woken);
}

static void power_task(void *arg) {
  (void)arg;
  uint16_t ring[VBAT_SAMPLES];
  size_t count = 0;
  size_t next = 0;
  int ticks = 0;
  publish_rails();
  for (;;) {
    /* The factory polls its IRQ flag every 2 s; the notification is
     * handled at once here. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TICK_MS)) > 0 && !s_sleeping) {
      handle_pmic_irq();
      continue;
    }
    if (s_sleeping) {
      continue;
    }
    if (s_vbat_reset) {
      s_vbat_reset = false;
      count = 0;
      next = 0;
      m5_pocket_bridge_set_number("vbatFiltered", 0);
    }
    if (s_page) {
      sample_vbat(ring, &count, &next);
      publish_gpio_levels();
    }
    if (++ticks >= 1000 / TICK_MS) {
      ticks = 0;
      publish_rails();
    }
  }
}

/* ---- requests --------------------------------------------------------------
 */

static void set_rail(const char *name, bool on) {
  for (size_t i = 0; i < RAIL_COUNT; i++) {
    if (strcmp(name, RAILS[i].name) != 0) {
      continue;
    }
    const esp_err_t err = m5_corep4x_set_switch(RAILS[i].which, on);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "%s update failed: %s", name, esp_err_to_name(err));
      return;
    }
    m5_pocket_bridge_set_bool(RAILS[i].value, on);
    if (RAILS[i].which == M5_COREP4X_SWITCH_AUDIO ||
        RAILS[i].which == M5_COREP4X_SWITCH_SPEAKER) {
      bool codec = false;
      bool amplifier = false;
      m5_corep4x_get_switch(M5_COREP4X_SWITCH_AUDIO, &codec);
      m5_corep4x_get_switch(M5_COREP4X_SWITCH_SPEAKER, &amplifier);
      audio_svc_rails_changed(codec, amplifier);
    }
    return;
  }
  ESP_LOGW(TAG, "unknown rail %s", name);
}

static void set_backlight(int percent) {
  percent = percent < 1 ? 1 : percent > 100 ? 100 : percent;
  if (m5_corep4x_backlight_set((uint32_t)percent) == ESP_OK) {
    s_backlight = (uint8_t)percent;
  }
  m5_pocket_bridge_set_number("backlight", s_backlight);
}

static void set_charge(int value) {
  const m5_corep4x_charge_t current = value == 2   ? M5_COREP4X_CHARGE_650MA
                                      : value == 1 ? M5_COREP4X_CHARGE_180MA
                                                   : M5_COREP4X_CHARGE_OFF;
  const esp_err_t err = m5_corep4x_set_charge(current);
  if (err == ESP_OK) {
    s_vbat_reset = true;
  } else {
    ESP_LOGW(TAG, "charge update failed: %s", esp_err_to_name(err));
  }
  const m5_corep4x_charge_t now = m5_corep4x_charge();
  m5_pocket_bridge_set_number("charge", now == M5_COREP4X_CHARGE_650MA   ? 2
                                        : now == M5_COREP4X_CHARGE_180MA ? 1
                                                                         : 0);
}

/* apply_m5bus_gpio_config. */
static void configure_gpio(int gpio, bool output, bool level, bool open_drain,
                           int pull) {
  esp_err_t err = ESP_OK;
  if (gpio < 0 || gpio >= GPIO_COUNT || !GPIO_IS_VALID_GPIO(gpio) ||
      (output && !GPIO_IS_VALID_OUTPUT_GPIO(gpio))) {
    err = ESP_ERR_INVALID_ARG;
  } else {
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = !output      ? GPIO_MODE_INPUT
                : open_drain ? GPIO_MODE_OUTPUT_OD
                             : GPIO_MODE_OUTPUT,
        .pull_up_en = pull == 1 ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en =
            pull == 2 ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&config);
    if (err == ESP_OK && output) {
      err = gpio_set_level((gpio_num_t)gpio, level ? 1 : 0);
    }
  }
  m5_pocket_bridge_set_text("gpioConfigError",
                            err == ESP_OK ? "" : esp_err_to_name(err));
  m5_pocket_bridge_post_event("gpioConfig", err == ESP_OK ? 0 : -1);
}

static void start_off(bool power_off) {
  if (s_busy) {
    return;
  }
  s_busy = true;
  if (xTaskCreate(off_task, "func_cd", 3072, power_off ? (void *)1 : NULL,
                  configMAX_PRIORITIES - 4, NULL) != pdPASS) {
    s_busy = false;
    if (power_off) {
      m5_pocket_bridge_post_event("powerOff", PM1_ERR_INTERNAL);
    }
  }
}

bool power_svc_request(const m5_pocket_request *request) {
  if (svc_is(request, "rail")) {
    char name[16] = {0};
    int on = 0;
    if (sscanf(svc_rest(request), "%15s %d", name, &on) == 2) {
      set_rail(name, on != 0);
    }
  } else if (svc_is(request, "backlight")) {
    set_backlight(svc_arg(request, 0));
  } else if (svc_is(request, "charge")) {
    set_charge(svc_arg(request, 0));
  } else if (svc_is(request, "powerPage")) {
    s_page = svc_arg(request, 0) != 0;
    s_vbat_reset = true;
  } else if (svc_is(request, "gpioConfig")) {
    configure_gpio(svc_arg(request, 0), svc_arg(request, 1) != 0,
                   svc_arg(request, 2) != 0, svc_arg(request, 3) != 0,
                   svc_arg(request, 4));
  } else if (svc_is(request, "lcdOff")) {
    /* Restart 10 s after the LCD rail goes off; closing the window does
     * not cancel it. */
    if (!s_lcd_restart) {
      const esp_timer_create_args_t args = {.callback = lcd_restart,
                                            .name = "pwr_rst"};
      if (esp_timer_create(&args, &s_lcd_restart) != ESP_OK) {
        s_lcd_restart = NULL;
        return true;
      }
    }
    if (esp_timer_start_once(s_lcd_restart, LCD_OFF_RESTART_US) == ESP_OK) {
      set_rail("LCD", false);
    }
  } else if (svc_is(request, "restart")) {
    if (svc_arg(request, 0) == 1) {
      esp_restart(); /* the Restart button of the rail dialogs */
    }
    start_off(false);
  } else if (svc_is(request, "powerOff")) {
    start_off(true);
  } else if (svc_is(request, "sleepButton")) {
    s_button_sleep = svc_arg(request, 0) != 0;
  } else if (svc_is(request, "sleepWake") || svc_is(request, "deepSleep")) {
    s_wake = (wake_options_t){
        .rtc_seconds = (uint32_t)svc_arg(request, 0),
        .touch = svc_arg(request, 1) != 0,
        .imu = svc_arg(request, 2) != 0,
    };
    if (svc_is(request, "deepSleep") && !s_busy) {
      s_busy = true;
      start_sleep(&s_wake);
    }
  } else {
    return false;
  }
  return true;
}

void power_svc_frame(void) {}

/* "V<version>.<YYMMDD>-<HHMMSS>" from the image's build date and time. */
static void publish_version(void) {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const esp_app_desc_t *desc = esp_app_get_description();
  char month[4] = {0};
  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  sscanf(desc->date, "%3s %d %d", month, &day, &year);
  sscanf(desc->time, "%d:%d:%d", &hour, &minute, &second);
  const char *found = strstr(months, month);
  const int mon = found && month[0] ? (int)(found - months) / 3 + 1 : 0;
  const char *version =
      desc->version[0] == 'v' ? desc->version + 1 : desc->version;
  char text[64];
  snprintf(text, sizeof(text), "V%s.%02d%02d%02d-%02d%02d%02d", version,
           year % 100, mon, day, hour, minute, second);
  m5_pocket_bridge_set_text("firmwareVersion", text);
}

void power_svc_start(void) {
  m5_pocket_bridge_set_bool("pm1Ready", m5_corep4x_pmic_ready());
  m5_pocket_bridge_set_bool("pybReady", m5_corep4x_expander_ready());
  m5_pocket_bridge_set_number("backlight", s_backlight);
  m5_pocket_bridge_set_number("charge", 1);
  m5_pocket_bridge_set_number("vbatFiltered", 0);
  publish_version();
  if (m5_corep4x_pmic_ready()) {
    uint8_t source = 0;
    if (m5_corep4x_wake_source(&source) == ESP_OK) {
      ESP_LOGI(TAG, "PM1 wake source 0x%02X", source);
    }
  }
  /* The button's deep sleep runs the whole sleep sequence on this task. */
  if (xTaskCreate(power_task, "p4_pm1_task", 6144, NULL, 5, &s_task) !=
      pdPASS) {
    ESP_LOGE(TAG, "no power task");
    s_task = NULL;
    return;
  }
  if (m5_corep4x_pmic_ready() &&
      m5_corep4x_pmic_irq_start(on_pmic_irq, NULL) != ESP_OK) {
    ESP_LOGW(TAG, "PM1 IRQ setup incomplete");
  }
}
