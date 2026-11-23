// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-License-Identifier: MIT
//
// Hardware behind the factory functions of M5AtomS3-UserDemo src/main.cpp,
// driven by the requests of apps/atoms3-factory/device.ts.
#include "atoms3_device.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "m5_atoms3/ir.h"
#include "m5_atoms3/port_a.h"
#include "nvs_flash.h"
#include "pocket_spec.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "atoms3_device";

#define WIFI_SHOWN 5
#define USB_EVENTS_PER_FRAME 8

static const char *const FUNCTION_NAMES[] = {"wifi", "i2c", "uart", "pwm",
                                             "adc",  "ir",  "imu"};

esp_err_t atoms3_device_init(atoms3_device_t *device) {
  memset(device, 0, sizeof(*device));
  device->active = ATOMS3_FUNC_NONE;
  m5_atoms3_button_init(&device->button);
  ESP_RETURN_ON_ERROR(m5_atoms3_button_gpio_init(), TAG, "button");

  esp_err_t err = m5_atoms3_i2c_internal_init(&device->internal_i2c);
  if (err == ESP_OK) {
    device->imu_ready =
        m5_atoms3_imu_init(device->internal_i2c, &device->imu) == ESP_OK;
  }
  if (!device->imu_ready) {
    ESP_LOGW(TAG, "IMU unavailable; the IMU function shows a centred ball");
  }
  device->ir_ready = m5_atoms3_ir_init() == ESP_OK;
  device->usb_ready = m5_atoms3_usb_serial_init() == ESP_OK;

  // setup(): ir_addr = (mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5])
  // stored in a uint16_t, so the low two bytes.
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  device->ir_address = (uint16_t)((mac[4] << 8) | mac[5]);
  m5_pocket_bridge_set_number("irAddress", device->ir_address);
  m5_pocket_bridge_set_bool("attached", true);
  return ESP_OK;
}

uint32_t atoms3_device_buttons(void *context) {
  atoms3_device_t *device = context;
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
  switch (
      m5_atoms3_button_update(&device->button, now, m5_atoms3_button_read())) {
  case M5_ATOMS3_BUTTON_CLICKED:
    return POCKET_BTN_CIRCLE;
  case M5_ATOMS3_BUTTON_HOLD:
    return POCKET_BTN_CROSS;
  default:
    return 0;
  }
}

// Wi-Fi ----------------------------------------------------------------------

static void wifi_event(void *arg, esp_event_base_t base, int32_t id,
                       void *data) {
  (void)base;
  (void)data;
  atoms3_device_t *device = arg;
  if (id == WIFI_EVENT_SCAN_DONE) {
    atomic_store(&device->wifi_scan_done, true);
  }
}

static void wifi_start(atoms3_device_t *device) {
  static bool stack_ready;
  if (device->wifi_started) {
    return;
  }
  if (!stack_ready) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      nvs_flash_init();
    }
    if (esp_netif_init() != ESP_OK ||
        esp_event_loop_create_default() != ESP_OK ||
        !esp_netif_create_default_wifi_sta() ||
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_event,
                                   device) != ESP_OK) {
      ESP_LOGE(TAG, "Wi-Fi stack init failed");
      return;
    }
    stack_ready = true;
  }
  wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&config);
  if (err == ESP_OK) {
    err = esp_wifi_set_mode(WIFI_MODE_STA);
  }
  if (err == ESP_OK) {
    err = esp_wifi_start();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi start: %s", esp_err_to_name(err));
    esp_wifi_deinit();
    m5_pocket_bridge_post_event("wifiDone", -1);
    return;
  }
  device->wifi_started = true;
}

static void wifi_stop(atoms3_device_t *device) {
  if (!device->wifi_started) {
    return;
  }
  esp_wifi_scan_stop();
  esp_wifi_stop();
  esp_wifi_deinit();
  device->wifi_started = false;
  atomic_store(&device->wifi_scan_done, false);
}

static void wifi_scan(atoms3_device_t *device) {
  wifi_start(device);
  if (!device->wifi_started) {
    return;
  }
  // WiFi.scanNetworks(true): an asynchronous active scan of all channels.
  esp_err_t err = esp_wifi_scan_start(NULL, false);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "scan start: %s", esp_err_to_name(err));
    m5_pocket_bridge_post_event("wifiDone", -1);
  }
}

static void wifi_publish(void) {
  uint16_t total = 0;
  esp_wifi_scan_get_ap_num(&total);
  wifi_ap_record_t records[WIFI_SHOWN];
  uint16_t count = WIFI_SHOWN;
  memset(records, 0, sizeof(records));
  if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) {
    count = 0;
  }
  float rssi[WIFI_SHOWN] = {0};
  for (uint16_t i = 0; i < count; i++) {
    char name[16];
    snprintf(name, sizeof(name), "wifi%u", (unsigned)(i + 1));
    m5_pocket_bridge_set_text(name, (const char *)records[i].ssid);
    rssi[i] = records[i].rssi;
  }
  m5_pocket_bridge_set_array("wifiRssi", rssi, count);
  m5_pocket_bridge_post_event("wifiDone", total);
  ESP_LOGI(TAG, "Wi-Fi scan: %u networks", (unsigned)total);
}

// Functions -----------------------------------------------------------------

