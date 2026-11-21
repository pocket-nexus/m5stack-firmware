/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/* The factory HAL (platforms/tab5/main/hal of M5Tab5-UserDemo) behind the
 * device bridge: board values are published under the field names of
 * apps/tab5-factory/device.ts, requests arrive as "<name> <arg>..." strings.
 * The polling periods are the factory panels' (power monitor and IMU 100 ms,
 * switches 200 ms, clock and CPU temperature 1 s). */
#include "services.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5/tab5.h"
#include "sound.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "tab5_services";

#define SD_MOUNT "/sd"
#define SD_ENTRIES_MAX 64
#define RS485_CHUNK 128
#define GPIO_TEST_MAX 24

/* ExtPortWindow and MbusWindow pins of panel_gpio.cpp. */
static const int GPIO_TEST_PINS[GPIO_TEST_MAX] = {
    49, 50, 0,  1,  54, 53, 18, 19, 5, 38, 7,  3,
    2,  47, 16, 17, 45, 52, 37, 6,  4, 48, 35, 51};

typedef struct {
  tab5_services_config_t config;
  int64_t next_fast, next_switches, next_slow, next_scan;
  int i2c_bus;
  i2c_master_bus_handle_t port_a;
  bool rs485_open;
  int backlight;
  int gpio_pins[GPIO_TEST_MAX];
  size_t gpio_count;
} services_t;

static services_t services;

/* update_system_time: the RTC holds the wall clock, the system clock follows
 * it. */
static void sync_system_time(void) {
  struct tm time;
  if (m5_tab5_rtc_get(&time) != ESP_OK)
    return;
  const struct timeval now = {.tv_sec = mktime(&time)};
  settimeofday(&now, NULL);
}

void tab5_services_start(const tab5_services_config_t *config) {
  services.config = *config;
  services.backlight = 100;
  if (m5_tab5_rtc_init() == ESP_OK)
    sync_system_time();
  else
    ESP_LOGE(TAG, "RX8130CE not answering");
  if (m5_tab5_power_monitor_init() != ESP_OK)
    ESP_LOGE(TAG, "INA226 not answering");
  if (m5_tab5_imu_init() != ESP_OK)
    ESP_LOGE(TAG, "BMI270 not answering");
  if (m5_tab5_rs485_init(115200) != ESP_OK)
    ESP_LOGE(TAG, "RS485 UART");
  if (tab5_sound_start() != ESP_OK)
    ESP_LOGE(TAG, "audio codecs");
  tab5_sound_set_volume(60);
  m5_pocket_bridge_set_text("panelIc", config->panel_name);
}

static void publish_clock(void) {
  const time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  m5_pocket_bridge_set_number("rtcYear", local.tm_year + 1900);
  m5_pocket_bridge_set_number("rtcMonth", local.tm_mon + 1);
  m5_pocket_bridge_set_number("rtcDay", local.tm_mday);
  m5_pocket_bridge_set_number("rtcHour", local.tm_hour);
  m5_pocket_bridge_set_number("rtcMinute", local.tm_min);
  m5_pocket_bridge_set_number("rtcSecond", local.tm_sec);
}

static void publish_i2c_scan(void) {
  i2c_master_bus_handle_t bus =
      services.i2c_bus == 1 ? m5_tab5_i2c() : services.port_a;
  uint8_t found[112];
  size_t count = 0;
  float addresses[112];
  /* Without EXT 5V the Port A devices are unpowered; the factory skips the
   * scan so that a missing pull-up cannot stall it. */
  bool ext5v = false;
  m5_tab5_get_switch(M5_TAB5_SWITCH_EXT_5V, &ext5v);
  if (bus && (services.i2c_bus == 1 || ext5v))
    m5_tab5_i2c_scan(bus, found, sizeof(found), &count);
  for (size_t i = 0; i < count; i++)
    addresses[i] = found[i];
  m5_pocket_bridge_set_array("i2cAddresses", addresses, count);
  m5_pocket_bridge_post_event("i2cScan", 1);
}

