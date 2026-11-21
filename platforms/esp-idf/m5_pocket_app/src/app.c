#include "app_internal.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#if CONFIG_IDF_TARGET_ESP32P4
#include "pocket/render_esp_ppa.h"
#include "pocketjs/esp32p4_ppa_lanes.h"
#include "pocketjs/esp32p4_ppa_status.h"
#endif

static const char *TAG = "m5_pocket_app";
static atomic_bool running;

m5_pocket_app_mode m5_pocket_app_build_mode(void) {
#if CONFIG_M5_POCKET_APP_MODE_AOT
  return M5_POCKET_APP_MODE_AOT;
#else
  return M5_POCKET_APP_MODE_JS;
#endif
}

static esp_err_t create_display(m5_pocket_app *app) {
  const m5_pocket_display_config *display = &app->config.display;
  m5_pocket_devices *devices = &app->devices;
  switch (display->kind) {
  case M5_POCKET_DISPLAY_ESP_LCD: {
    pocket_display_esp_lcd_config lcd = display->lcd;
    lcd.struct_size = sizeof(lcd);
    pocket_display_esp_lcd *object = pocket_display_esp_lcd_create(&lcd);
    if (!object) {
      ESP_LOGE(TAG, "pocket_display_esp_lcd_create failed");
      return ESP_ERR_NO_MEM;
    }
    app->display_object = object;
    devices->display = pocket_display_esp_lcd_vtable();
    devices->display_context = object;
    devices->display_bytes = pocket_display_esp_lcd_buffer_bytes(object);
    return ESP_OK;
  }
#if CONFIG_IDF_TARGET_ESP32P4
  case M5_POCKET_DISPLAY_ESP_DPI: {
    pocket_display_esp_dpi_config dpi = display->dpi;
    dpi.struct_size = sizeof(dpi);
    pocket_display_esp_dpi *object = pocket_display_esp_dpi_create(&dpi);
    if (!object) {
      ESP_LOGE(TAG, "pocket_display_esp_dpi_create failed");
      return ESP_ERR_NO_MEM;
    }
    app->display_object = object;
    devices->display = pocket_display_esp_dpi_vtable();
    devices->display_context = object;
    devices->display_bytes = pocket_display_esp_dpi_buffer_bytes(object);
    return ESP_OK;
  }
#endif
  case M5_POCKET_DISPLAY_CUSTOM:
    if (!display->custom)
      return ESP_ERR_INVALID_ARG;
    devices->display = display->custom;
    devices->display_context = display->custom_context;
    devices->display_bytes = display->custom_buffer_bytes;
    return ESP_OK;
  default:
    return ESP_ERR_INVALID_ARG;
  }
}

static esp_err_t create_renderer(m5_pocket_app *app) {
#if CONFIG_IDF_TARGET_ESP32P4
  const m5_pocket_display_config *display = &app->config.display;
  const m5_pocket_app_image *image = app->config.image;
  if (!display->ppa)
    return ESP_OK;
  const uint32_t lanes = display->ppa_lanes ? display->ppa_lanes : 1;
  if (lanes > 2 || display->ppa_helper_core < 0 ||
      display->ppa_helper_core >= portNUM_PROCESSORS)
    return ESP_ERR_INVALID_ARG;
  /* The renderer draws at the raster density: logical pixels times it. */
  const uint32_t density = image->facts && image->facts->display
                               ? image->facts->display->raster_density
                               : 1;
  const pocket_ppa_config config = {
      .struct_size = sizeof(config),
      .scale = density ? density : 1,
      .min_fill_pixels = 1024,
      .min_blend_pixels = 256,
      .min_srm_pixels = 256,
      .mask_budget = display->ppa_mask_budget ? display->ppa_mask_budget
                                              : app->devices.display_bytes / 2,
  };
  for (uint32_t i = 0; i < lanes; i++) {
    pocketjs_esp32p4_ppa_t *device = NULL;
    esp_err_t err = pocketjs_esp32p4_ppa_create(&device);
    if (err != ESP_OK)
      return err;
    app->ppa_devices[i] = device;
    app->ppa_renderers[i] = pocket_render_esp_ppa_create(
        &config, pocketjs_esp32p4_ppa_status_ops(device));
    if (!app->ppa_renderers[i])
      return ESP_ERR_NO_MEM;
  }
  if (lanes == 1) {
    app->devices.renderer = pocket_render_esp_ppa_vtable();
    app->devices.renderer_context = app->ppa_renderers[0];
    return ESP_OK;
  }
  uint32_t row_alignment = display->ppa_row_alignment;
  if (!row_alignment)
    row_alignment =
        display->kind == M5_POCKET_DISPLAY_ESP_DPI && display->dpi.row_alignment
            ? display->dpi.row_alignment
            : 1;
  pocketjs_esp32p4_ppa_lanes_t *pair = NULL;
  esp_err_t err = pocketjs_esp32p4_ppa_lanes_create(
      app->ppa_renderers[0], app->ppa_renderers[1], display->ppa_helper_core,
      row_alignment, &pair);
  if (err != ESP_OK)
    return err;
  app->ppa_lanes = pair;
  app->devices.renderer = pocketjs_esp32p4_ppa_lanes_vtable();
  app->devices.renderer_context = pair;
#else
  (void)app;
#endif
  return ESP_OK;
}

