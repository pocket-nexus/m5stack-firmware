/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * Badge service, a C port of M5StopWatch-UserDemo (commit 81099c7)
 * main/hal/hal_badge.cpp (slot files and fallback rules),
 * main/hal/utils/config_ap/config_ap.cpp (editor access point and HTTP
 * routes) and main/hal/utils/wear_levelling/wear_levelling.c (FAT mount).
 * The captive DNS answer follows the DnsServer of 78/esp-wifi-connect 3.1.3
 * that config_ap.cpp uses. The editor page is
 * main/hal/utils/config_ap/assets/badge_config_ap.html, unchanged.
 */
#include "badge.h"

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"
#include "lwip/sockets.h"
#include "m5_pocket/bridge.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "stopwatch_badge";

#define BASE_PATH "/spiflash"
#define BADGE_DIR BASE_PATH "/badge"
#define ACTIVE_SLOT_FILE BADGE_DIR "/active_slot.txt"
#define SLOT_COUNT 6
#define AP_SSID_PREFIX "M5StopWatch"
#define AP_URL "http://192.168.4.1"
#define AP_MAX_CLIENTS 4
#define MAX_UPLOAD_BYTES (2 * 1024 * 1024)
#define JSON_TYPE "application/json"
#define DNS_PORT 53
#define DNS_TTL_S 28
#define MAX_IMAGE_SIDE 466
#define MAX_CANVAS_SIDE 512
#define PATH_MAX_LEN 96

extern const char editor_html_start[] asm("_binary_badge_config_ap_html_start");
extern const char editor_html_end[] asm("_binary_badge_config_ap_html_end");

static const char *const CAPTIVE_URLS[] = {
    "/hotspot-detect.html",       "/generate_204*", "/mobile/status.php",
    "/check_network_status.txt",  "/ncsi.txt",      "/fwlink/",
    "/connectivity-check.html",   "/success.txt",   "/portal.html",
    "/library/test/success.html",
};

/* Slot files, s_active and the FAT volume. */
static SemaphoreHandle_t s_lock;
/* Editor session start and stop. */
static SemaphoreHandle_t s_edit_lock;
static portMUX_TYPE s_dirty_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_dirty;
static int s_active = -1;
static wl_handle_t s_wl = WL_INVALID_HANDLE;
static char s_ssid[33];

static bool s_editing;
static bool s_wifi_ready;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_httpd;
/* Set before the DNS task starts, cleared by the task when it ends. */
static volatile bool s_dns_alive;
static volatile bool s_dns_run;
static volatile bool s_closing;

/* ---- Slot files (hal_badge.cpp) ---- */

static void slot_meta_path(char *out, int slot) {
  snprintf(out, PATH_MAX_LEN, "%s/slot_%u.meta", BADGE_DIR, (unsigned)slot);
}

static void slot_image_path(char *out, int slot, const char *extension) {
  snprintf(out, PATH_MAX_LEN, "%s/slot_%u.%s", BADGE_DIR, (unsigned)slot,
           extension);
}

static bool ensure_badge_dir(void) {
  if (mkdir(BADGE_DIR, 0775) == 0 || errno == EEXIST) {
    return true;
  }
  ESP_LOGE(TAG, "failed to create badge dir: %s, errno=%d", BADGE_DIR, errno);
  return false;
}

static bool write_file(const char *path, const void *data, size_t size) {
  FILE *file = fopen(path, "wb");
  if (file == NULL) {
    ESP_LOGE(TAG, "failed to open file for write: %s", path);
    return false;
  }
  const size_t written = size == 0 ? 0 : fwrite(data, 1, size, file);
  fclose(file);
  if (written != size) {
    ESP_LOGE(TAG, "failed to write file: %s", path);
    return false;
  }
  return true;
}

static bool write_text_file(const char *path, const char *text) {
  return write_file(path, text, strlen(text));
}

/* Reads at most 31 bytes and strips trailing newlines and spaces. */
static void read_text_file(const char *path, char out[32]) {
  out[0] = '\0';
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    return;
  }
  size_t size = fread(out, 1, 31, file);
  fclose(file);
  while (size > 0 && (out[size - 1] == '\n' || out[size - 1] == '\r' ||
                      out[size - 1] == ' ')) {
    --size;
  }
  out[size] = '\0';
}