static void poll_rs485(void) {
  uint8_t data[RS485_CHUNK + 1];
  size_t received = 0;
  if (m5_tab5_rs485_read(data, RS485_CHUNK, &received, 0) != ESP_OK ||
      received == 0)
    return;
  data[received] = 0;
  /* Drop the line ends; the monitor shows each chunk as a line. */
  while (received > 0 &&
         (data[received - 1] == '\n' || data[received - 1] == '\r'))
    data[--received] = 0;
  m5_pocket_bridge_set_text("rs485Text", (const char *)data);
  m5_pocket_bridge_post_event("rs485", 1);
}

void tab5_services_publish(void *context) {
  (void)context;
  const int64_t now = esp_timer_get_time();
  publish_clock();
  if (now >= services.next_fast) {
    services.next_fast = now + 100000;
    m5_tab5_power_reading_t power;
    if (m5_tab5_power_monitor_read(&power) == ESP_OK) {
      m5_pocket_bridge_set_number("busVoltage", power.bus_voltage);
      m5_pocket_bridge_set_number("shuntCurrent", power.current);
    }
    m5_tab5_imu_reading_t imu;
    if (m5_tab5_imu_read(&imu) == ESP_OK) {
      m5_pocket_bridge_set_number("accelX", imu.accel_x);
      m5_pocket_bridge_set_number("accelY", imu.accel_y);
      m5_pocket_bridge_set_number("accelZ", imu.accel_z);
      m5_pocket_bridge_set_number("gyroX", imu.gyro_x);
      m5_pocket_bridge_set_number("gyroY", imu.gyro_y);
      m5_pocket_bridge_set_number("gyroZ", imu.gyro_z);
    }
  }
  if (now >= services.next_switches) {
    services.next_switches = now + 200000;
    bool level = false;
    if (m5_tab5_headphone_detect(&level) == ESP_OK)
      m5_pocket_bridge_set_bool("headphoneConnected", level);
    if (m5_tab5_usb_c_detect(&level) == ESP_OK)
      m5_pocket_bridge_set_bool("usbCConnected", level);
    /* No USB host stack: the USB-A port reads as empty. */
    m5_pocket_bridge_set_bool("usbAConnected", false);
  }
  if (now >= services.next_slow) {
    services.next_slow = now + 1000000;
    float celsius = 0;
    if (m5_tab5_cpu_temperature(&celsius) == ESP_OK)
      m5_pocket_bridge_set_number("cpuTemperature", (int)celsius);
  }
  if (services.i2c_bus != 0 && now >= services.next_scan) {
    services.next_scan = now + 300000;
    publish_i2c_scan();
  }
  if (services.rs485_open)
    poll_rs485();
  m5_pocket_bridge_set_number("recordTestState", tab5_sound_record_state());
  m5_pocket_bridge_set_bool("musicPlaying", tab5_sound_music_playing());
  float left[TAB5_SOUND_COLUMNS], right[TAB5_SOUND_COLUMNS];
  if (tab5_sound_columns(left, right)) {
    m5_pocket_bridge_set_array("captureLeft", left, TAB5_SOUND_COLUMNS);
    m5_pocket_bridge_set_array("captureRight", right, TAB5_SOUND_COLUMNS);
  }
}

static bool named(const m5_pocket_request *request, const char *name) {
  return request->name_length == strlen(name) &&
         memcmp(request->name, name, request->name_length) == 0;
}

static int arg(const m5_pocket_request *request, size_t index) {
  return index < request->argc ? (int)request->args[index] : 0;
}

/* The word after the request name. */
static bool word(const m5_pocket_request *request, size_t index, char *out,
                 size_t capacity) {
  const char *text = request->text;
  const char *end = text + request->text_length;
  for (size_t i = 0; i <= index; i++) {
    while (text < end && *text != ' ')
      text++;
    while (text < end && *text == ' ')
      text++;
  }
  size_t length = 0;
  while (text + length < end && text[length] != ' ' && length + 1 < capacity)
    length++;
  memcpy(out, text, length);
  out[length] = 0;
  return length > 0;
}

