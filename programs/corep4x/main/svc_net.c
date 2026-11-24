/* Ethernet, PING and the TIMER window's SNTP probe (svc.h), after
 * m5_corep4_tool::ethernet_init and ethernet_event_handler,
 * ui_function_ping_window.cpp and ui_function_timer_window.cpp of
 * M5CoreP4X-UserDemo.
 *
 * Ethernet: the "net" task brings the IP101 up at boot (DHCP client netif,
 * as the factory's app_main) and checks the L3B rail every 500 ms: when
 * the rail is off the driver is removed and the PHY pins are released;
 * when it is on again the PHY is reset and the driver installed again.
 * Published: ethState (0 disconnected, 1 link up, 2 got an address),
 * ethIp, ethGateway.
 *
 * PING: "ping <mask>" pings the checked targets one after the other on the
 * "func_ping" task (4 echoes of 32 bytes, 500 ms apart, 1000 ms timeout)
 * and publishes pingRunning, pingRows, pingSummary and pingSummaryTone. */
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "m5_corep4x/bsp.h"
#include "m5_corep4x/ethernet.h"
#include "net_ping.h"
#include "net_sntp.h"
#include "ping/ping_sock.h"
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "net_svc";

/* NET_REFRESH_MS: the L3B check. */
#define POLL_MS 500
/* main.cpp: the wait for the L3B rail before the PHY reset. */
#define L3B_SETTLE_MS 200
#define GOT_IP_BIT BIT0
#define NTP_SERVER "ntp.aliyun.com"
#define NTP_WAIT_ETH_MS 4000
#define NTP_SYNC_TIMEOUT_MS 6000
/* 2020-01-01 00:00:00 UTC */
#define CLOCK_VALID_FROM 1577836800

/* ---- Ethernet ------------------------------------------------------------
 */

/* Held by the net task while it installs or removes the driver, and by
 * net_svc_sleep. */
static SemaphoreHandle_t s_eth_lock;
static esp_eth_handle_t s_eth;
static esp_netif_t *s_netif;
static esp_eth_netif_glue_handle_t s_glue;
/* The PHY did not answer since the L3B rail last came on. */
static bool s_eth_failed;
static bool s_pins_released;
static volatile bool s_sleeping;
/* The rail is on and the PHY has not failed: Ethernet is up or coming up
 * (the factory's ethernet_initialized, set at boot). */
static volatile bool s_eth_expected;

/* Link state, written by the event handlers. */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static EventGroupHandle_t s_events;
static bool s_link;
static bool s_got_ip;
static char s_ip[16];
static char s_gateway[16];

static void publish_eth(void) {
  char ip[sizeof(s_ip)];
  char gateway[sizeof(s_gateway)];
  int state;
  taskENTER_CRITICAL(&s_mux);
  memcpy(ip, s_ip, sizeof(ip));
  memcpy(gateway, s_gateway, sizeof(gateway));
  state = s_got_ip ? 2 : s_link ? 1 : 0;
  taskEXIT_CRITICAL(&s_mux);
  m5_pocket_bridge_set_text("ethIp", ip);
  m5_pocket_bridge_set_text("ethGateway", gateway);
  m5_pocket_bridge_set_number("ethState", state);
}

/* Link down or driver stopped: link, address and gateway cleared. */
static void clear_link(void) {
  taskENTER_CRITICAL(&s_mux);
  s_link = false;
  s_got_ip = false;
  s_ip[0] = '\0';
  s_gateway[0] = '\0';
  taskEXIT_CRITICAL(&s_mux);
  xEventGroupClearBits(s_events, GOT_IP_BIT);
}