/* Whole file in PSRAM, followed by `pad` zero bytes. */
static uint8_t *read_binary_file(const char *path, size_t *size, size_t pad) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }
  uint8_t *data = NULL;
  long length = -1;
  if (fseek(file, 0, SEEK_END) == 0) {
    length = ftell(file);
  }
  if (length >= 0) {
    rewind(file);
    data = heap_caps_calloc(1, (size_t)length + pad,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (data == NULL) {
      data = heap_caps_calloc(1, (size_t)length + pad, MALLOC_CAP_8BIT);
    }
  }
  if (data != NULL) {
    const size_t read = length == 0 ? 0 : fread(data, 1, (size_t)length, file);
    if (read != (size_t)length) {
      heap_caps_free(data);
      data = NULL;
    } else {
      *size = (size_t)length;
    }
  }
  fclose(file);
  return data;
}

/* normalize_extension: the file name's extension, else the content type. */
static const char *normalize_extension(const char *file_name,
                                       const char *content_type) {
  const char *dot = strrchr(file_name, '.');
  if (dot != NULL) {
    const char *ext = dot + 1;
    if (strcasecmp(ext, "png") == 0) {
      return "png";
    }
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
      return "jpg";
    }
  }
  char type[64];
  size_t i = 0;
  for (; content_type[i] != '\0' && i + 1 < sizeof(type); ++i) {
    type[i] = (char)tolower((unsigned char)content_type[i]);
  }
  type[i] = '\0';
  if (strstr(type, "png") != NULL) {
    return "png";
  }
  if (strstr(type, "jpeg") != NULL || strstr(type, "jpg") != NULL) {
    return "jpg";
  }
  return NULL;
}

/* The stored active slot; 0 when the file is missing or invalid. */
static int read_active_slot(void) {
  char text[32];
  read_text_file(ACTIVE_SLOT_FILE, text);
  if (text[0] == '\0') {
    return 0;
  }
  const unsigned long slot = strtoul(text, NULL, 10);
  if (slot >= SLOT_COUNT) {
    ESP_LOGW(TAG, "invalid active badge slot: %lu", slot);
    return 0;
  }
  return (int)slot;
}

/* "png", "jpg" or NULL for an empty slot. */
static const char *read_slot_extension(int slot) {
  char path[PATH_MAX_LEN];
  char ext[32];
  slot_meta_path(path, slot);
  read_text_file(path, ext);
  if (strcmp(ext, "png") == 0) {
    return "png";
  }
  if (strcmp(ext, "jpg") == 0) {
    return "jpg";
  }
  static const char *const candidates[] = {"png", "jpg", "jpeg"};
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    slot_image_path(path, slot, candidates[i]);
    if (access(path, R_OK) == 0) {
      return i == 0 ? "png" : "jpg";
    }
  }
  return NULL;
}

static bool is_slot_available(int slot) {
  return read_slot_extension(slot) != NULL;
}

/* load_badge_slot without the image: the slot's file is readable. */
static bool is_slot_loadable(int slot) {
  const char *ext = read_slot_extension(slot);
  if (ext == NULL) {
    return false;
  }
  char path[PATH_MAX_LEN];
  slot_image_path(path, slot, ext);
  return access(path, R_OK) == 0;
}

static bool persist_active_slot(int slot) {
  if (slot < 0 || slot >= SLOT_COUNT) {
    return false;
  }
  char text[4];
  snprintf(text, sizeof(text), "%d", slot);
  return write_text_file(ACTIVE_SLOT_FILE, text);
}

static bool clear_active_slot(void) {
  return unlink(ACTIVE_SLOT_FILE) == 0 || errno == ENOENT;
}

static int find_first_available_slot(void) {
  for (int slot = 0; slot < SLOT_COUNT; ++slot) {
    if (is_slot_available(slot)) {
      return slot;
    }
  }
  return -1;
}

/* Hal::loadBadgeImage: the stored slot, else the first occupied slot, which
 * is then stored when `persist` is set; -1 when no slot loads. */
static int resolve_active_locked(bool persist) {
  const int stored = read_active_slot();
  if (is_slot_loadable(stored)) {
    return stored;
  }
  const int fallback = find_first_available_slot();
  if (fallback >= 0 && is_slot_loadable(fallback)) {
    if (persist) {
      persist_active_slot(fallback);
    }
    return fallback;
  }
  return -1;
}

static void mark_dirty(void) {
  taskENTER_CRITICAL(&s_dirty_mux);
  s_dirty = true;
  taskEXIT_CRITICAL(&s_dirty_mux);
}

/* Republishes badgeSlots/badgeActive and marks the active image dirty. */
static void changed_locked(bool persist) {
  float slots[SLOT_COUNT];
  for (int slot = 0; slot < SLOT_COUNT; ++slot) {
    slots[slot] = is_slot_available(slot) ? 1.0f : 0.0f;
  }
  s_active = resolve_active_locked(persist);
  m5_pocket_bridge_set_array("badgeSlots", slots, SLOT_COUNT);
  m5_pocket_bridge_set_number("badgeActive", s_active);
  mark_dirty();
}