static void set_switch(const m5_pocket_request *request) {
  static const struct {
    const char *name;
    m5_tab5_switch_t which;
  } switches[] = {
      {"charge", M5_TAB5_SWITCH_CHARGE},
      {"quickCharge", M5_TAB5_SWITCH_QUICK_CHARGE},
      {"ext5v", M5_TAB5_SWITCH_EXT_5V},
      {"usbA5v", M5_TAB5_SWITCH_USB_A_5V},
      {"extAntenna", M5_TAB5_SWITCH_EXT_ANTENNA},
  };
  char name[16];
  if (!word(request, 0, name, sizeof(name)))
    return;
  for (size_t i = 0; i < sizeof(switches) / sizeof(switches[0]); i++)
    if (strcmp(name, switches[i].name) == 0)
      m5_tab5_set_switch(switches[i].which, arg(request, 0) != 0);
}

/* HalEsp32::setRtcTime: the chip, then the system clock from it. */
static void set_rtc(const m5_pocket_request *request) {
  struct tm time = {
      .tm_year = arg(request, 0) - 1900,
      .tm_mon = arg(request, 1) - 1,
      .tm_mday = arg(request, 2),
      .tm_hour = arg(request, 3),
      .tm_min = arg(request, 4),
      .tm_sec = arg(request, 5),
  };
  mktime(&time); /* fills tm_wday */
  if (m5_tab5_rtc_set(&time) == ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(50));
    sync_system_time();
  }
}

static bool touched(void) {
  esp_lcd_touch_point_data_t point;
  uint8_t count = 0;
  return esp_lcd_touch_read_data(services.config.touch) == ESP_OK &&
         esp_lcd_touch_get_data(services.config.touch, &point, &count, 1) ==
             ESP_OK &&
         count > 0;
}

/* HalEsp32::powerOff, sleepAndTouchWakeup, sleepAndShakeWakeup and
 * sleepAndRtcWakeup. */
static void power(int action) {
  if (action == 2) {
    m5_tab5_backlight_set(0);
    while (touched())
      vTaskDelay(pdMS_TO_TICKS(100));
    while (!touched())
      vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(500));
    m5_tab5_backlight_set(services.backlight);
    return;
  }
  if (action == 3 || action == 4) {
    m5_tab5_rtc_clear_irq();
    m5_tab5_imu_clear_irq();
    vTaskDelay(pdMS_TO_TICKS(200));
    if (action == 3) {
      m5_tab5_imu_enable_motion_irq();
    } else {
      /* The factory wakes on the RTC alarm 11 s later by setting the clock to
       * 00:01:49 and the alarm to the next minute. */
      struct tm time;
      if (m5_tab5_rtc_get(&time) == ESP_OK) {
        time.tm_hour = 0;
        time.tm_min = 1;
        time.tm_sec = 49;
        m5_tab5_rtc_set(&time);
        m5_tab5_rtc_set_alarm(&time);
      }
    }
  }
  tab5_sound_clip(TAB5_CLIP_SHUTDOWN);
  m5_tab5_backlight_set(0);
  vTaskDelay(pdMS_TO_TICKS(100));
  while (tab5_sound_busy())
    vTaskDelay(pdMS_TO_TICKS(100));
  m5_tab5_power_off();
}

static void set_i2c_scan(int bus) {
  if (bus == 2 && !services.port_a)
    m5_tab5_port_a_i2c_init(&services.port_a);
  if (bus != 2 && services.port_a) {
    m5_tab5_port_a_i2c_deinit();
    services.port_a = NULL;
  }
  services.i2c_bus = bus;
  services.next_scan = 0;
}

/* MbusWindow / ExtPortWindow: gpioInitOutput, gpioSetLevel and gpioReset. */
static void gpio_test(const m5_pocket_request *request) {
  for (size_t i = 0; i < services.gpio_count; i++)
    gpio_set_level(services.gpio_pins[i], 0);
  services.gpio_count = 0;
  if (arg(request, 0) == 0)
    return;
  if (services.port_a) {
    m5_tab5_port_a_i2c_deinit();
    services.port_a = NULL;
  }
  for (size_t i = 0; i < sizeof(GPIO_TEST_PINS) / sizeof(GPIO_TEST_PINS[0]);
       i++) {
    const int pin = GPIO_TEST_PINS[i];
    gpio_reset_pin(pin);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    services.gpio_pins[services.gpio_count++] = pin;
  }
}

