// SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
// SPDX-License-Identifier: MIT
//
// ESP-IDF port of the Wi-Fi parts of uiflow-micropython
// m5stack/modules/startup/__init__.py (Startup) and sticks3.py (SetupApp,
// DNSServer). The portal page is files/system/sticks3/index.html.

#include "m5_sticks3_net.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "m5_sticks3_net";

#define NVS_NAMESPACE "uiflow"
#define PORTAL_URL "http://192.168.4.1/"
#define AP_MAX_CLIENTS 2
#define RETRY_US (5 * 1000 * 1000)
#define SCAN_MAX 20
#define DNS_TTL 300

extern const char portal_html_start[] asm("_binary_portal_html_start");
extern const char portal_html_end[] asm("_binary_portal_html_end");

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static m5_sticks3_net_state_t s_state;
static bool s_connect_wanted;
static esp_timer_handle_t s_retry;
static httpd_handle_t s_httpd;
static TaskHandle_t s_dns_task;
static volatile bool s_dns_run;

static void set_status(m5_sticks3_net_status_t status) {
  taskENTER_CRITICAL(&s_lock);
  s_state.status = status;
  taskEXIT_CRITICAL(&s_lock);
}

static m5_sticks3_net_status_t get_status(void) {
  taskENTER_CRITICAL(&s_lock);
  const m5_sticks3_net_status_t status = s_state.status;
  taskEXIT_CRITICAL(&s_lock);
  return status;
}

static void count(uint32_t *field, int delta) {
  taskENTER_CRITICAL(&s_lock);
  if (delta < 0 && *field == 0) {
    // An AP station leaving before it was counted.
  } else {
    *field = (uint32_t)((int)*field + delta);
  }
  taskEXIT_CRITICAL(&s_lock);
}

static void read_credentials(char *ssid, size_t ssid_size, char *password,
                             size_t password_size) {
  ssid[0] = '\0';
  password[0] = '\0';
  nvs_handle_t nvs;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
    return;
  }
  if (nvs_get_str(nvs, "ssid0", ssid, &ssid_size) != ESP_OK) {
    ssid[0] = '\0';
  }
  if (nvs_get_str(nvs, "pswd0", password, &password_size) != ESP_OK) {
    password[0] = '\0';
  }
  nvs_close(nvs);
}

static esp_err_t save_credentials(const char *ssid, const char *password) {
  nvs_handle_t nvs;
  ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG,
                      "NVS open");
  esp_err_t err = nvs_set_str(nvs, "ssid0", ssid);
  if (err == ESP_OK) {
    err = nvs_set_str(nvs, "pswd0", password);
  }
  if (err == ESP_OK) {
    err = nvs_commit(nvs);
  }
  nvs_close(nvs);
  return err;
}

/// Startup.connect_network: connect when an SSID is given.
static void connect_station(const char *ssid, const char *password) {
  taskENTER_CRITICAL(&s_lock);
  s_state.configured = ssid[0] != '\0';
  taskEXIT_CRITICAL(&s_lock);
  if (ssid[0] == '\0') {
    return;
  }
  wifi_config_t config = {0};
  strlcpy((char *)config.sta.ssid, ssid, sizeof(config.sta.ssid));
  strlcpy((char *)config.sta.password, password, sizeof(config.sta.password));
  if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK) {
    set_status(M5_STICKS3_NET_FAILED);
    return;
  }
  s_connect_wanted = true;
  set_status(M5_STICKS3_NET_CONNECTING);
  if (esp_wifi_connect() != ESP_OK) {
    set_status(M5_STICKS3_NET_FAILED);
  }
}

static void connect_saved(void) {
  char ssid[33], password[65];
  read_credentials(ssid, sizeof(ssid), password, sizeof(password));
  connect_station(ssid, password);
}

static void retry(void *arg) {
  (void)arg;
  if (s_connect_wanted) {
    set_status(M5_STICKS3_NET_CONNECTING);
    esp_wifi_connect();
  }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
  (void)arg;
  (void)data;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_connect_wanted) {
      set_status(M5_STICKS3_NET_FAILED);
      esp_timer_stop(s_retry);
      esp_timer_start_once(s_retry, RETRY_US);
    } else {
      set_status(M5_STICKS3_NET_IDLE);
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    set_status(M5_STICKS3_NET_GOT_IP);
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
    count(&s_state.ap_stations, 1);
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
    count(&s_state.ap_stations, -1);
  }
}