static void lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }

static void unlock(void) { xSemaphoreGive(s_lock); }

static bool set_active_locked(unsigned long slot, const char **message) {
  if (slot >= SLOT_COUNT) {
    *message = "invalid badge slot";
    return false;
  }
  if (!is_slot_available((int)slot)) {
    *message = "badge image not found";
    return false;
  }
  if (!persist_active_slot((int)slot)) {
    *message = "failed to update active slot";
    return false;
  }
  *message = "active slot updated";
  return true;
}

static bool delete_slot_locked(unsigned long slot, const char **message) {
  if (slot >= SLOT_COUNT) {
    *message = "invalid badge slot";
    return false;
  }
  static const char *const candidates[] = {"png", "jpg", "jpeg"};
  char path[PATH_MAX_LEN];
  bool deleted = false;
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    slot_image_path(path, (int)slot, candidates[i]);
    if (unlink(path) == 0) {
      deleted = true;
    }
  }
  slot_meta_path(path, (int)slot);
  if (unlink(path) == 0) {
    deleted = true;
  }
  if (!deleted) {
    *message = "badge image not found";
    return false;
  }
  if (read_active_slot() == (int)slot) {
    const int next = find_first_available_slot();
    if (next >= 0) {
      persist_active_slot(next);
    } else {
      clear_active_slot();
    }
  }
  *message = "badge image deleted";
  return true;
}

static bool save_upload_locked(unsigned long slot, const char *file_name,
                               const char *content_type, const uint8_t *data,
                               size_t size, const char **message) {
  if (slot >= SLOT_COUNT) {
    *message = "invalid badge slot";
    return false;
  }
  if (size == 0) {
    *message = "empty upload body";
    return false;
  }
  if (!ensure_badge_dir()) {
    *message = "failed to create badge directory";
    return false;
  }
  const char *extension = normalize_extension(file_name, content_type);
  if (extension == NULL || strcmp(extension, "jpg") != 0) {
    *message = "only jpg images are supported";
    return false;
  }

  char final_path[PATH_MAX_LEN];
  char temp_path[PATH_MAX_LEN + 4];
  slot_image_path(final_path, (int)slot, extension);
  snprintf(temp_path, sizeof(temp_path), "%s.tmp", final_path);
  if (!write_file(temp_path, data, size)) {
    *message = "failed to store image";
    return false;
  }
  if (unlink(final_path) != 0 && errno != ENOENT) {
    unlink(temp_path);
    *message = "failed to replace existing image";
    return false;
  }
  if (rename(temp_path, final_path) != 0) {
    unlink(temp_path);
    *message = "failed to finalize image";
    return false;
  }
  /* The factory removes the other extensions except "jpeg" for a jpg. */
  char old_path[PATH_MAX_LEN];
  slot_image_path(old_path, (int)slot, "png");
  unlink(old_path);

  char meta_path[PATH_MAX_LEN];
  slot_meta_path(meta_path, (int)slot);
  if (!write_text_file(meta_path, extension)) {
    *message = "failed to store image metadata";
    return false;
  }
  char active[4];
  snprintf(active, sizeof(active), "%u", (unsigned)slot);
  if (!write_text_file(ACTIVE_SLOT_FILE, active)) {
    *message = "failed to store active slot";
    return false;
  }
  ESP_LOGI(TAG, "badge image saved, slot=%lu, path=%s", slot, final_path);
  *message = "upload success";
  return true;
}

/* ---- Status text ---- */

static void log_message(const char *message) {
  ESP_LOGI(TAG, "%s", message);
  m5_pocket_bridge_set_text("apMessage", message);
}

/* ---- Captive DNS: every query gets the A record 192.168.4.1 ---- */