static void set_gpio(const m5_pocket_request *request) {
  const int pin = arg(request, 0);
  for (size_t i = 0; i < services.gpio_count; i++)
    if (services.gpio_pins[i] == pin)
      gpio_set_level(pin, arg(request, 1) != 0);
}

/* HalEsp32::scanSdCard: mount, list the root, unmount. */
static void scan_sd(void) {
  static char list[1024];
  float directories[SD_ENTRIES_MAX];
  size_t count = 0;
  size_t used = 0;
  list[0] = 0;
  sdmmc_card_t *card = NULL;
  if (m5_tab5_sdcard_mount(SD_MOUNT, 25, &card) == ESP_OK) {
    DIR *dir = opendir(SD_MOUNT "/");
    struct dirent *entry;
    while (dir && (entry = readdir(dir)) != NULL && count < SD_ENTRIES_MAX) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      const size_t length = strlen(entry->d_name);
      if (used + length + 2 > sizeof(list))
        break;
      if (used)
        list[used++] = '/';
      memcpy(list + used, entry->d_name, length);
      used += length;
      list[used] = 0;
      directories[count++] = entry->d_type == DT_DIR ? 1.0f : 0.0f;
    }
    if (dir)
      closedir(dir);
    m5_tab5_sdcard_unmount(SD_MOUNT, card);
  } else {
    ESP_LOGW(TAG, "no microSD card mounted");
  }
  m5_pocket_bridge_set_text("sdList", list);
  m5_pocket_bridge_set_array("sdDirectories", directories, count);
  m5_pocket_bridge_post_event("sdScan", 1);
}

void tab5_services_request(void *context, const m5_pocket_request *request) {
  (void)context;
  if (named(request, "backlight")) {
    services.backlight = arg(request, 0);
    m5_tab5_backlight_set(services.backlight);
  } else if (named(request, "volume")) {
    tab5_sound_set_volume(arg(request, 0));
  } else if (named(request, "switch")) {
    set_switch(request);
  } else if (named(request, "tone")) {
    int notes[8];
    size_t count = 0;
    for (size_t i = 1; i < request->argc && count < 8; i++)
      notes[count++] = arg(request, i);
    tab5_sound_tones(notes, count, arg(request, 0));
  } else if (named(request, "sound")) {
    tab5_sound_clip(TAB5_CLIP_STARTUP);
  } else if (named(request, "rtc")) {
    set_rtc(request);
  } else if (named(request, "power")) {
    power(arg(request, 0));
  } else if (named(request, "i2cScan")) {
    set_i2c_scan(arg(request, 0));
  } else if (named(request, "gpioTest")) {
    gpio_test(request);
  } else if (named(request, "gpio")) {
    set_gpio(request);
  } else if (named(request, "capture")) {
    tab5_sound_capture(arg(request, 0));
  } else if (named(request, "recordTest")) {
    tab5_sound_record_test(arg(request, 0));
  } else if (named(request, "music")) {
    if (arg(request, 0))
      tab5_sound_clip(TAB5_CLIP_MUSIC);
    else
      tab5_sound_stop_music();
  } else if (named(request, "rs485")) {
    services.rs485_open = arg(request, 0) != 0;
  } else if (named(request, "rs485Send")) {
    const char *text = memchr(request->text, ' ', request->text_length);
    if (text) {
      text++;
      const size_t length =
          request->text_length - (size_t)(text - request->text);
      m5_tab5_rs485_write((const uint8_t *)text, length);
      m5_tab5_rs485_write((const uint8_t *)"\n", 1);
    }
  } else if (named(request, "camera")) {
    /* No camera pipeline: the preview reports what was asked. */
    m5_pocket_bridge_set_bool("cameraCapturing", arg(request, 0) != 0);
  } else if (named(request, "sdScan")) {
    scan_sd();
  } else {
    ESP_LOGW(TAG, "unknown request %.*s", (int)request->text_length,
             request->text);
  }
}
