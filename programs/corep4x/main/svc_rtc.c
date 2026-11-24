/* RX8130CE time and the TIMER window's requests (svc.h), after the RTC
 * steps of M5CoreP4X-UserDemo's app_main and main/tools.cpp
 * (repair_rtc_time_if_needed) and ui_function_timer_window.cpp.
 *
 * At boot the RTC is started (battery switch-over and charging on, flags
 * cleared), a time outside 2020..2098 is replaced by the image's build
 * date at 00:00:00, and the system clock is set from the RTC (local time
 * UTC+8, TZ "CST-8"). The "rtc" task then publishes rtcYear, rtcMonth
 * (1..12), rtcDay, rtcHour, rtcMinute, rtcSecond and rtcReady every second;
 * without an RTC it publishes the image's build time with rtcReady false
 * and probes the RTC again every second. */
#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_corep4x/rtc.h"
#include "net_sntp.h"
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "rtc_svc";

/* RTC_REFRESH_MS */
#define REFRESH_MS 1000

static TaskHandle_t s_task;
/* The image's build time: the TIMER tiles without an RTC. */
static struct tm s_build;
static bool s_read_failed;

/* fill_weekday: tm_wday from mktime on a copy. */
static void fill_weekday(struct tm *t) {
  struct tm copy = *t;
  copy.tm_isdst = -1;
  if (mktime(&copy) != (time_t)-1) {
    t->tm_wday = copy.tm_wday;
  }
}

static int month_number(const char *name) {
  static const char names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  for (int i = 0; i < 12; i++) {
    if (strncmp(name, names + 3 * i, 3) == 0) {
      return i + 1;
    }
  }
  return 1;
}

/* compile_time_to_tm, from the image description ("Mmm dd yyyy",
 * "hh:mm:ss"); 2025-01-01 00:00:00 when they do not parse. */
static void build_time(struct tm *out) {
  const esp_app_desc_t *desc = esp_app_get_description();
  char month[4] = {0};
  int day = 1;
  int year = 2025;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (sscanf(desc->date, "%3s %d %d", month, &day, &year) != 3 ||
      sscanf(desc->time, "%d:%d:%d", &hour, &minute, &second) != 3) {
    strcpy(month, "Jan");
    day = 1;
    year = 2025;
    hour = minute = second = 0;
  }
  memset(out, 0, sizeof(*out));
  out->tm_year = year - 1900;
  out->tm_mon = month_number(month) - 1;
  out->tm_mday = day;
  out->tm_hour = hour;
  out->tm_min = minute;
  out->tm_sec = second;
  out->tm_isdst = -1;
  fill_weekday(out);
}

/* rtc_time_needs_restore */
static bool needs_restore(const struct tm *t) {
  const int year = t->tm_year + 1900;
  if (year == 2099 || year < 2020 || year > 2099) {
    return true;
  }
  if (t->tm_mon < 0 || t->tm_mon > 11 || t->tm_mday < 1 || t->tm_mday > 31) {
    return true;
  }
  return t->tm_hour < 0 || t->tm_hour > 23 || t->tm_min < 0 || t->tm_min > 59 ||
         t->tm_sec < 0 || t->tm_sec > 59;
}

static int clamp(int value, int low, int high) {
  return value < low ? low : value > high ? high : value;
}

static int days_in(int year, int month) {
  static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return month == 2 && leap ? 29 : days[month - 1];
}

/* normalize_edit_time: each field clamped into its range. */
static void normalize(struct tm *t) {
  const int year = clamp(t->tm_year + 1900, 2020, 2099);
  const int month = clamp(t->tm_mon + 1, 1, 12);
  t->tm_year = year - 1900;
  t->tm_mon = month - 1;
  t->tm_mday = clamp(t->tm_mday, 1, days_in(year, month));
  t->tm_hour = clamp(t->tm_hour, 0, 23);
  t->tm_min = clamp(t->tm_min, 0, 59);
  t->tm_sec = clamp(t->tm_sec, 0, 59);
}

static void set_system_clock(const struct tm *local) {
  struct tm copy = *local;
  copy.tm_isdst = -1;
  const time_t now = mktime(&copy);
  if (now == (time_t)-1) {
    return;
  }
  const struct timeval tv = {.tv_sec = now, .tv_usec = 0};
  settimeofday(&tv, NULL);
}

/* The values before rtcReady, so the application never sees a ready flag
 * with stale fields. */