static void on_eth_event(void *arg, esp_event_base_t base, int32_t id,
                         void *data) {
  (void)arg;
  (void)base;
  if (!data || *(esp_eth_handle_t *)data != s_eth) {
    return;
  }
  switch (id) {
  case ETHERNET_EVENT_CONNECTED:
    ESP_LOGI(TAG, "Ethernet link up");
    taskENTER_CRITICAL(&s_mux);
    s_link = true;
    taskEXIT_CRITICAL(&s_mux);
    break;
  case ETHERNET_EVENT_DISCONNECTED:
  case ETHERNET_EVENT_STOP:
    ESP_LOGI(TAG, "Ethernet link down");
    clear_link();
    break;
  default:
    return;
  }
  publish_eth();
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id,
                        void *data) {
  (void)arg;
  (void)base;
  const ip_event_got_ip_t *event = data;
  if (!event || !s_netif || event->esp_netif != s_netif) {
    return;
  }
  if (id == IP_EVENT_ETH_GOT_IP) {
    char ip[sizeof(s_ip)];
    char gateway[sizeof(s_gateway)];
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
    snprintf(gateway, sizeof(gateway), IPSTR, IP2STR(&event->ip_info.gw));
    ESP_LOGI(TAG, "Ethernet IP %s, gateway %s", ip, gateway);
    taskENTER_CRITICAL(&s_mux);
    memcpy(s_ip, ip, sizeof(s_ip));
    memcpy(s_gateway, gateway, sizeof(s_gateway));
    s_got_ip = true;
    taskEXIT_CRITICAL(&s_mux);
    xEventGroupSetBits(s_events, GOT_IP_BIT);
  } else if (id == IP_EVENT_ETH_LOST_IP) {
    taskENTER_CRITICAL(&s_mux);
    s_got_ip = false;
    s_ip[0] = '\0';
    s_gateway[0] = '\0';
    taskEXIT_CRITICAL(&s_mux);
    xEventGroupClearBits(s_events, GOT_IP_BIT);
  } else {
    return;
  }
  publish_eth();
}

/* Stop, netif glue, netif, driver: the teardown order of ESP-IDF's
 * protocol_examples_common eth_stop. Under s_eth_lock. */
static void eth_down(void) {
  if (!s_eth) {
    return;
  }
  esp_eth_handle_t eth = s_eth;
  esp_err_t err = esp_eth_stop(eth);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Ethernet stop: %s", esp_err_to_name(err));
  }
  if (s_glue) {
    esp_eth_del_netif_glue(s_glue);
    s_glue = NULL;
  }
  if (s_netif) {
    esp_netif_destroy(s_netif);
    s_netif = NULL;
  }
  s_eth = NULL;
  err = m5_corep4x_eth_del(eth);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Ethernet driver removal: %s", esp_err_to_name(err));
  }
  clear_link();
  publish_eth();
  ESP_LOGI(TAG, "Ethernet stopped");
}

/* ethernet_init(true): netif ESP_NETIF_DEFAULT_ETH (DHCP client), the
 * driver attached and started. Under s_eth_lock. */
static esp_err_t eth_up(void) {
  esp_eth_handle_t eth = NULL;
  ESP_RETURN_ON_ERROR(m5_corep4x_eth_new(&eth), TAG, "Ethernet driver");
  const esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *netif = esp_netif_new(&netif_config);
  esp_eth_netif_glue_handle_t glue = netif ? esp_eth_new_netif_glue(eth) : NULL;
  esp_err_t err = glue ? esp_netif_attach(netif, glue) : ESP_ERR_NO_MEM;
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet netif: %s", esp_err_to_name(err));
    if (glue) {
      esp_eth_del_netif_glue(glue);
    }
    if (netif) {
      esp_netif_destroy(netif);
    }
    m5_corep4x_eth_del(eth);
    return err;
  }
  s_eth = eth;
  s_netif = netif;
  s_glue = glue;
  /* Autonegotiation: up to 4 s without a cable. */
  err = esp_eth_start(eth);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet start: %s", esp_err_to_name(err));
    eth_down();
    return err;
  }
  ESP_LOGI(TAG, "Ethernet started");
  return ESP_OK;
}

static void net_task(void *arg) {
  (void)arg;
  bool was_on = false;
  bool l3b = true;
  for (;;) {
    if (s_sleeping) {
      /* net_svc_sleep removed the driver; the I2C bus is left to the
       * sleep sequence. */
      vTaskDelay(pdMS_TO_TICKS(POLL_MS));
      continue;
    }
    bool on = l3b;
    if (m5_corep4x_get_switch(M5_COREP4X_SWITCH_L3B, &on) == ESP_OK) {
      l3b = on;
    }
    xSemaphoreTake(s_eth_lock, portMAX_DELAY);
    if (!s_sleeping) {
      if (l3b && !s_eth && !s_eth_failed) {
        s_eth_expected = true;
        if (!was_on) {
          vTaskDelay(pdMS_TO_TICKS(L3B_SETTLE_MS));
        }
        s_pins_released = false;
        s_eth_failed = eth_up() != ESP_OK;
        s_eth_expected = !s_eth_failed;
      } else if (!l3b) {
        s_eth_expected = false;
        if (s_eth) {
          ESP_LOGI(TAG, "L3B off: Ethernet PHY unpowered");
          eth_down();
        }
        if (!s_pins_released) {
          m5_corep4x_eth_release_pins();
          s_pins_released = true;
        }
        s_eth_failed = false;
      }
    }
    xSemaphoreGive(s_eth_lock);
    was_on = l3b;
    vTaskDelay(pdMS_TO_TICKS(POLL_MS));
  }
}