static void dns_task(void *arg) {
  (void)arg;
  const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    ESP_LOGE(TAG, "DNS socket");
    s_dns_alive = false;
    vTaskDelete(NULL);
    return;
  }
  const struct timeval timeout = {.tv_sec = 0, .tv_usec = 200000};
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  struct sockaddr_in bind_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(DNS_PORT),
      .sin_addr.s_addr = htonl(INADDR_ANY),
  };
  if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
    ESP_LOGE(TAG, "failed to bind port %d", DNS_PORT);
    close(sock);
    s_dns_alive = false;
    vTaskDelete(NULL);
    return;
  }
  uint8_t packet[512];
  while (s_dns_run) {
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    /* Room for the 16-byte answer after the question. */
    const int length = recvfrom(sock, packet, sizeof(packet) - 16, 0,
                                (struct sockaddr *)&from, &from_len);
    if (length < 12 || (packet[2] & 0x80) != 0) {
      continue; /* timeout, runt or a response */
    }
    if (((packet[4] << 8) | packet[5]) == 0) {
      continue; /* no question */
    }
    /* Keep the header and the first question. */
    int pos = 12;
    while (pos < length && packet[pos] != 0) {
      const int label = packet[pos];
      if (label > 63) {
        pos = length;
        break;
      }
      pos += label + 1;
    }
    pos += 1 + 4; /* terminator, QTYPE, QCLASS */
    if (pos > length) {
      continue;
    }
    packet[2] |= 0x80; /* response */
    packet[3] |= 0x80; /* recursion available */
    packet[4] = 0;     /* one question */
    packet[5] = 1;
    packet[6] = 0; /* one answer */
    packet[7] = 1;
    packet[8] = packet[9] = packet[10] = packet[11] = 0;
    // clang-format off
    const uint8_t answer[] = {
        0xC0, 0x0C,                  /* name: the question's */
        0x00, 0x01, 0x00, 0x01,      /* A, IN */
        0x00, 0x00, 0x00, DNS_TTL_S, /* TTL */
        0x00, 0x04, 192, 168, 4, 1,  /* 192.168.4.1 */
    };
    // clang-format on
    memcpy(packet + pos, answer, sizeof(answer));
    sendto(sock, packet, pos + sizeof(answer), 0, (struct sockaddr *)&from,
           from_len);
  }
  close(sock);
  s_dns_alive = false;
  vTaskDelete(NULL);
}

static void dns_start(void) {
  s_dns_run = true;
  s_dns_alive = true;
  if (xTaskCreate(dns_task, "badge_dns", 4096, NULL, 5, NULL) != pdPASS) {
    s_dns_run = false;
    s_dns_alive = false;
    ESP_LOGE(TAG, "DNS task");
  }
}

