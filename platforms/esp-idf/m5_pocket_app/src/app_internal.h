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
  void *ppa_devices[2], *ppa_renderers[2], *ppa_lanes;
  void *backend; /* mode state */
  uint32_t cadence_hz;
  TaskHandle_t task;
  SemaphoreHandle_t started, stopped;
  esp_err_t start_result;
  atomic_bool stop;
  m5_pocket_app_stats stats;
  /* Image names m5_pocket_app_set_texture bound, with their handles. */
  struct {
    char *name;
    int32_t handle;
  } textures[M5_POCKET_APP_TEXTURES_MAX];
};

const pocket_input_source_v1 *m5_pocket_input_vtable(void);

/* Mode backends (app_js.c or app_aot.c), called on the app task. boot
 * creates the runtime, boots and presents; it sets app->cadence_hz. */
esp_err_t m5_pocket_backend_boot(m5_pocket_app *app);
int32_t m5_pocket_backend_tick(m5_pocket_app *app);
void m5_pocket_backend_destroy(m5_pocket_app *app);
/* Texture operations of m5_pocket_app_set_texture, on the app task between
 * frames. upload returns a handle or -1; bind maps a name to a handle (-1:
 * unbind) for the application. */
int32_t m5_pocket_backend_upload_texture(m5_pocket_app *app,
                                         const uint16_t *pixels, uint32_t width,
                                         uint32_t height);
void m5_pocket_backend_free_texture(m5_pocket_app *app, int32_t handle);
esp_err_t m5_pocket_backend_bind_texture(m5_pocket_app *app, const char *name,
                                         int32_t handle);
pocket_runtime *m5_pocket_backend_runtime(m5_pocket_app *app);