/* ---- PING ----------------------------------------------------------------
 */

/* Guards the rows and the session handle. */
static SemaphoreHandle_t s_ping_lock;
static net_ping_row_t s_rows[NET_PING_TARGETS];
static esp_ping_handle_t s_session;
static volatile bool s_ping_running;
static volatile bool s_ping_abort;

/* The running test: checked targets, the gateway host, the current
 * session's replies. */
static struct {
  unsigned mask;
  char gateway[16];
  int row;
  uint32_t received;
  uint64_t rtt_sum_ms;
  SemaphoreHandle_t done;
} s_job;

static void publish_rows_locked(void) {
  char text[NET_PING_TARGETS * (NET_PING_TEXT_MAX + 3)];
  net_ping_rows_format(s_rows, NET_PING_TARGETS, text, sizeof(text));
  m5_pocket_bridge_set_text("pingRows", text);
}

static void set_row(int row, uint8_t dot, uint8_t tone, const char *text) {
  xSemaphoreTake(s_ping_lock, portMAX_DELAY);
  net_ping_row_set(&s_rows[row], dot, tone, text);
  publish_rows_locked();
  xSemaphoreGive(s_ping_lock);
}

/* A new result text in the row's current tones. */
static void set_row_text(int row, const char *text) {
  xSemaphoreTake(s_ping_lock, portMAX_DELAY);
  net_ping_row_set(&s_rows[row], s_rows[row].dot, s_rows[row].tone, text);
  publish_rows_locked();
  xSemaphoreGive(s_ping_lock);
}

static void clear_rows(void) {
  xSemaphoreTake(s_ping_lock, portMAX_DELAY);
  for (int i = 0; i < NET_PING_TARGETS; i++) {
    net_ping_row_set(&s_rows[i], SVC_MUTED, SVC_MUTED, "--");
  }
  publish_rows_locked();
  xSemaphoreGive(s_ping_lock);
}

static void publish_summary(const char *text, uint8_t tone) {
  m5_pocket_bridge_set_number("pingSummaryTone", tone);
  m5_pocket_bridge_set_text("pingSummary", text);
}

static void publish_running(bool running) {
  s_ping_running = running;
  m5_pocket_bridge_set_bool("pingRunning", running);
}

