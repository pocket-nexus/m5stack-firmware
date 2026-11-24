/* NEC transmit and receive on the RMT (svc.h), after
 * ui_function_irtxrx_window.cpp and otool_ir/ir_tools.cpp of
 * M5CoreP4X-UserDemo.
 *
 * TX GPIO49: 1 MHz, 64 symbols, queue depth 2, output inverted, 38 kHz
 * carrier at 50 % duty. RX GPIO13: 1 MHz, 128 symbols, carrier
 * demodulation 38 kHz at 33 % duty, glitch filter 1250 ns, idle after
 * 30 ms, a 512-symbol receive buffer, decoded on the "ir_rx_task". The
 * receiver keeps running after the window closes, as in the factory.
 *
 * Requests: "ir 1|0" (window open/closed), "irInit", "irSend <address>
 * <command> <repeat 0|1> <auto 0|1>", "irClear". Published: irReady,
 * irInitializing, irSending, irResult + irResultTone, irFrames, irAddress,
 * irCommand, irRepeat, and the event irNext after an Auto+1 send. */
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ir_nec.h"
#include "m5_corep4x/bsp.h"
#include "svc.h"
#include <stdio.h>

static const char *TAG = "ir_svc";

#define RESOLUTION_HZ 1000000
#define CARRIER_HZ 38000
#define TX_DUTY 0.5f
#define RX_DUTY 0.33f
#define TX_SYMBOLS 64
#define RX_SYMBOLS 128
#define RX_BUFFER_SYMBOLS 512
#define RX_MIN_NS 1250
#define RX_MAX_NS 30000000
/* Two frames of at most 120 ms each. */
#define TX_DONE_TIMEOUT_MS 1000

static rmt_channel_handle_t s_tx;
static rmt_channel_handle_t s_rx;
static rmt_encoder_handle_t s_copy;
static QueueHandle_t s_rx_queue;
static TaskHandle_t s_rx_task;
static rmt_symbol_word_t *s_rx_raw;
static uint32_t s_rx_words[RX_BUFFER_SYMBOLS];
static rmt_symbol_word_t s_tx_frame[IR_NEC_FRAME_SYMBOLS];
static ir_nec_state_t s_nec;

/* The driver is set up (is_initialized). */
static volatile bool s_ready;
static volatile bool s_initializing;
static volatile bool s_sending;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_frames;

static struct {
  uint16_t address;
  uint16_t command;
  bool repeat;
  bool auto_increment;
} s_send;

static void publish_result(const char *text, uint8_t tone) {
  m5_pocket_bridge_set_number("irResultTone", tone);
  m5_pocket_bridge_set_text("irResult", text);
}

static void publish_state(void) {
  m5_pocket_bridge_set_bool("irReady", s_ready);
  m5_pocket_bridge_set_bool("irInitializing", s_initializing);
  m5_pocket_bridge_set_bool("irSending", s_sending);
}

/* ---- receive --------------------------------------------------------------
 */

static bool rx_done(rmt_channel_handle_t channel,
                    const rmt_rx_done_event_data_t *data, void *arg) {
  (void)channel;
  (void)arg;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(s_rx_queue, data, &woken);
  return woken == pdTRUE;
}

/* ir_rx_on_decoded: the newest frame and the count. */
static void received(const rmt_symbol_word_t *symbols, size_t count) {
  if (count > RX_BUFFER_SYMBOLS) {
    count = RX_BUFFER_SYMBOLS;
  }
  for (size_t i = 0; i < count; i++) {
    s_rx_words[i] = ir_symbol(symbols[i].duration0, symbols[i].level0,
                              symbols[i].duration1, symbols[i].level1);
  }
  ir_nec_frame_t frame;
  if (!ir_nec_decode(&s_nec, s_rx_words, count, &frame)) {
    return;
  }
  ESP_LOGI(TAG, "[NEC] addr=0x%04X cmd=0x%04X%s", frame.address, frame.command,
           frame.repeat ? " (repeat)" : "");
  taskENTER_CRITICAL(&s_mux);
  const uint32_t frames = ++s_frames;
  taskEXIT_CRITICAL(&s_mux);
  m5_pocket_bridge_set_number("irAddress", frame.address);
  m5_pocket_bridge_set_number("irCommand", frame.command);
  m5_pocket_bridge_set_bool("irRepeat", frame.repeat);
  m5_pocket_bridge_set_number("irFrames", frames);
}

static void rx_task(void *arg) {
  (void)arg;
  const rmt_receive_config_t config = {
      .signal_range_min_ns = RX_MIN_NS,
      .signal_range_max_ns = RX_MAX_NS,
  };
  const size_t size = RX_BUFFER_SYMBOLS * sizeof(rmt_symbol_word_t);
  esp_err_t err = rmt_receive(s_rx, s_rx_raw, size, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "start receive failed: %s", esp_err_to_name(err));
  }
  while (err == ESP_OK) {
    rmt_rx_done_event_data_t event;
    if (xQueueReceive(s_rx_queue, &event, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    received(event.received_symbols, event.num_symbols);
    err = rmt_receive(s_rx, s_rx_raw, size, &config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "restart receive failed: %s", esp_err_to_name(err));
    }
  }
  s_rx_task = NULL;
  vTaskDelete(NULL);
}

