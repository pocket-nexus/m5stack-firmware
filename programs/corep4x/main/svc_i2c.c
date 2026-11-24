/* I2C SCAN window: the stable scan of ui_function_i2cscan_window.cpp, three
 * passes over 0x08..0x77 50 ms apart, keeping the addresses that answered
 * every pass, on the internal bus or on Port A (I2C1, created for the scan
 * and deleted afterwards). */
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_corep4x/bsp.h"
#include "svc.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "svc_i2c";

#define FIRST 0x08
#define LAST 0x77
#define PASSES 3
#define PASS_GAP_MS 50
#define PROBE_TIMEOUT_MS 200

static atomic_bool s_running;
/* Set by the scan task, published by i2c_svc_frame on the app task so the
 * addresses and the result event reach the application in one frame. */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_done;
static int s_result;
static float s_found[LAST - FIRST + 1];
static size_t s_count;

static void scan_bus(i2c_master_bus_handle_t bus, float *found, size_t *count) {
  uint8_t hits[LAST + 1] = {0};
  for (int pass = 0; pass < PASSES; pass++) {
    for (int addr = FIRST; addr <= LAST; addr++) {
      if (i2c_master_probe(bus, (uint16_t)addr, PROBE_TIMEOUT_MS) == ESP_OK) {
        hits[addr]++;
      }
    }
    if (pass + 1 < PASSES) {
      vTaskDelay(pdMS_TO_TICKS(PASS_GAP_MS));
    }
  }
  *count = 0;
  for (int addr = FIRST; addr <= LAST; addr++) {
    if (hits[addr] == PASSES) {
      found[(*count)++] = (float)addr;
    }
  }
}

static void scan_task(void *arg) {
  const bool port_a = arg != NULL;
  float found[LAST - FIRST + 1];
  size_t count = 0;
  int result = 0;
  if (!port_a) {
    i2c_master_bus_handle_t bus = m5_corep4x_i2c();
    if (bus) {
      scan_bus(bus, found, &count);
    } else {
      result = -1;
    }
  } else {
    /* An existing bus on port 1 is reused and kept. */
    i2c_master_bus_handle_t bus = NULL;
    bool owned = false;
    if (i2c_master_get_bus_handle(M5_COREP4X_PORT_A_I2C_PORT, &bus) != ESP_OK) {
      const i2c_master_bus_config_t config = {
          .i2c_port = M5_COREP4X_PORT_A_I2C_PORT,
          .sda_io_num = M5_COREP4X_PORT_A_SDA,
          .scl_io_num = M5_COREP4X_PORT_A_SCL,
          .clk_source = I2C_CLK_SRC_DEFAULT,
          .glitch_ignore_cnt = 7,
          .flags.enable_internal_pullup = true,
      };
      if (i2c_new_master_bus(&config, &bus) == ESP_OK) {
        owned = true;
      } else {
        bus = NULL;
      }
    }
    if (bus) {
      scan_bus(bus, found, &count);
      if (owned) {
        i2c_del_master_bus(bus);
      }
    } else {
      result = -1;
    }
  }
  ESP_LOGI(TAG, "%s: %u device(s)", port_a ? "Port A" : "internal",
           (unsigned)count);
  portENTER_CRITICAL(&s_lock);
  memcpy(s_found, found, count * sizeof(float));
  s_count = count;
  s_result = result;
  s_done = true;
  portEXIT_CRITICAL(&s_lock);
  vTaskDelete(NULL);
}

bool i2c_svc_request(const m5_pocket_request *request) {
  if (!svc_is(request, "i2cScan")) {
    return false;
  }
  bool idle = false;
  if (!atomic_compare_exchange_strong(&s_running, &idle, true)) {
    return true;
  }
  const bool port_a = svc_arg(request, 0) == 1;
  if (xTaskCreate(scan_task, "func_i2c", 4096, port_a ? (void *)1 : NULL,
                  configMAX_PRIORITIES - 4, NULL) != pdPASS) {
    atomic_store(&s_running, false);
    m5_pocket_bridge_set_array("i2cAddresses", NULL, 0);
    m5_pocket_bridge_post_event("i2cScan", -2);
  }
  return true;
}

void i2c_svc_frame(void) {
  float found[LAST - FIRST + 1];
  size_t count = 0;
  int result = 0;
  bool done = false;
  portENTER_CRITICAL(&s_lock);
  if (s_done) {
    s_done = false;
    done = true;
    count = s_count;
    result = s_result;
    memcpy(found, s_found, count * sizeof(float));
  }
  portEXIT_CRITICAL(&s_lock);
  if (!done) {
    return;
  }
  m5_pocket_bridge_set_array("i2cAddresses", found, count);
  m5_pocket_bridge_post_event("i2cScan", result);
  atomic_store(&s_running, false);
}

void i2c_svc_start(void) {}