static void on_ping_reply(esp_ping_handle_t session, void *arg) {
  (void)arg;
  uint32_t elapsed_ms = 0;
  uint16_t seqno = 0;
  esp_ping_get_profile(session, ESP_PING_PROF_TIMEGAP, &elapsed_ms,
                       sizeof(elapsed_ms));
  esp_ping_get_profile(session, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  s_job.received++;
  s_job.rtt_sum_ms += elapsed_ms;
  char text[NET_PING_TEXT_MAX];
  net_ping_progress_text(true, seqno, NET_PING_COUNT, text, sizeof(text));
  set_row_text(s_job.row, text);
}

static void on_ping_timeout(esp_ping_handle_t session, void *arg) {
  (void)arg;
  uint16_t seqno = 0;
  esp_ping_get_profile(session, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  char text[NET_PING_TEXT_MAX];
  net_ping_progress_text(false, seqno, NET_PING_COUNT, text, sizeof(text));
  set_row_text(s_job.row, text);
}

static void on_ping_end(esp_ping_handle_t session, void *arg) {
  (void)arg;
  uint32_t received = 0;
  if (esp_ping_get_profile(session, ESP_PING_PROF_REPLY, &received,
                           sizeof(received)) == ESP_OK) {
    s_job.received = received;
  }
  xSemaphoreGive(s_job.done);
}

/* resolve_ping_target: a literal IPv4 address, else getaddrinfo (AF_INET).
 */
static bool resolve(const char *host, ip_addr_t *out, char *error,
                    size_t size) {
  memset(out, 0, sizeof(*out));
  struct in_addr direct;
  if (inet_pton(AF_INET, host, &direct) == 1) {
    ip_addr_set_ip4_u32_val(*out, direct.s_addr);
    return true;
  }
  const struct addrinfo hints = {.ai_family = AF_INET,
                                 .ai_socktype = SOCK_STREAM};
  struct addrinfo *res = NULL;
  const int gai = getaddrinfo(host, NULL, &hints, &res);
  if (gai != 0 || !res) {
    snprintf(error, size, "DNS failed: %d", gai);
    if (res) {
      freeaddrinfo(res);
    }
    return false;
  }
  const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)res->ai_addr;
  ip_addr_set_ip4_u32_val(*out, ipv4->sin_addr.s_addr);
  freeaddrinfo(res);
  return true;
}

/* run_ping_once: true when the session ended without an error and with at
 * least one reply; the mean round trip in *mean_ms, else the error text. */
static bool ping_once(int row, const char *host, uint32_t *mean_ms, char *error,
                      size_t size) {
  error[0] = '\0';
  *mean_ms = 0;
  s_job.row = row;
  s_job.received = 0;
  s_job.rtt_sum_ms = 0;
  ip_addr_t target;
  if (!resolve(host, &target, error, size)) {
    return false;
  }
  if (!s_job.done) {
    s_job.done = xSemaphoreCreateBinary();
    if (!s_job.done) {
      snprintf(error, size, "semaphore alloc failed");
      return false;
    }
  }
  xSemaphoreTake(s_job.done, 0);

  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.target_addr = target;
  config.count = NET_PING_COUNT;
  config.timeout_ms = NET_PING_TIMEOUT_MS;
  config.interval_ms = NET_PING_INTERVAL_MS;
  config.data_size = NET_PING_DATA_SIZE;
  /* The callbacks format and publish the row. */
  config.task_stack_size = 4096;
  const esp_ping_callbacks_t callbacks = {
      .cb_args = NULL,
      .on_ping_success = on_ping_reply,
      .on_ping_timeout = on_ping_timeout,
      .on_ping_end = on_ping_end,
  };
  esp_ping_handle_t session = NULL;
  esp_err_t err = esp_ping_new_session(&config, &callbacks, &session);
  if (err != ESP_OK) {
    snprintf(error, size, "new session failed: %s", esp_err_to_name(err));
    return false;
  }
  xSemaphoreTake(s_ping_lock, portMAX_DELAY);
  s_session = session;
  xSemaphoreGive(s_ping_lock);
  err = s_ping_abort ? ESP_ERR_INVALID_STATE : esp_ping_start(session);
  if (err == ESP_OK) {
    if (xSemaphoreTake(s_job.done, pdMS_TO_TICKS(NET_PING_SESSION_MS)) !=
        pdTRUE) {
      err = ESP_ERR_TIMEOUT;
      snprintf(error, size, "ping session timeout");
      esp_ping_stop(session);
      xSemaphoreTake(s_job.done, pdMS_TO_TICKS(1000));
    }
  } else {
    snprintf(error, size, "start failed: %s", esp_err_to_name(err));
  }
  xSemaphoreTake(s_ping_lock, portMAX_DELAY);
  s_session = NULL;
  xSemaphoreGive(s_ping_lock);
  esp_ping_delete_session(session);
  if (s_job.received > 0) {
    *mean_ms = (uint32_t)(s_job.rtt_sum_ms / s_job.received);
  }
  return err == ESP_OK && s_job.received > 0;
}

static void ping_task(void *arg) {
  (void)arg;
  uint32_t ok = 0;
  uint32_t total = 0;
  for (int i = 0; i < NET_PING_TARGETS; i++) {
    total += (s_job.mask >> i) & 1u;
  }
  char summary[96];
  for (int i = 0; i < NET_PING_TARGETS && !s_ping_abort; i++) {
    if (!(s_job.mask & (1u << i))) {
      continue;
    }
    set_row(i, SVC_WARNING, SVC_WARNING, "Testing...");
    snprintf(summary, sizeof(summary), "Testing: %s...", net_ping_names[i]);
    publish_summary(summary, SVC_WARNING);
    const char *host = i == 3 ? s_job.gateway : net_ping_hosts[i];
    uint32_t mean_ms = 0;
    char error[NET_PING_TEXT_MAX];
    const bool success = ping_once(i, host, &mean_ms, error, sizeof(error));
    ok += success ? 1 : 0;
    char text[NET_PING_TEXT_MAX];
    net_ping_result_text(success, mean_ms, error, text, sizeof(text));
    const uint8_t tone = success ? SVC_SUCCESS : SVC_ERROR;
    set_row(i, tone, tone, text);
  }
  const uint8_t tone =
      net_ping_summary_text(ok, total, summary, sizeof(summary));
  publish_summary(summary, tone);
  publish_running(false);
  vTaskDelete(NULL);
}

/* start_ping */
static void start_ping(unsigned mask) {
  if (s_ping_running) {
    return;
  }
  bool got_ip;
  char gateway[sizeof(s_gateway)];
  taskENTER_CRITICAL(&s_mux);
  got_ip = s_got_ip;
  memcpy(gateway, s_gateway, sizeof(gateway));
  taskEXIT_CRITICAL(&s_mux);
  if (!got_ip) {
    publish_summary("Network not ready. Connect ETH or WiFi first.", SVC_ERROR);
    return;
  }
  mask &= (1u << NET_PING_TARGETS) - 1;
  if (mask == 0) {
    publish_summary("Select at least one target.", SVC_WARNING);
    return;
  }
  int checked = 0;
  xSemaphoreTake(s_ping_lock, portMAX_DELAY);
  for (int i = 0; i < NET_PING_TARGETS; i++) {
    const bool on = (mask >> i) & 1u;
    checked += on ? 1 : 0;
    net_ping_row_set(&s_rows[i], SVC_MUTED, SVC_MUTED,
                     on ? "Waiting..." : "--");
  }
  publish_rows_locked();
  xSemaphoreGive(s_ping_lock);

  s_job.mask = mask;
  snprintf(s_job.gateway, sizeof(s_job.gateway), "%s",
           gateway[0] ? gateway : net_ping_hosts[3]);
  s_ping_abort = false;
  publish_running(true);
  char summary[64];
  snprintf(summary, sizeof(summary), "Starting test on %d target(s)...",
           checked);
  publish_summary(summary, SVC_WARNING);
  if (xTaskCreate(ping_task, "func_ping", 6144, NULL, 5, NULL) != pdPASS) {
    publish_running(false);
    publish_summary("Ping failed: cannot create worker task", SVC_ERROR);
  }
}

/* ---- SNTP probe ----------------------------------------------------------
 */

static portMUX_TYPE s_probe_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_probe_running;
static volatile bool s_timer_open;
static volatile bool s_ntp_ok;

static void publish_ntp(bool available) {
  s_ntp_ok = available;
  m5_pocket_bridge_set_bool("ntpReady", available);
}

bool net_sntp_local_time(struct tm *out) {
  time_t now = 0;
  time(&now);
  if (now < CLOCK_VALID_FROM) {
    return false;
  }
  setenv("TZ", "CST-8", 1);
  tzset();
  if (!localtime_r(&now, out)) {
    return false;
  }
  const int year = out->tm_year + 1900;
  return year >= 2020 && year < 2099;
}

/* fetch_aliyun_ntp_time */
static esp_err_t sntp_fetch(void) {
  esp_netif_sntp_deinit();
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
  config.wait_for_sync = true;
  config.start = true;
  esp_err_t err = esp_netif_sntp_init(&config);
  if (err == ESP_ERR_INVALID_STATE) {
    esp_netif_sntp_deinit();
    err = esp_netif_sntp_init(&config);
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Aliyun NTP init failed: %s", esp_err_to_name(err));
    return err;
  }
  err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(NTP_SYNC_TIMEOUT_MS));
  esp_netif_sntp_deinit();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Aliyun NTP sync failed: %s", esp_err_to_name(err));
    return err;
  }
  struct tm local;
  if (!net_sntp_local_time(&local)) {
    ESP_LOGW(TAG, "Aliyun NTP sync produced invalid local time");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Aliyun NTP is available: %04d-%02d-%02d %02d:%02d:%02d",
           local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
           local.tm_min, local.tm_sec);
  return ESP_OK;
}