static void dns_stop(void) {
  s_dns_run = false;
  while (s_dns_alive) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/* ---- HTTP routes (config_ap.cpp Session) ---- */

static esp_err_t send_error(httpd_req_t *req, httpd_err_code_t code,
                            const char *message) {
  httpd_resp_send_err(req, code, message);
  return ESP_FAIL;
}

static esp_err_t send_json(httpd_req_t *req, const char *body) {
  httpd_resp_set_type(req, JSON_TYPE);
  return httpd_resp_send(req, body, (ssize_t)strlen(body));
}

static esp_err_t send_message(httpd_req_t *req, const char *message) {
  char body[160];
  snprintf(body, sizeof(body), "{\"status\":\"ok\",\"message\":\"%s\"}",
           message);
  return send_json(req, body);
}

static bool parse_slot(httpd_req_t *req, unsigned long *slot) {
  char query[64] = {0};
  if (httpd_req_get_url_query_len(req) == 0 ||
      httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    return false;
  }
  char value[16] = {0};
  if (httpd_query_key_value(query, "slot", value, sizeof(value)) != ESP_OK) {
    return false;
  }
  *slot = strtoul(value, NULL, 10);
  return true;
}

static esp_err_t handle_index(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  /* EMBED_TXTFILES appends a NUL that is not part of the page. */
  return httpd_resp_send(req, editor_html_start,
                         editor_html_end - editor_html_start - 1);
}

static esp_err_t handle_state(httpd_req_t *req) {
  char json[1024];
  lock();
  const int active = read_active_slot();
  bool has_image[SLOT_COUNT];
  for (int slot = 0; slot < SLOT_COUNT; ++slot) {
    has_image[slot] = is_slot_available(slot);
  }
  unlock();
  int used = snprintf(json, sizeof(json),
                      "{\"apSsid\":\"%s\",\"apUrl\":\"%s\",\"slotCount\":%d,"
                      "\"activeSlot\":%d,\"slots\":[",
                      s_ssid, AP_URL, SLOT_COUNT, active);
  for (int slot = 0; slot < SLOT_COUNT; ++slot) {
    used +=
        snprintf(json + used, sizeof(json) - (size_t)used,
                 "%s{\"slot\":%d,\"hasImage\":%s,\"isActive\":%s,"
                 "\"imageUrl\":\"/badge/image?slot=%d\"}",
                 slot == 0 ? "" : ",", slot, has_image[slot] ? "true" : "false",
                 slot == active ? "true" : "false", slot);
  }
  snprintf(json + used, sizeof(json) - (size_t)used, "]}");
  return send_json(req, json);
}

static esp_err_t handle_image(httpd_req_t *req) {
  unsigned long slot = 0;
  if (!parse_slot(req, &slot)) {
    return send_error(req, HTTPD_400_BAD_REQUEST, "missing slot");
  }
  if (slot >= SLOT_COUNT) {
    return send_error(req, HTTPD_404_NOT_FOUND, "invalid badge slot");
  }
  lock();
  const char *extension = read_slot_extension((int)slot);
  uint8_t *data = NULL;
  size_t size = 0;
  if (extension != NULL) {
    char path[PATH_MAX_LEN];
    slot_image_path(path, (int)slot, extension);
    data = read_binary_file(path, &size, 1);
  }
  unlock();
  if (extension == NULL) {
    return send_error(req, HTTPD_404_NOT_FOUND, "badge image not found");
  }
  if (data == NULL) {
    return send_error(req, HTTPD_404_NOT_FOUND, "failed to read image file");
  }
  httpd_resp_set_type(req, strcmp(extension, "png") == 0 ? "image/png"
                                                         : "image/jpeg");
  const esp_err_t err = httpd_resp_send(req, (const char *)data, (ssize_t)size);
  heap_caps_free(data);
  return err;
}

static esp_err_t handle_set_active(httpd_req_t *req) {
  unsigned long slot = 0;
  if (!parse_slot(req, &slot)) {
    return send_error(req, HTTPD_400_BAD_REQUEST, "missing slot");
  }
  const char *message = NULL;
  lock();
  const bool ok = set_active_locked(slot, &message);
  if (ok) {
    changed_locked(false);
  }
  unlock();
  if (!ok) {
    return send_error(req, HTTPD_400_BAD_REQUEST, message);
  }
  return send_message(req, message);
}

static esp_err_t handle_delete_image(httpd_req_t *req) {
  unsigned long slot = 0;
  if (!parse_slot(req, &slot)) {
    return send_error(req, HTTPD_400_BAD_REQUEST, "missing slot");
  }
  const char *message = NULL;
  lock();
  const bool ok = delete_slot_locked(slot, &message);
  if (ok) {
    changed_locked(false);
  }
  unlock();
  if (!ok) {
    return send_error(req, HTTPD_400_BAD_REQUEST, message);
  }
  return send_message(req, message);
}

static esp_err_t handle_captive_portal(httpd_req_t *req) {
  char location[64];
  snprintf(location, sizeof(location), "%s/?_=%lld", AP_URL,
           (long long)esp_timer_get_time());
  log_message("Received captive portal probe, redirecting to upload page");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", location);
  httpd_resp_set_hdr(req, "Connection", "close");
  return httpd_resp_send(req, NULL, 0);
}

static void stop_session(void);

static void close_task(void *arg) {
  (void)arg;
  /* Let the "closing" response leave before the server stops. */
  vTaskDelay(pdMS_TO_TICKS(200));
  stop_session();
  m5_pocket_bridge_post_event("badgeClosed", 1);
  s_closing = false;
  vTaskDelete(NULL);
}

static esp_err_t handle_close(httpd_req_t *req) {
  if (!s_closing) {
    s_closing = true;
    if (xTaskCreate(close_task, "badge_close", 4096, NULL, 5, NULL) != pdPASS) {
      s_closing = false;
      ESP_LOGE(TAG, "close task");
    }
  }
  return httpd_resp_sendstr(req, "closing");
}

static esp_err_t handle_upload(httpd_req_t *req) {
  if (req->content_len == 0 || req->content_len > MAX_UPLOAD_BYTES) {
    return send_error(req, HTTPD_400_BAD_REQUEST, "invalid upload size");
  }
  unsigned long slot = 0;
  if (!parse_slot(req, &slot)) {
    return send_error(req, HTTPD_400_BAD_REQUEST, "missing slot");
  }

  char *file_name = NULL;
  const size_t name_length = httpd_req_get_hdr_value_len(req, "X-File-Name");
  if (name_length > 0) {
    file_name = malloc(name_length + 1);
    if (file_name != NULL &&
        httpd_req_get_hdr_value_str(req, "X-File-Name", file_name,
                                    name_length + 1) != ESP_OK) {
      file_name[0] = '\0';
    }
  }
  char content_type[64] = {0};
  if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                  sizeof(content_type)) != ESP_OK) {
    content_type[0] = '\0';
  }

  const size_t size = req->content_len;
  uint8_t *data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (data == NULL) {
    free(file_name);
    return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                      "failed to read upload body");
  }
  size_t offset = 0;
  while (offset < size) {
    const int received =
        httpd_req_recv(req, (char *)data + offset, size - offset);
    if (received <= 0) {
      heap_caps_free(data);
      free(file_name);
      return send_error(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "failed to read upload body");
    }
    offset += (size_t)received;
  }

  const char *message = NULL;
  lock();
  const bool ok = save_upload_locked(
      slot, file_name != NULL && file_name[0] != '\0' ? file_name : "badge.jpg",
      content_type, data, size, &message);
  changed_locked(false);
  unlock();
  heap_caps_free(data);
  free(file_name);
  if (!ok) {
    return send_error(req, HTTPD_400_BAD_REQUEST, message);
  }
  return send_message(req, message);
}