static esp_err_t start_receive(void) {
  if (s_rx_task) {
    return ESP_OK;
  }
  if (xTaskCreate(rx_task, "ir_rx_task", 4096, NULL, 5, &s_rx_task) != pdPASS) {
    s_rx_task = NULL;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

/* ---- set-up ---------------------------------------------------------------
 */

static void delete_channels(void) {
  if (s_tx) {
    rmt_disable(s_tx);
    rmt_del_channel(s_tx);
    s_tx = NULL;
  }
  if (s_rx) {
    rmt_disable(s_rx);
    rmt_del_channel(s_rx);
    s_rx = NULL;
  }
  if (s_copy) {
    rmt_del_encoder(s_copy);
    s_copy = NULL;
  }
}

static esp_err_t tx_carrier(void) {
  const rmt_carrier_config_t carrier = {
      .frequency_hz = CARRIER_HZ,
      .duty_cycle = TX_DUTY,
  };
  return rmt_apply_carrier(s_tx, &carrier);
}

static esp_err_t create_channels(void) {
  const rmt_tx_channel_config_t tx_config = {
      .gpio_num = M5_COREP4X_IR_TX,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = RESOLUTION_HZ,
      .mem_block_symbols = TX_SYMBOLS,
      .trans_queue_depth = 2,
      .flags.invert_out = 1,
  };
  esp_err_t err = rmt_new_tx_channel(&tx_config, &s_tx);
  if (err != ESP_OK) {
    s_tx = NULL;
    return err;
  }
  if ((err = tx_carrier()) != ESP_OK || (err = rmt_enable(s_tx)) != ESP_OK) {
    return err;
  }
  const rmt_copy_encoder_config_t copy_config = {};
  if ((err = rmt_new_copy_encoder(&copy_config, &s_copy)) != ESP_OK) {
    s_copy = NULL;
    return err;
  }

  const rmt_rx_channel_config_t rx_config = {
      .gpio_num = M5_COREP4X_IR_RX,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = RESOLUTION_HZ,
      .mem_block_symbols = RX_SYMBOLS,
  };
  if ((err = rmt_new_rx_channel(&rx_config, &s_rx)) != ESP_OK) {
    s_rx = NULL;
    return err;
  }
  const rmt_carrier_config_t demodulation = {
      .frequency_hz = CARRIER_HZ,
      .duty_cycle = RX_DUTY,
  };
  if ((err = rmt_apply_carrier(s_rx, &demodulation)) != ESP_OK) {
    return err;
  }
  const rmt_rx_event_callbacks_t callbacks = {.on_recv_done = rx_done};
  if ((err = rmt_rx_register_event_callbacks(s_rx, &callbacks, NULL)) !=
      ESP_OK) {
    return err;
  }
  return rmt_enable(s_rx);
}

/* ir_tools::init(TX GPIO49, RX GPIO13, no enable pin). */
static esp_err_t driver_init(void) {
  if (!s_rx_queue) {
    s_rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
  }
  if (!s_rx_raw) {
    s_rx_raw = heap_caps_malloc(RX_BUFFER_SYMBOLS * sizeof(rmt_symbol_word_t),
                                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  if (!s_rx_queue || !s_rx_raw) {
    return ESP_ERR_NO_MEM;
  }
  gpio_reset_pin(M5_COREP4X_IR_TX);
  gpio_set_direction(M5_COREP4X_IR_TX, GPIO_MODE_OUTPUT);
  gpio_set_level(M5_COREP4X_IR_TX, 0);
  gpio_reset_pin(M5_COREP4X_IR_RX);
  gpio_set_direction(M5_COREP4X_IR_RX, GPIO_MODE_INPUT);
  const esp_err_t err = create_channels();
  if (err != ESP_OK) {
    delete_channels();
  }
  return err;
}

/* ir_init_task */
static void init_task(void *arg) {
  (void)arg;
  if (m5_corep4x_set_switch(M5_COREP4X_SWITCH_L3B, true) == ESP_OK) {
    m5_pocket_bridge_set_bool("railL3B", true);
  } else {
    ESP_LOGW(TAG, "L3B on failed");
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_err_t err = ESP_OK;
  if (!s_tx) {
    err = driver_init();
  }
  if (err == ESP_OK) {
    err = start_receive();
  }
  if (err == ESP_OK) {
    s_ready = true;
  }
  char text[64];
  snprintf(text, sizeof(text), "IR TX/RX init %s: %s",
           err == ESP_OK ? "OK" : "failed", esp_err_to_name(err));
  s_initializing = false;
  publish_state();
  publish_result(text, err == ESP_OK ? SVC_SUCCESS : SVC_ERROR);
  vTaskDelete(NULL);
}

/* start_ir_init_task */
static void start_init(const char *text) {
  if (s_initializing) {
    return;
  }
  s_initializing = true;
  publish_state();
  publish_result(text, SVC_WARNING);
  if (xTaskCreate(init_task, "func_ir_init", 4096, NULL, 5, NULL) != pdPASS) {
    s_initializing = false;
    publish_state();
    publish_result("Cannot create IR init task.", SVC_ERROR);
  }
}

/* ---- send -----------------------------------------------------------------
 */

/* ir_tools::send for NEC: the 38 kHz carrier applied, the frame queued
 * (twice with Repeat), then the end of the transmission awaited. */
static esp_err_t send_frame(uint16_t address, uint16_t command, bool repeat) {
  uint32_t words[IR_NEC_FRAME_SYMBOLS];
  ir_nec_encode(address, command, words);
  for (size_t i = 0; i < IR_NEC_FRAME_SYMBOLS; i++) {
    s_tx_frame[i].duration0 = ir_symbol_duration0(words[i]);
    s_tx_frame[i].level0 = (words[i] >> 15) & 1u;
    s_tx_frame[i].duration1 = ir_symbol_duration1(words[i]);
    s_tx_frame[i].level1 = (words[i] >> 31) & 1u;
  }
  esp_err_t err = tx_carrier();
  if (err != ESP_OK) {
    return err;
  }
  const rmt_transmit_config_t config = {.loop_count = 0};
  const int frames = repeat ? 2 : 1;
  for (int i = 0; i < frames && err == ESP_OK; i++) {
    err = rmt_transmit(s_tx, s_copy, s_tx_frame, sizeof(s_tx_frame), &config);
  }
  const esp_err_t done = rmt_tx_wait_all_done(s_tx, TX_DONE_TIMEOUT_MS);
  return err != ESP_OK ? err : done;
}

/* ir_send_task */
static void send_task(void *arg) {
  (void)arg;
  char text[96];
  uint8_t tone = SVC_WARNING;
  if (!s_ready) {
    snprintf(text, sizeof(text), "IR is not initialized yet.");
  } else {
    const esp_err_t err =
        send_frame(s_send.address, s_send.command, s_send.repeat);
    tone = err == ESP_OK ? SVC_SUCCESS : SVC_ERROR;
    if (s_send.auto_increment) {
      const uint16_t next = (uint16_t)(s_send.command + 1u);
      snprintf(text, sizeof(text), "TX 0x%04X/0x%04X -> %s  next cmd 0x%04X",
               s_send.address, s_send.command, esp_err_to_name(err), next);
      m5_pocket_bridge_post_event("irNext", next);
    } else {
      snprintf(text, sizeof(text), "TX NEC addr 0x%04X cmd 0x%04X -> %s",
               s_send.address, s_send.command, esp_err_to_name(err));
    }
  }
  s_sending = false;
  publish_state();
  publish_result(text, tone);
  vTaskDelete(NULL);
}

/* ir_send_event_cb */
static void start_send(const m5_pocket_request *request) {
  if (s_initializing || s_sending) {
    return;
  }
  s_send.address = (uint16_t)svc_arg(request, 0);
  s_send.command = (uint16_t)svc_arg(request, 1);
  s_send.repeat = svc_arg(request, 2) != 0;
  s_send.auto_increment = svc_arg(request, 3) != 0;
  s_sending = true;
  publish_state();
  publish_result("Sending NEC frame...", SVC_WARNING);
  if (xTaskCreate(send_task, "func_ir_tx", 4096, NULL, 5, NULL) != pdPASS) {
    s_sending = false;
    publish_state();
    publish_result("Cannot create IR send task.", SVC_ERROR);
  }
}

/* ---- service --------------------------------------------------------------
 */

void ir_svc_start(void) {}

void ir_svc_frame(void) {}

bool ir_svc_request(const m5_pocket_request *request) {
  if (svc_is(request, "ir")) {
    if (svc_arg(request, 0) == 0) {
      return true;
    }
    if (s_ready) {
      start_receive();
      publish_state();
      publish_result("IR TX/RX is ready. NEC RX active.", SVC_SUCCESS);
    } else {
      start_init("Auto initializing IR TX/RX...");
    }
    return true;
  }
  if (svc_is(request, "irInit")) {
    start_init("Reinitializing IR TX/RX...");
    return true;
  }
  if (svc_is(request, "irSend")) {
    start_send(request);
    return true;
  }
  if (svc_is(request, "irClear")) {
    taskENTER_CRITICAL(&s_mux);
    s_frames = 0;
    taskEXIT_CRITICAL(&s_mux);
    m5_pocket_bridge_set_number("irFrames", 0);
    return true;
  }
  return false;
}
