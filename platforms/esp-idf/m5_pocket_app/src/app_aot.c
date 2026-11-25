/* AOT mode: the application is a MicroTS-compiled Rust static library that
 * implements m5_pocket/aot.h. It reads and writes the bridge itself, between
 * frames, through the generated device.ts accessors. */
#include "app_internal.h"
#include "esp_log.h"
#include "m5_pocket/aot.h"
#include <string.h>

static const char *TAG = "m5_pocket_app";

esp_err_t m5_pocket_backend_boot(m5_pocket_app *app) {
  const m5_pocket_aot_config config = {
      .struct_size = sizeof(config),
      .display = app->devices.display,
      .display_context = app->devices.display_context,
      .renderer = app->devices.renderer,
      .renderer_context = app->devices.renderer_context,
      .inputs = app->devices.inputs,
      .input_contexts = app->devices.input_contexts,
      .input_count = app->devices.input_count,
      .heap_limit = app->config.heap_limit,
      .frame_buffer_budget = app->devices.display_bytes,
  };
  m5_pocket_aot *aot = m5_pocket_aot_create(&config);
  if (!aot) {
    ESP_LOGE(TAG, "m5_pocket_aot_create failed");
    return ESP_FAIL;
  }
  app->backend = aot;
  app->cadence_hz = m5_pocket_aot_cadence_hz(aot);
  return ESP_OK;
}

int32_t m5_pocket_backend_tick(m5_pocket_app *app) {
  return m5_pocket_aot_tick(app->backend);
}

int32_t m5_pocket_backend_skip(m5_pocket_app *app) {
  return m5_pocket_aot_skip(app->backend);
}

pocket_runtime *m5_pocket_backend_runtime(m5_pocket_app *app) {
  (void)app;
  return NULL;
}

void m5_pocket_backend_destroy(m5_pocket_app *app) {
  if (app->backend)
    m5_pocket_aot_destroy(app->backend);
  app->backend = NULL;
}

int32_t m5_pocket_backend_upload_texture(m5_pocket_app *app,
                                         const uint16_t *pixels, uint32_t width,
                                         uint32_t height) {
  return m5_pocket_aot_upload_texture(app->backend, pixels,
                                      (size_t)width * height * sizeof(uint16_t),
                                      width, height, 0 /* PSM_5650 */);
}

void m5_pocket_backend_free_texture(m5_pocket_app *app, int32_t handle) {
  m5_pocket_aot_free_texture(app->backend, handle);
}

esp_err_t m5_pocket_backend_bind_texture(m5_pocket_app *app, const char *name,
                                         int32_t handle) {
  return m5_pocket_aot_register_image(app->backend, name, strlen(name),
                                      handle) == POCKET_STATUS_OK
             ? ESP_OK
             : ESP_ERR_INVALID_STATE;
}