/* ---- Editor session ---- */

static void make_ap_ssid(void) {
  uint8_t mac[6] = {0};
  const esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "read mac failed: %s", esp_err_to_name(err));
    snprintf(s_ssid, sizeof(s_ssid), "%s", AP_SSID_PREFIX);
    return;
  }
  snprintf(s_ssid, sizeof(s_ssid), "%s-%02X%02X", AP_SSID_PREFIX, mac[4],
           mac[5]);
}

static bool wifi_stack_ready(void) {
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "netif init failed: %s", esp_err_to_name(err));
    return false;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "event loop init failed: %s", esp_err_to_name(err));
    return false;
  }
  if (!s_wifi_ready) {
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.nvs_enable = false;
    err = esp_wifi_init(&config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "wifi init failed: %s", esp_err_to_name(err));
      return false;
    }
    s_wifi_ready = true;
  }
  return true;
}

static bool start_access_point(void) {
  if (s_ap_netif == NULL) {
    s_ap_netif = esp_netif_create_default_wifi_ap();
  }
  if (s_ap_netif == NULL) {
    log_message("Failed to create AP network interface");
    return false;
  }

  esp_netif_ip_info_t ip_info;
  esp_netif_set_ip4_addr(&ip_info.ip, 192, 168, 4, 1);
  esp_netif_set_ip4_addr(&ip_info.gw, 192, 168, 4, 1);
  esp_netif_set_ip4_addr(&ip_info.netmask, 255, 255, 255, 0);
  esp_netif_dhcps_stop(s_ap_netif);
  esp_netif_set_ip_info(s_ap_netif, &ip_info);
  /* DHCP offers the access point as the DNS server. */
  uint8_t offer_dns = 1;
  esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET,
                         ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns,
                         sizeof(offer_dns));
  esp_netif_dns_info_t dns = {0};
  dns.ip.type = ESP_IPADDR_TYPE_V4;
  dns.ip.u_addr.ip4 = ip_info.gw;
  esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns);
  esp_netif_dhcps_start(s_ap_netif);

  dns_start();

  wifi_config_t config = {
      .ap =
          {
              .channel = 1,
              .authmode = WIFI_AUTH_OPEN,
              .max_connection = AP_MAX_CLIENTS,
          },
  };
  strlcpy((char *)config.ap.ssid, s_ssid, sizeof(config.ap.ssid));
  config.ap.ssid_len = (uint8_t)strlen(s_ssid);

  esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED &&
      err != ESP_ERR_WIFI_MODE) {
    ESP_LOGW(TAG, "wifi stop before ap failed: %s", esp_err_to_name(err));
  }
  if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
    log_message("Failed to set AP mode");
    return false;
  }
  if (esp_wifi_set_config(WIFI_IF_AP, &config) != ESP_OK) {
    log_message("Failed to apply AP configuration");
    return false;
  }
  if (esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK) {
    log_message("Failed to disable Wi-Fi power save");
    return false;
  }
  if (esp_wifi_start() != ESP_OK) {
    log_message("Failed to start AP");
    return false;
  }
  return true;
}

static bool start_web_server(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 24;
  config.recv_wait_timeout = 15;
  config.send_wait_timeout = 15;
  config.stack_size = 6144;
  config.uri_match_fn = httpd_uri_match_wildcard;
  if (httpd_start(&s_httpd, &config) != ESP_OK) {
    s_httpd = NULL;
    log_message("Failed to start web server");
    return false;
  }

  static const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = handle_index},
      {.uri = "/badge/state", .method = HTTP_GET, .handler = handle_state},
      {.uri = "/badge/image", .method = HTTP_GET, .handler = handle_image},
      {.uri = "/badge/active",
       .method = HTTP_POST,
       .handler = handle_set_active},
      {.uri = "/badge/image",
       .method = HTTP_DELETE,
       .handler = handle_delete_image},
      {.uri = "/upload", .method = HTTP_POST, .handler = handle_upload},
      {.uri = "/close", .method = HTTP_POST, .handler = handle_close},
  };
  esp_err_t err = ESP_OK;
  for (size_t i = 0; err == ESP_OK && i < sizeof(routes) / sizeof(routes[0]);
       ++i) {
    err = httpd_register_uri_handler(s_httpd, &routes[i]);
  }
  for (size_t i = 0;
       err == ESP_OK && i < sizeof(CAPTIVE_URLS) / sizeof(CAPTIVE_URLS[0]);
       ++i) {
    const httpd_uri_t captive = {
        .uri = CAPTIVE_URLS[i],
        .method = HTTP_GET,
        .handler = handle_captive_portal,
    };
    err = httpd_register_uri_handler(s_httpd, &captive);
  }
  if (err != ESP_OK) {
    log_message("Failed to register web routes");
    return false;
  }
  return true;
}