static void publish(bool ready, const struct tm *t) {
  m5_pocket_bridge_set_number("rtcYear", t->tm_year + 1900);
  m5_pocket_bridge_set_number("rtcMonth", t->tm_mon + 1);
  m5_pocket_bridge_set_number("rtcDay", t->tm_mday);
  m5_pocket_bridge_set_number("rtcHour", t->tm_hour);
  m5_pocket_bridge_set_number("rtcMinute", t->tm_min);
  m5_pocket_bridge_set_number("rtcSecond", t->tm_sec);
  m5_pocket_bridge_set_bool("rtcReady", ready);
}

/* refresh_rtc_time with ensure_rtc_ready's retry. */
static void refresh(void) {
  if (m5_corep4x_rtc_init() != ESP_OK) {
    publish(false, &s_build);
    return;
  }
  struct tm now;
  const esp_err_t err = m5_corep4x_rtc_get(&now);
  if (err != ESP_OK) {
    if (!s_read_failed) {
      ESP_LOGW(TAG, "RX8130 read failed: %s", esp_err_to_name(err));
    }
    s_read_failed = true;
    return;
  }
  s_read_failed = false;
  publish(true, &now);
}

/* set_rtc_time */
static esp_err_t save(struct tm *t, const char *label) {
  if (m5_corep4x_rtc_init() != ESP_OK) {
    ESP_LOGW(TAG, "%s save failed: RX8130 is not ready.", label);
    return ESP_ERR_INVALID_STATE;
  }
  fill_weekday(t);
  const esp_err_t err = m5_corep4x_rtc_set(t);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "%s save failed: %s", label, esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "%s saved: %04d-%02d-%02d %02d:%02d:%02d", label,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
             t->tm_min, t->tm_sec);
  }
  refresh();
  return err;
}

static volatile bool s_sleeping;

static void rtc_task(void *arg) {
  (void)arg;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(REFRESH_MS));
    if (!s_sleeping) {
      refresh();
    }
  }
}

void rtc_svc_sleep(void) { s_sleeping = true; }

void rtc_svc_start(void) {
  setenv("TZ", "CST-8", 1);
  tzset();
  build_time(&s_build);
  struct tm now;
  if (m5_corep4x_rtc_init() == ESP_OK && m5_corep4x_rtc_get(&now) == ESP_OK) {
    if (needs_restore(&now)) {
      struct tm restored = s_build;
      restored.tm_hour = 0;
      restored.tm_min = 0;
      restored.tm_sec = 0;
      ESP_LOGW(TAG,
               "RTC time is unreasonable (%04d-%02d-%02d %02d:%02d:%02d), "
               "restoring to firmware date %04d-%02d-%02d 00:00:00",
               now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour,
               now.tm_min, now.tm_sec, restored.tm_year + 1900,
               restored.tm_mon + 1, restored.tm_mday);
      if (m5_corep4x_rtc_set(&restored) == ESP_OK) {
        now = restored;
      }
    }
    set_system_clock(&now);
    publish(true, &now);
    ESP_LOGI(TAG, "RTC (RX8130) initialized successfully");
  } else {
    ESP_LOGE(TAG, "RTC init failed");
    publish(false, &s_build);
  }
  if (xTaskCreate(rtc_task, "rtc", 4096, NULL, 4, &s_task) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create the RTC task");
  }
}

void rtc_svc_frame(void) {}

bool rtc_svc_request(const m5_pocket_request *request) {
  if (svc_is(request, "timerPage")) {
    const bool open = svc_arg(request, 0) != 0;
    net_svc_timer_page(open);
    if (open && s_task) {
      xTaskNotifyGive(s_task);
    }
    return true;
  }
  if (svc_is(request, "rtcSet")) {
    if (request->argc < 6) {
      ESP_LOGW(TAG, "rtcSet needs 6 values");
      return true;
    }
    struct tm t = {
        .tm_year = svc_arg(request, 0) - 1900,
        .tm_mon = svc_arg(request, 1) - 1,
        .tm_mday = svc_arg(request, 2),
        .tm_hour = svc_arg(request, 3),
        .tm_min = svc_arg(request, 4),
        .tm_sec = svc_arg(request, 5),
        .tm_isdst = -1,
    };
    normalize(&t);
    save(&t, "Date");
    return true;
  }
  if (svc_is(request, "rtcNtp")) {
    if (!net_svc_sntp_synced()) {
      return true;
    }
    struct tm t;
    if (!net_sntp_local_time(&t)) {
      net_svc_sntp_forget();
      ESP_LOGW(TAG, "NTP button ignored: system clock is not synchronized");
      return true;
    }
    save(&t, "NTP");
    return true;
  }
  return false;
}

esp_err_t rtc_svc_save_system_time(void) {
  struct tm t;
  if (!net_sntp_local_time(&t)) {
    return ESP_ERR_INVALID_STATE;
  }
  return save(&t, "System clock");
}
