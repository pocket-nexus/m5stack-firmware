/* Calls on an internal-RAM stack for an app task whose stack is in PSRAM.
 * A flash operation (NVS, FAT, Wi-Fi start) switches the cache off, so the
 * task that makes it cannot have its stack in PSRAM. The host starts one
 * helper task with an internal stack next to such an app task and runs the
 * program's request handler on it; hooks use m5_pocket_app_run_internal. */
#include "app_internal.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "freertos/queue.h"

static const char *TAG = "m5_pocket_app";

/* Declared in esp_memory_utils.h. That header is not included: its inline
 * esp_ptr_in_rtc_slow() leaves its parameter unused on ESP32-P4, which the
 * warnings of this component reject (m5_strict_warnings). */
bool esp_ptr_external_ram(const void *p);

typedef struct {
  void (*fn)(void *context);
  void *context;
} internal_call;

static QueueHandle_t calls;
static SemaphoreHandle_t callers, done;
static TaskHandle_t helper;
static uint32_t stack_free_min;

static void run_calls(void *argument) {
  (void)argument;
  internal_call call;
  for (;;) {
    if (xQueueReceive(calls, &call, portMAX_DELAY) != pdTRUE)
      continue;
    call.fn(call.context);
    const uint32_t free_bytes =
        uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    if (!stack_free_min || free_bytes < stack_free_min)
      stack_free_min = free_bytes;
    xSemaphoreGive(done);
  }
}

esp_err_t m5_pocket_internal_start(const m5_pocket_app_config *config) {
  if (helper)
    return ESP_OK;
  calls = xQueueCreate(1, sizeof(internal_call));
  callers = xSemaphoreCreateMutex();
  done = xSemaphoreCreateBinary();
  const BaseType_t core =
      config->task_core < 0 ? tskNO_AFFINITY : (BaseType_t)config->task_core;
  const UBaseType_t priority = config->task_priority
                                   ? (UBaseType_t)config->task_priority
                                   : CONFIG_M5_POCKET_APP_TASK_PRIORITY;
  stack_free_min = 0;
  if (!calls || !callers || !done ||
      xTaskCreatePinnedToCore(run_calls, "m5_pocket_int",
                              CONFIG_M5_POCKET_APP_INTERNAL_STACK, NULL,
                              priority, &helper, core) != pdPASS) {
    helper = NULL;
    m5_pocket_internal_stop();
    ESP_LOGE(TAG, "no internal-stack helper task");
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

void m5_pocket_internal_stop(void) {
  if (helper)
    vTaskDelete(helper);
  if (calls)
    vQueueDelete(calls);
  if (callers)
    vSemaphoreDelete(callers);
  if (done)
    vSemaphoreDelete(done);
  helper = NULL;
  calls = NULL;
  callers = done = NULL;
}

uint32_t m5_pocket_internal_stack_free_min(void) { return stack_free_min; }

esp_err_t m5_pocket_app_run_internal(void (*fn)(void *context), void *context) {
  if (!fn)
    return ESP_ERR_INVALID_ARG;
  /* A caller on an internal stack, the helper itself included, runs fn. */
  if (!esp_ptr_external_ram(esp_cpu_get_sp())) {
    fn(context);
    return ESP_OK;
  }
  if (!helper)
    return ESP_ERR_INVALID_STATE;
  const internal_call call = {.fn = fn, .context = context};
  xSemaphoreTake(callers, portMAX_DELAY);
  xQueueSend(calls, &call, portMAX_DELAY);
  xSemaphoreTake(done, portMAX_DELAY);
  xSemaphoreGive(callers);
  return ESP_OK;
}