static atoms3_function_t function_named(const m5_pocket_request *request) {
  const char *end = request->text + request->text_length;
  const char *arg = request->name + request->name_length;
  while (arg < end && *arg == ' ') {
    arg++;
  }
  size_t length = (size_t)(end - arg);
  for (int i = 0; i < (int)(sizeof(FUNCTION_NAMES) / sizeof(*FUNCTION_NAMES));
       i++) {
    if (strlen(FUNCTION_NAMES[i]) == length &&
        memcmp(arg, FUNCTION_NAMES[i], length) == 0) {
      return (atoms3_function_t)i;
    }
  }
  return ATOMS3_FUNC_NONE;
}

static void leave(atoms3_device_t *device) {
  switch (device->active) {
  case ATOMS3_FUNC_WIFI:
    // func_wifi_t::stop calls WiFi.scanDelete; the radio is also released
    // here to return its memory to the heap.
    wifi_stop(device);
    break;
  case ATOMS3_FUNC_UART:
    m5_atoms3_port_a_uart_close();
    device->uart_open = false;
    break;
  case ATOMS3_FUNC_PWM:
    m5_atoms3_port_a_pwm_close();
    device->pwm_open = false;
    break;
  case ATOMS3_FUNC_ADC:
    m5_atoms3_port_a_adc_close();
    device->adc_open = false;
    break;
  default:
    break;
  }
  device->active = ATOMS3_FUNC_NONE;
}

static bool is(const m5_pocket_request *request, const char *name) {
  return request->name_length == strlen(name) &&
         memcmp(request->name, name, request->name_length) == 0;
}

void atoms3_device_request(void *context, const m5_pocket_request *request) {
  atoms3_device_t *device = context;
  if (is(request, "enter")) {
    leave(device);
    device->active = function_named(request);
    if (device->active == ATOMS3_FUNC_ADC) {
      device->adc_open = m5_atoms3_port_a_adc_open() == ESP_OK;
    }
  } else if (is(request, "leave")) {
    leave(device);
  } else if (is(request, "wifiScan")) {
    wifi_scan(device);
  } else if (is(request, "i2cScan")) {
    uint8_t addresses[6];
    size_t count = 0;
    if (m5_atoms3_port_a_i2c_scan(addresses, 6, &count) != ESP_OK) {
      count = 0;
    }
    float values[6];
    for (size_t i = 0; i < count; i++) {
      values[i] = addresses[i];
    }
    m5_pocket_bridge_set_array("i2cAddresses", values, count);
    m5_pocket_bridge_post_event("i2cDone", (double)count);
  } else if (is(request, "uart") && request->argc == 3) {
    if (device->uart_open) {
      m5_atoms3_port_a_uart_close();
    }
    device->uart_open = m5_atoms3_port_a_uart_open(
                            (uint32_t)request->args[0], (int)request->args[1],
                            (int)request->args[2]) == ESP_OK;
  } else if (is(request, "pwm") && request->argc == 2) {
    uint8_t g1 = (uint8_t)request->args[0];
    uint8_t g2 = (uint8_t)request->args[1];
    if (!device->pwm_open) {
      device->pwm_open = m5_atoms3_port_a_pwm_open(g1, g2) == ESP_OK;
    } else {
      m5_atoms3_port_a_pwm_set(g1, g2);
    }
  } else if (is(request, "adc")) {
    int g1 = 0, g2 = 0;
    if (device->adc_open && m5_atoms3_port_a_adc_read(32, &g1, &g2) == ESP_OK) {
      const float values[2] = {(float)g1, (float)g2};
      m5_pocket_bridge_set_array("adc", values, 2);
    }
  } else if (is(request, "ir") && request->argc == 2) {
    if (device->ir_ready) {
      m5_atoms3_ir_send_nec((uint16_t)request->args[0],
                            (uint16_t)request->args[1]);
    }
  } else {
    ESP_LOGW(TAG, "unknown request: %.*s", (int)request->text_length,
             request->text);
  }
}

void atoms3_device_before_frame(void *context) {
  atoms3_device_t *device = context;
  uint8_t bytes[64];
  switch (device->active) {
  case ATOMS3_FUNC_WIFI:
    if (atomic_exchange(&device->wifi_scan_done, false)) {
      wifi_publish();
    }
    break;
  case ATOMS3_FUNC_UART:
    if (!device->uart_open || !device->usb_ready) {
      break;
    }
    // Grove => USB, USB => Grove. The screen shows the last byte of each
    // direction, so one event per direction and frame is published.
    for (int n; (n = m5_atoms3_port_a_uart_read(bytes, sizeof(bytes))) > 0;) {
      m5_atoms3_usb_serial_write(bytes, (size_t)n);
      m5_pocket_bridge_post_event("uartRx", bytes[n - 1]);
    }
    for (int n; (n = m5_atoms3_usb_serial_read(bytes, sizeof(bytes))) > 0;) {
      m5_atoms3_port_a_uart_write(bytes, (size_t)n);
      m5_pocket_bridge_post_event("uartTx", bytes[n - 1]);
    }
    break;
  case ATOMS3_FUNC_PWM:
  case ATOMS3_FUNC_IR:
    if (device->usb_ready) {
      int n = m5_atoms3_usb_serial_read(bytes, USB_EVENTS_PER_FRAME);
      for (int i = 0; i < n; i++) {
        m5_pocket_bridge_post_event("usbRx", bytes[i]);
      }
    }
    break;
  case ATOMS3_FUNC_IMU:
    if (device->imu_ready) {
      m5_atoms3_imu_sample_t s;
      if (m5_atoms3_imu_read(&device->imu, &s) == ESP_OK) {
        const float values[6] = {s.ax, s.ay, s.az, s.gx, s.gy, s.gz};
        m5_pocket_bridge_set_array("imu", values, 6);
      }
    }
    break;
  default:
    break;
  }
}