/* Session::stop: HTTP server, DNS server, Wi-Fi, AP interface. */
static void teardown(void) {
  if (s_httpd != NULL) {
    httpd_stop(s_httpd);
    s_httpd = NULL;
  }
  dns_stop();
  if (s_wifi_ready) {
    const esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED &&
        err != ESP_ERR_WIFI_MODE) {
      ESP_LOGW(TAG, "wifi stop failed: %s", esp_err_to_name(err));
    }
  }
  if (s_ap_netif != NULL) {
    esp_netif_destroy_default_wifi(s_ap_netif);
    s_ap_netif = NULL;
  }
  if (s_wifi_ready) {
    esp_wifi_deinit();
    s_wifi_ready = false;
  }
  s_editing = false;
}

static void stop_session(void) {
  xSemaphoreTake(s_edit_lock, portMAX_DELAY);
  const bool was_editing = s_editing;
  teardown();
  if (was_editing) {
    ESP_LOGI(TAG, "Exited edit mode");
  }
  m5_pocket_bridge_set_text("apMessage", "");
  /* The factory reloads the badge after edit mode (Hal::loadBadgeImage). */
  lock();
  changed_locked(true);
  unlock();
  xSemaphoreGive(s_edit_lock);
}

static esp_err_t start_session(void) {
  xSemaphoreTake(s_edit_lock, portMAX_DELAY);
  if (s_editing) {
    xSemaphoreGive(s_edit_lock);
    return ESP_OK;
  }
  ESP_LOGI(TAG, "start badge edit mode via AP");
  lock();
  const bool dir_ok = ensure_badge_dir();
  unlock();
  if (!dir_ok) {
    log_message("Failed to create badge storage directory");
    xSemaphoreGive(s_edit_lock);
    return ESP_FAIL;
  }
  if (!wifi_stack_ready()) {
    log_message("Wi-Fi initialization failed");
    teardown();
    xSemaphoreGive(s_edit_lock);
    return ESP_FAIL;
  }
  if (!start_access_point() || !start_web_server()) {
    teardown();
    lock();
    changed_locked(true);
    unlock();
    xSemaphoreGive(s_edit_lock);
    return ESP_FAIL;
  }
  s_editing = true;
  char message[96];
  snprintf(message, sizeof(message), "Connect to Wi-Fi: %s\nAnd open page:\n%s",
           s_ssid, AP_URL);
  log_message(message);
  xSemaphoreGive(s_edit_lock);
  return ESP_OK;
}

/* ---- Public API ---- */

esp_err_t stopwatch_badge_init(void) {
  if (s_lock == NULL) {
    s_lock = xSemaphoreCreateMutex();
    s_edit_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL || s_edit_lock == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }
  make_ap_ssid();

  const esp_vfs_fat_mount_config_t mount_config = {
      .max_files = 4,
      .format_if_mount_failed = true,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
      .use_one_fat = false,
  };
  esp_err_t err = ESP_OK;
  if (s_wl == WL_INVALID_HANDLE) {
    err = esp_vfs_fat_spiflash_mount_rw_wl(BASE_PATH, "storage", &mount_config,
                                           &s_wl);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
      s_wl = WL_INVALID_HANDLE;
    }
  }

  lock();
  changed_locked(true);
  unlock();
  m5_pocket_bridge_set_text("apSsid", s_ssid);
  m5_pocket_bridge_set_text("apMessage", "");
  return err;
}