static esp_err_t create_inputs(m5_pocket_app *app) {
  const m5_pocket_input_config *input = &app->config.input;
  m5_pocket_devices *devices = &app->devices;
  if (input->source_count > M5_POCKET_INPUTS_MAX - 1 ||
      (input->source_count && (!input->sources || !input->source_contexts)))
    return ESP_ERR_INVALID_ARG;
  devices->inputs[0] = m5_pocket_input_vtable();
  devices->input_contexts[0] = &app->config.input;
  devices->input_count = 1;
  for (size_t i = 0; i < input->source_count; i++) {
    if (!input->sources[i])
      return ESP_ERR_INVALID_ARG;
    devices->inputs[devices->input_count] = input->sources[i];
    devices->input_contexts[devices->input_count++] = input->source_contexts[i];
  }
  return ESP_OK;
}

static void destroy_devices(m5_pocket_app *app) {
#if CONFIG_IDF_TARGET_ESP32P4
  /* The lanes before their renderers, the renderers before their devices. */
  if (app->ppa_lanes)
    pocketjs_esp32p4_ppa_lanes_destroy(app->ppa_lanes);
  for (int i = 0; i < 2; i++) {
    if (app->ppa_renderers[i])
      pocket_render_esp_ppa_destroy(app->ppa_renderers[i]);
    if (app->ppa_devices[i])
      pocketjs_esp32p4_ppa_destroy(app->ppa_devices[i]);
    app->ppa_renderers[i] = app->ppa_devices[i] = NULL;
  }
  app->ppa_lanes = NULL;
  if (app->display_object &&
      app->config.display.kind == M5_POCKET_DISPLAY_ESP_DPI)
    pocket_display_esp_dpi_destroy(app->display_object);
#endif
  if (app->display_object &&
      app->config.display.kind == M5_POCKET_DISPLAY_ESP_LCD)
    pocket_display_esp_lcd_destroy(app->display_object);
  app->display_object = NULL;
}

static void update_stack(m5_pocket_app *app) {
  uint32_t free_bytes = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
  if (!app->stats.stack_free_min || free_bytes < app->stats.stack_free_min)
    app->stats.stack_free_min = free_bytes;
}

static void run(void *argument) {
  m5_pocket_app *app = argument;
  const m5_pocket_app_config *config = &app->config;
  app->start_result = m5_pocket_backend_boot(app);
  update_stack(app);
  xSemaphoreGive(app->started);
  if (app->start_result == ESP_OK) {
    const int64_t period = 1000000 / (app->cadence_hz ? app->cadence_hz : 30);
    int64_t next = esp_timer_get_time();
    while (!atomic_load(&app->stop)) {
      if (config->before_frame)
        config->before_frame(config->hook_context);
      const int64_t started = esp_timer_get_time();
      const int32_t status = m5_pocket_backend_tick(app);
      const int64_t now = esp_timer_get_time();
      const uint32_t spent = (uint32_t)(now - started);
      app->stats.frames++;
      app->stats.last_frame_us = spent;
      if (spent > app->stats.max_frame_us)
        app->stats.max_frame_us = spent;
      if (status != POCKET_STATUS_OK) {
        /* A failed guest stays failed; log the first and every 256th. */
        if ((app->stats.frame_errors++ & 0xFF) == 0)
          ESP_LOGE(TAG, "frame %" PRIu32 " status %d", app->stats.frames,
                   (int)status);
      }
      if ((app->stats.frames & 0x1F) == 1)
        update_stack(app);
      if (config->after_frame)
        config->after_frame(config->hook_context, status);
      next += period;
      if (now >= next) {
        /* Behind schedule: skip the slots already missed. */
        const int64_t missed = (now - next) / period + 1;
        app->stats.frames_skipped += (uint32_t)missed;
        next += missed * period;
      }
      const int64_t wait_us = next - esp_timer_get_time();
      if (wait_us > 0)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS((wait_us + 999) / 1000));
    }
  }
  m5_pocket_backend_destroy(app);
  xSemaphoreGive(app->stopped);
  vTaskSuspend(NULL);
}