esp_err_t m5_sticks3_net_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase");
    err = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(err, TAG, "NVS");
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
  const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "Wi-Fi init");
  ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL),
      TAG, "Wi-Fi events");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL),
      TAG, "IP events");
  const esp_timer_create_args_t timer = {.callback = retry,
                                         .name = "wifi_retry"};
  ESP_RETURN_ON_ERROR(esp_timer_create(&timer, &s_retry), TAG, "timer");

  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  snprintf(s_state.ap_ssid, sizeof(s_state.ap_ssid), "S3_%02X%02X", mac[4],
           mac[5]);

  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
  connect_saved();
  return ESP_OK;
}

// ---- Captive DNS (sticks3.py DNSServer) ----

static void dns_task(void *arg) {
  (void)arg;
  const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    ESP_LOGE(TAG, "DNS socket");
    s_dns_task = NULL;
    vTaskDelete(NULL);
    return;
  }
  const int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  const struct timeval timeout = {.tv_sec = 0, .tv_usec = 200000};
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  struct sockaddr_in bind_addr = {.sin_family = AF_INET,
                                  .sin_port = htons(53),
                                  .sin_addr.s_addr = htonl(INADDR_ANY)};
  if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
    ESP_LOGE(TAG, "DNS bind");
    close(sock);
    s_dns_task = NULL;
    vTaskDelete(NULL);
    return;
  }
  static uint8_t packet[512];
  while (s_dns_run) {
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    const int length = recvfrom(sock, packet, sizeof(packet) - 16, 0,
                                (struct sockaddr *)&from, &from_len);
    if (length < 12) {
      continue;
    }
    const unsigned flags = ((unsigned)packet[2] << 8) | packet[3];
    const unsigned questions = ((unsigned)packet[4] << 8) | packet[5];
    if ((flags & 0x8000) || questions != 1) {
      continue;
    }
    int pos = 12;
    while (pos < length && packet[pos] != 0) {
      const int label = packet[pos];
      if (label > 63 || pos + label + 1 > length) {
        pos = length;
        break;
      }
      pos += label + 1;
    }
    if (pos + 5 > length) {
      continue;
    }
    pos += 1; // terminator
    const unsigned qtype = ((unsigned)packet[pos] << 8) | packet[pos + 1];
    const unsigned qclass = ((unsigned)packet[pos + 2] << 8) | packet[pos + 3];
    if (qtype != 1 || qclass != 1) {
      continue;
    }
    int out = pos + 4; // keep header and question
    packet[2] = 0x81;
    packet[3] = 0x80;
    packet[6] = 0x00;
    packet[7] = 0x01;
    packet[8] = packet[9] = packet[10] = packet[11] = 0;
    const uint8_t answer[] = {0xC0,
                              0x0C,
                              0x00,
                              0x01,
                              0x00,
                              0x01,
                              0x00,
                              0x00,
                              (DNS_TTL >> 8) & 0xFF,
                              DNS_TTL & 0xFF,
                              0x00,
                              0x04,
                              192,
                              168,
                              4,
                              1};
    memcpy(packet + out, answer, sizeof(answer));
    out += sizeof(answer);
    sendto(sock, packet, out, 0, (struct sockaddr *)&from, from_len);
  }
  close(sock);
  s_dns_task = NULL;
  vTaskDelete(NULL);
}

// ---- Portal (sticks3.py SetupApp.register_routes) ----

static esp_err_t redirect(httpd_req_t *req, const char *location) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", location);
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_index(httpd_req_t *req) {
  count(&s_state.portal_clients, 1);
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, portal_html_start,
                         portal_html_end - portal_html_start);
}

static esp_err_t handle_other(httpd_req_t *req) {
  // Captive-portal probes and every unknown path go to the portal page.
  count(&s_state.portal_clients, 1);
  return redirect(req, PORTAL_URL);
}

static void json_string(char *out, size_t size, size_t *used,
                        const char *text) {
  if (*used + 2 >= size) {
    return;
  }
  out[(*used)++] = '"';
  for (const unsigned char *c = (const unsigned char *)text;
       *c && *used + 8 < size; ++c) {
    if (*c == '"' || *c == '\\') {
      out[(*used)++] = '\\';
      out[(*used)++] = (char)*c;
    } else if (*c < 0x20) {
      *used += (size_t)snprintf(out + *used, size - *used, "\\u%04x", *c);
    } else {
      out[(*used)++] = (char)*c;
    }
  }
  out[(*used)++] = '"';
  out[*used] = '\0';
}

