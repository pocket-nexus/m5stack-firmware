/* Board services of the CoreP4X factory demo: the request dispatch, the
 * per-frame publishing and the helpers the service modules share. */
#include "services.h"

#include "esp_log.h"
#include "m5_corep4x/bsp.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "svc.h"
#include <string.h>

static const char *TAG = "corep4x_svc";
static const char *PREFS = "corep4_prefs";

static esp_lcd_touch_handle_t s_touch;
static m5_corep4x_display_t *s_lcd;
static m5_pocket_app *s_app;
static bool s_attached;

bool svc_is(const m5_pocket_request *request, const char *name) {
  const size_t length = strlen(name);
  return request->name_length == length &&
         memcmp(request->name, name, length) == 0;
}

const char *svc_rest(const m5_pocket_request *request) {
  if (request->name_length >= request->text_length) {
    return "";
  }
  return request->text + request->name_length + 1;
}

int svc_arg(const m5_pocket_request *request, size_t i) {
  return i < request->argc ? (int)request->args[i] : 0;
}

m5_pocket_app *svc_app(void) { return s_app; }

esp_lcd_touch_handle_t svc_touch(void) { return s_touch; }

esp_err_t svc_texture(const char *name, const uint16_t *pixels, uint32_t width,
                      uint32_t height, const char *handle_value) {
  if (!s_app) {
    return ESP_ERR_INVALID_STATE;
  }
  int32_t handle = -1;
  const esp_err_t err = m5_pocket_app_set_texture(
      s_app, name, pixels, width, height, (size_t)width * 2U, &handle);
  if (err == ESP_OK) {
    m5_pocket_bridge_set_number(handle_value, handle);
  }
  return err;
}

size_t svc_contacts(uint16_t *xs, uint16_t *ys, size_t capacity) {
  if (!s_touch || capacity == 0) {
    return 0;
  }
  esp_lcd_touch_point_data_t points[5];
  uint8_t count = 0;
  const uint8_t max = capacity < 5 ? (uint8_t)capacity : 5;
  if (esp_lcd_touch_get_data(s_touch, points, &count, max) != ESP_OK) {
    return 0;
  }
  for (uint8_t i = 0; i < count; i++) {
    /* The factory's net mapping is raw - 1 (mirror in the driver, then
     * LVGL's 180-degree rotation). */
    xs[i] = points[i].x > 0 ? (uint16_t)(points[i].x - 1) : 0;
    ys[i] = points[i].y > 0 ? (uint16_t)(points[i].y - 1) : 0;
    if (xs[i] >= M5_COREP4X_LCD_WIDTH) {
      xs[i] = M5_COREP4X_LCD_WIDTH - 1;
    }
    if (ys[i] >= M5_COREP4X_LCD_HEIGHT) {
      ys[i] = M5_COREP4X_LCD_HEIGHT - 1;
    }
  }
  return count;
}

bool svc_pref_get(const char *key, bool fallback) {
  nvs_handle_t nvs;
  if (nvs_open(PREFS, NVS_READONLY, &nvs) != ESP_OK) {
    return fallback;
  }
  uint8_t value = fallback ? 1 : 0;
  if (nvs_get_u8(nvs, key, &value) != ESP_OK) {
    value = fallback ? 1 : 0;
  }
  nvs_close(nvs);
  return value != 0;
}

typedef struct {
  const char *key;
  bool value;
} pref_write_t;

static void pref_write(void *context) {
  const pref_write_t *w = context;
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(PREFS, NVS_READWRITE, &nvs);
  if (err == ESP_OK) {
    err = nvs_set_u8(nvs, w->key, w->value ? 1 : 0);
    if (err == ESP_OK) {
      err = nvs_commit(nvs);
    }
    nvs_close(nvs);
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "NVS %s: %s", w->key, esp_err_to_name(err));
  }
}

void svc_pref_set(const char *key, bool value) {
  pref_write_t w = {.key = key, .value = value};
  if (m5_pocket_app_run_internal(pref_write, &w) != ESP_OK) {
    pref_write(&w);
  }
}

void services_display_off(bool touch_off) {
  if (touch_off && s_touch) {
    const esp_err_t err = m5_corep4x_touch_power_down(s_touch);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "touch power down: %s", esp_err_to_name(err));
    }
    s_touch = NULL;
  }
  if (s_lcd) {
    m5_corep4x_display_del(s_lcd);
  }
}

void services_start(esp_lcd_touch_handle_t touch, m5_corep4x_display_t *lcd) {
  s_touch = touch;
  s_lcd = lcd;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  power_svc_start();
  rtc_svc_start();
  storage_svc_start();
  i2c_svc_start();
  canvas_svc_start();
  imu_svc_start();
  audio_svc_start();
  net_svc_start();
  ir_svc_start();
}

void services_set_app(m5_pocket_app *app) { s_app = app; }

void services_publish(void *context) {
  (void)context;
  if (!s_attached) {
    s_attached = true;
    m5_pocket_bridge_set_bool("hostAttached", true);
  }
  power_svc_frame();
  rtc_svc_frame();
  storage_svc_frame();
  i2c_svc_frame();
  canvas_svc_frame();
  imu_svc_frame();
  audio_svc_frame();
  net_svc_frame();
  ir_svc_frame();
}

/* The longest request the application sends ("sdList <path>"). */
#define REQUEST_TEXT_MAX 256

void services_request(void *context, const m5_pocket_request *request) {
  (void)context;
  if (!request || request->struct_size < sizeof(*request)) {
    return;
  }
  /* AOT mode passes the bytes of the request without a terminator: the
   * modules get a NUL-terminated copy (svc_rest, sscanf). */
  char text[REQUEST_TEXT_MAX];
  const size_t length = request->text_length < sizeof(text) - 1
                            ? request->text_length
                            : sizeof(text) - 1;
  memcpy(text, request->text, length);
  text[length] = '\0';
  m5_pocket_request copy = *request;
  copy.text = text;
  copy.text_length = length;
  if (request->name >= request->text &&
      request->name + request->name_length <= request->text + length) {
    copy.name = text + (request->name - request->text);
  } else {
    copy.name = text;
    copy.name_length = 0;
  }
  request = &copy;
  if (svc_is(request, "hello")) {
    return;
  }
  if (power_svc_request(request) || rtc_svc_request(request) ||
      storage_svc_request(request) || i2c_svc_request(request) ||
      canvas_svc_request(request) || imu_svc_request(request) ||
      audio_svc_request(request) || net_svc_request(request) ||
      ir_svc_request(request)) {
    return;
  }
  ESP_LOGW(TAG, "unhandled request: %s", text);
}
