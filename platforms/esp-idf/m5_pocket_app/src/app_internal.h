#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "m5_pocket/app.h"
#include <stdatomic.h>

#define M5_POCKET_INPUTS_MAX 8

/* The display, renderer and input objects shared by both modes. */
typedef struct {
  const pocket_display_output_v1 *display;
  void *display_context;
  size_t display_bytes;
  const pocket_renderer_v1 *renderer; /* NULL: software */
  void *renderer_context;
  const pocket_input_source_v1 *inputs[M5_POCKET_INPUTS_MAX];
  void *input_contexts[M5_POCKET_INPUTS_MAX];
  size_t input_count;
} m5_pocket_devices;

struct m5_pocket_app {
  m5_pocket_app_config config;
  m5_pocket_devices devices;
  void *display_object; /* pocket_display_esp_lcd / _dpi, when created */
  void *ppa_device, *ppa_renderer;
  void *backend; /* mode state */
  uint32_t cadence_hz;
  TaskHandle_t task;
  SemaphoreHandle_t started, stopped;
  esp_err_t start_result;
  atomic_bool stop;
  m5_pocket_app_stats stats;
};

const pocket_input_source_v1 *m5_pocket_input_vtable(void);

/* Mode backends (app_js.c or app_aot.c), called on the app task. boot
 * creates the runtime, boots and presents; it sets app->cadence_hz. */
esp_err_t m5_pocket_backend_boot(m5_pocket_app *app);
int32_t m5_pocket_backend_tick(m5_pocket_app *app);
void m5_pocket_backend_destroy(m5_pocket_app *app);