esp_err_t stopwatch_badge_select(int slot) {
  if (s_lock == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  if (slot < 0 || slot >= SLOT_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  lock();
  if (!is_slot_loadable(slot)) {
    unlock();
    return ESP_ERR_NOT_FOUND;
  }
  if (!persist_active_slot(slot)) {
    ESP_LOGW(TAG, "failed to store active slot %d", slot);
  }
  ESP_LOGI(TAG, "load badge image, slot=%d", slot);
  changed_locked(false);
  unlock();
  return ESP_OK;
}

esp_err_t stopwatch_badge_edit(bool on) {
  if (s_lock == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  if (on) {
    return start_session();
  }
  stop_session();
  return ESP_OK;
}

/* ---- Decode for the application ---- */

static void decode_jpeg(uint8_t *jpeg, size_t size,
                        stopwatch_badge_image *out) {
  esp_jpeg_image_cfg_t config = {
      .indata = jpeg,
      .indata_size = (uint32_t)size,
      .out_format = JPEG_IMAGE_FORMAT_RGB565,
      .out_scale = JPEG_IMAGE_SCALE_0,
  };
  esp_jpeg_image_output_t info = {0};
  if (esp_jpeg_get_image_info(&config, &info) != ESP_OK || info.width == 0 ||
      info.height == 0) {
    ESP_LOGW(TAG, "badge image is not a baseline JPEG");
    return;
  }
  unsigned shift = 0;
  while (shift < 3 && ((info.width >> shift) > MAX_IMAGE_SIDE ||
                       (info.height >> shift) > MAX_IMAGE_SIDE)) {
    ++shift;
  }
  const uint32_t width = info.width >> shift;
  const uint32_t height = info.height >> shift;
  if (width == 0 || height == 0 || width > MAX_CANVAS_SIDE ||
      height > MAX_CANVAS_SIDE) {
    ESP_LOGW(TAG, "badge image %ux%u is too large", info.width, info.height);
    return;
  }
  uint32_t canvas = 64;
  while (canvas < width || canvas < height) {
    canvas <<= 1;
  }

  /* One spare row and column: the decoder's last MCU may reach past the
   * scaled size. */
  const size_t decoded_size = (size_t)(width + 1) * (height + 2) * 2;
  uint16_t *decoded =
      heap_caps_malloc(decoded_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  uint16_t *pixels = heap_caps_calloc((size_t)canvas * canvas, 2,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (decoded == NULL || pixels == NULL) {
    ESP_LOGE(TAG, "no memory for the badge image");
    heap_caps_free(decoded);
    heap_caps_free(pixels);
    return;
  }
  config.outbuf = (uint8_t *)decoded;
  config.outbuf_size = (uint32_t)decoded_size;
  config.out_scale = (esp_jpeg_image_scale_t)shift;
  esp_jpeg_image_output_t result = {0};
  if (esp_jpeg_decode(&config, &result) != ESP_OK || result.width != width ||
      result.height != height) {
    ESP_LOGW(TAG, "badge image decode failed");
    heap_caps_free(decoded);
    heap_caps_free(pixels);
    return;
  }

  /* RGB565 (red high) to the PocketJS PSM_5650 order (red low). */
  const uint32_t left = (canvas - width) / 2;
  const uint32_t top = (canvas - height) / 2;
  for (uint32_t y = 0; y < height; ++y) {
    const uint16_t *src = decoded + (size_t)y * width;
    uint16_t *dst = pixels + (size_t)(top + y) * canvas + left;
    for (uint32_t x = 0; x < width; ++x) {
      const uint16_t p = src[x];
      dst[x] = (uint16_t)(((p & 0x001F) << 11) | (p & 0x07E0) | (p >> 11));
    }
  }
  heap_caps_free(decoded);

  out->pixels = pixels;
  out->canvas_w = canvas;
  out->canvas_h = canvas;
  out->image_w = width;
  out->image_h = height;
  ESP_LOGI(TAG, "badge image %ux%u, scale 1/%u, canvas %" PRIu32, info.width,
           info.height, 1u << shift, canvas);
}

bool stopwatch_badge_image_pending(void) {
  taskENTER_CRITICAL(&s_dirty_mux);
  const bool dirty = s_dirty;
  taskEXIT_CRITICAL(&s_dirty_mux);
  return dirty;
}

bool stopwatch_badge_take_image(stopwatch_badge_image *out) {
  if (out == NULL) {
    return false;
  }
  taskENTER_CRITICAL(&s_dirty_mux);
  const bool dirty = s_dirty;
  s_dirty = false;
  taskEXIT_CRITICAL(&s_dirty_mux);
  if (!dirty) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  if (s_lock == NULL) {
    return true;
  }

  uint8_t *jpeg = NULL;
  size_t size = 0;
  lock();
  const int slot = s_active;
  if (slot >= 0) {
    const char *extension = read_slot_extension(slot);
    if (extension != NULL && strcmp(extension, "jpg") == 0) {
      char path[PATH_MAX_LEN];
      slot_image_path(path, slot, extension);
      /* Zero padding: esp_jpeg_get_image_info reads a few bytes past the
       * last segment header it checks. */
      jpeg = read_binary_file(path, &size, 16);
    } else if (extension != NULL) {
      ESP_LOGW(TAG, "slot %d holds a %s image; only JPEG is decoded", slot,
               extension);
    }
  }
  unlock();
  if (jpeg != NULL) {
    decode_jpeg(jpeg, size, out);
    heap_caps_free(jpeg);
  }
  return true;
}