static void release(m5_pocket_app *app) {
  if (app->task) {
    if (app->config.task_stack_caps)
      vTaskDeleteWithCaps(app->task);
    else
      vTaskDelete(app->task);
  }
  destroy_devices(app);
  if (app->started)
    vSemaphoreDelete(app->started);
  if (app->stopped)
    vSemaphoreDelete(app->stopped);
  m5_pocket_bridge_set_request_handler(NULL, NULL);
  free(app);
  atomic_store(&running, false);
}

esp_err_t m5_pocket_app_start(const m5_pocket_app_config *config,
                              m5_pocket_app **out) {
  if (!config || config->struct_size < sizeof(*config) || !out ||
      !config->image ||
      config->image->struct_size < sizeof(m5_pocket_app_image))
    return ESP_ERR_INVALID_ARG;
  if (config->image->mode != (uint32_t)m5_pocket_app_build_mode()) {
    ESP_LOGE(TAG, "the embedded image was built for another mode");
    return ESP_ERR_INVALID_ARG;
  }
  bool expected = false;
  if (!atomic_compare_exchange_strong(&running, &expected, true))
    return ESP_ERR_INVALID_STATE;
  m5_pocket_app *app = calloc(1, sizeof(*app));
  if (!app) {
    atomic_store(&running, false);
    return ESP_ERR_NO_MEM;
  }
  app->config = *config;
  m5_pocket_app_config *own = &app->config;
  if (!own->heap_limit)
    own->heap_limit = 1024 * 1024;
  if (!own->task_stack_bytes)
    own->task_stack_bytes = CONFIG_M5_POCKET_APP_TASK_STACK;
  atomic_init(&app->stop, false);
  esp_err_t err = create_display(app);
  if (err == ESP_OK)
    err = create_renderer(app);
  if (err == ESP_OK)
    err = create_inputs(app);
  if (err == ESP_OK) {
    app->started = xSemaphoreCreateBinary();
    app->stopped = xSemaphoreCreateBinary();
    if (!app->started || !app->stopped)
      err = ESP_ERR_NO_MEM;
  }
  if (err != ESP_OK) {
    release(app);
    return err;
  }
  m5_pocket_bridge_set_request_handler(own->on_request, own->hook_context);
  const BaseType_t core =
      own->task_core < 0 ? tskNO_AFFINITY : (BaseType_t)own->task_core;
  const UBaseType_t priority = own->task_priority
                                   ? (UBaseType_t)own->task_priority
                                   : CONFIG_M5_POCKET_APP_TASK_PRIORITY;
  BaseType_t created =
      own->task_stack_caps
          ? xTaskCreatePinnedToCoreWithCaps(
                run, "m5_pocket_app", own->task_stack_bytes, app, priority,
                &app->task, core, own->task_stack_caps)
          : xTaskCreatePinnedToCore(run, "m5_pocket_app", own->task_stack_bytes,
                                    app, priority, &app->task, core);
  if (created != pdPASS) {
    app->task = NULL;
    release(app);
    return ESP_ERR_NO_MEM;
  }
  xSemaphoreTake(app->started, portMAX_DELAY);
  if (app->start_result != ESP_OK) {
    err = app->start_result;
    xSemaphoreTake(app->stopped, portMAX_DELAY);
    release(app);
    return err;
  }
  ESP_LOGI(TAG, "%s: %s mode, %" PRIu32 " Hz, display buffers %u bytes",
           config->image->name,
           m5_pocket_app_build_mode() == M5_POCKET_APP_MODE_AOT ? "AOT" : "JS",
           app->cadence_hz, (unsigned)app->devices.display_bytes);
  *out = app;
  return ESP_OK;
}

esp_err_t m5_pocket_app_stop(m5_pocket_app *app) {
  if (!app)
    return ESP_ERR_INVALID_ARG;
  atomic_store(&app->stop, true);
  xTaskNotifyGive(app->task);
  xSemaphoreTake(app->stopped, portMAX_DELAY);
  release(app);
  return ESP_OK;
}

esp_err_t m5_pocket_app_get_stats(m5_pocket_app *app,
                                  m5_pocket_app_stats *out) {
  if (!app || !out)
    return ESP_ERR_INVALID_ARG;
  *out = app->stats;
  return ESP_OK;
}

pocket_runtime *m5_pocket_app_runtime(m5_pocket_app *app) {
  return app ? m5_pocket_backend_runtime(app) : NULL;
}