static bool eth_ready(void) {
  taskENTER_CRITICAL(&s_mux);
  const bool ready = s_link && s_got_ip;
  taskEXIT_CRITICAL(&s_mux);
  return ready;
}

/* ntp_probe_task */
static void probe_task(void *arg) {
  (void)arg;
  /* wait_for_ethernet_connection(4000) returns at once without a
   * driver. */
  if (!eth_ready() && s_eth_expected && !s_sleeping) {
    xEventGroupWaitBits(s_events, GOT_IP_BIT, pdFALSE, pdFALSE,
                        pdMS_TO_TICKS(NTP_WAIT_ETH_MS));
  }
  bool available = false;
  if (eth_ready() && !s_sleeping) {
    available = sntp_fetch() == ESP_OK;
  } else {
    ESP_LOGI(TAG, "Aliyun NTP probe skipped: Ethernet is not ready");
  }
  taskENTER_CRITICAL(&s_probe_mux);
  s_probe_running = false;
  taskEXIT_CRITICAL(&s_probe_mux);
  if (s_timer_open) {
    publish_ntp(available);
  }
  vTaskDelete(NULL);
}

void net_svc_timer_page(bool open) {
  s_timer_open = open;
  publish_ntp(false);
  if (!open) {
    return;
  }
  taskENTER_CRITICAL(&s_probe_mux);
  const bool start = !s_probe_running;
  s_probe_running = true;
  taskEXIT_CRITICAL(&s_probe_mux);
  if (start &&
      xTaskCreate(probe_task, "rtc_ntp_probe", 4096, NULL, 5, NULL) != pdPASS) {
    ESP_LOGW(TAG, "Failed to create Aliyun NTP probe task");
    taskENTER_CRITICAL(&s_probe_mux);
    s_probe_running = false;
    taskEXIT_CRITICAL(&s_probe_mux);
  }
}