static esp_err_t handle_config(httpd_req_t *req) {
  static wifi_ap_record_t records[SCAN_MAX];
  uint16_t found = SCAN_MAX;
  if (esp_wifi_scan_start(NULL, true) != ESP_OK ||
      esp_wifi_scan_get_ap_records(&found, records) != ESP_OK) {
    found = 0;
  }
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  const size_t size = 256 + (size_t)found * 96;
  char *json = malloc(size);
  if (!json) {
    return httpd_resp_send_500(req);
  }
  size_t used = (size_t)snprintf(json, size, "{\"name\":");
  json_string(json, size, &used, s_state.ap_ssid);
  used += (size_t)snprintf(json + used, size - used,
                           ",\"mac\":\"%02X%02X%02X%02X%02X%02X\",\"aps\":[{}",
                           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  for (uint16_t i = 0; i < found && used + 96 < size; ++i) {
    used += (size_t)snprintf(json + used, size - used, ",{\"ssid\":");
    json_string(json, size, &used, (const char *)records[i].ssid);
    used += (size_t)snprintf(json + used, size - used,
                             ",\"rssi\":%d,\"lock\":%d}", records[i].rssi,
                             records[i].authmode != WIFI_AUTH_OPEN);
  }
  used += (size_t)snprintf(json + used, size - used, "]}");
  httpd_resp_set_type(req, "application/json");
  const esp_err_t err = httpd_resp_send(req, json, (ssize_t)used);
  free(json);
  return err;
}

static void url_decode(char *text) {
  char *out = text;
  for (const char *in = text; *in; ++in) {
    if (*in == '+') {
      *out++ = ' ';
    } else if (*in == '%' && in[1] && in[2]) {
      char hex[3] = {in[1], in[2], '\0'};
      *out++ = (char)strtol(hex, NULL, 16);
      in += 2;
    } else {
      *out++ = *in;
    }
  }
  *out = '\0';
}

static esp_err_t handle_wifisave(httpd_req_t *req) {
  char query[256] = {0};
  char ssid[33 * 3] = {0}, password[65 * 3] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    httpd_query_key_value(query, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(query, "psk", password, sizeof(password));
  }
  url_decode(ssid);
  url_decode(password);
  ssid[32] = '\0';
  password[64] = '\0';
  ESP_LOGI(TAG, "saving Wi-Fi config for \"%s\"", ssid);
  if (save_credentials(ssid, password) != ESP_OK) {
    ESP_LOGW(TAG, "cannot save Wi-Fi config");
  }
  connect_station(ssid, password);
  return redirect(req, "/?save");
}

esp_err_t m5_sticks3_net_setup_start(void) {
  if (s_httpd) {
    return ESP_OK;
  }
  // SetupApp.on_launch: disconnect the station, open AP "S3_XXXX".
  s_connect_wanted = false;
  esp_timer_stop(s_retry);
  set_status(M5_STICKS3_NET_IDLE);
  esp_wifi_disconnect();
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "AP mode");
  wifi_config_t ap = {
      .ap =
          {
              .channel = 1,
              .authmode = WIFI_AUTH_OPEN,
              .max_connection = AP_MAX_CLIENTS,
          },
  };
  strlcpy((char *)ap.ap.ssid, s_state.ap_ssid, sizeof(ap.ap.ssid));
  ap.ap.ssid_len = (uint8_t)strlen(s_state.ap_ssid);
  ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap), TAG, "AP config");
  taskENTER_CRITICAL(&s_lock);
  s_state.ap_stations = 0;
  s_state.portal_clients = 0;
  s_state.setup_active = true;
  taskEXIT_CRITICAL(&s_lock);

  s_dns_run = true;
  if (xTaskCreate(dns_task, "captive_dns", 3072, NULL, 4, &s_dns_task) !=
      pdPASS) {
    s_dns_run = false;
    ESP_LOGE(TAG, "DNS task");
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.lru_purge_enable = true;
  ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &config), TAG, "HTTP server");
  static const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = handle_index},
      {.uri = "/config.json", .method = HTTP_GET, .handler = handle_config},
      {.uri = "/wifisave", .method = HTTP_GET, .handler = handle_wifisave},
      {.uri = "/*", .method = HTTP_GET, .handler = handle_other},
  };
  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
    httpd_register_uri_handler(s_httpd, &routes[i]);
  }
  ESP_LOGI(TAG, "setup access point %s", s_state.ap_ssid);
  return ESP_OK;
}

esp_err_t m5_sticks3_net_setup_stop(void) {
  if (s_httpd) {
    httpd_stop(s_httpd);
    s_httpd = NULL;
  }
  s_dns_run = false;
  while (s_dns_task) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  taskENTER_CRITICAL(&s_lock);
  s_state.setup_active = false;
  s_state.ap_stations = 0;
  taskEXIT_CRITICAL(&s_lock);
  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "STA mode");
  // SetupApp.on_exit: reconnect the saved network when the station is idle.
  if (get_status() == M5_STICKS3_NET_IDLE) {
    connect_saved();
  }
  return ESP_OK;
}

void m5_sticks3_net_get_state(m5_sticks3_net_state_t *out) {
  taskENTER_CRITICAL(&s_lock);
  *out = s_state;
  taskEXIT_CRITICAL(&s_lock);
}