void net_svc_sntp_forget(void) { publish_ntp(false); }

bool net_svc_sntp_synced(void) { return s_ntp_ok; }

/* ---- service --------------------------------------------------------------
 */

void net_svc_start(void) {
  s_eth_lock = xSemaphoreCreateMutex();
  s_ping_lock = xSemaphoreCreateMutex();
  s_events = xEventGroupCreate();
  if (!s_eth_lock || !s_ping_lock || !s_events) {
    ESP_LOGE(TAG, "no memory");
    return;
  }
  clear_rows();
  publish_running(false);
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "netif init: %s", esp_err_to_name(err));
    return;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
    return;
  }
  err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event,
                                   NULL);
  if (err == ESP_OK) {
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_ip_event,
                                     NULL);
  }
  if (err == ESP_OK) {
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP,
                                     on_ip_event, NULL);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "event handlers: %s", esp_err_to_name(err));
    return;
  }
  publish_eth();
  s_eth_expected = true;
  if (xTaskCreate(net_task, "net", 4096, NULL, 5, NULL) != pdPASS) {
    s_eth_expected = false;
    ESP_LOGE(TAG, "Failed to create the Ethernet task");
  }
}

void net_svc_frame(void) {}

bool net_svc_request(const m5_pocket_request *request) {
  if (svc_is(request, "pingPage")) {
    if (svc_arg(request, 0) != 0) {
      clear_rows();
      publish_eth();
    }
    return true;
  }
  if (svc_is(request, "ping")) {
    start_ping((unsigned)svc_arg(request, 0));
    return true;
  }
  return false;
}

void net_svc_sleep(void) {
  s_sleeping = true;
  s_eth_expected = false;
  s_ping_abort = true;
  if (s_ping_lock) {
    xSemaphoreTake(s_ping_lock, portMAX_DELAY);
    if (s_session) {
      esp_ping_stop(s_session);
    }
    xSemaphoreGive(s_ping_lock);
  }
  taskENTER_CRITICAL(&s_probe_mux);
  const bool probing = s_probe_running;
  taskEXIT_CRITICAL(&s_probe_mux);
  if (!probing) {
    /* A running probe deinitializes SNTP itself; its sync wait uses the
     * SNTP semaphore. */
    esp_netif_sntp_deinit();
  }
  if (s_eth_lock) {
    xSemaphoreTake(s_eth_lock, portMAX_DELAY);
    eth_down();
    xSemaphoreGive(s_eth_lock);
  }
  m5_corep4x_eth_release_pins();
  s_pins_released = true;
  ESP_LOGI(TAG, "Ethernet PHY reset asserted (held low), RMII GPIOs set to "
                "high-Z");
}
