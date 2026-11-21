/* m5_pocket_app smoke program: the tiny app of ../app runs for a scripted
 * number of frames on a transfer-only display, with scripted button and touch
 * input and bridge traffic in both directions, then stops and reports. */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_pocket/app.h"
#include "pocket_spec.h"
#include "transfer_display.h"
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>

static const char *TAG = "m5_aot_smoke";
#define SMOKE_FRAMES 120

typedef struct {
  atomic_uint frame;
  atomic_uint requests;
} smoke_state;

static uint32_t buttons(void *context) {
  smoke_state *state = context;
  uint32_t frame = atomic_load(&state->frame);
  /* CROSS held for two frames, twice: the counter's ActionHandler. */
  return (frame >= 10 && frame < 12) || (frame >= 30 && frame < 32)
             ? POCKET_BTN_CROSS
             : 0;
}

static bool touch(void *context, float *x, float *y) {
  smoke_state *state = context;
  uint32_t frame = atomic_load(&state->frame);
  /* A tap on the "Taps" button at the centre of the 240x135 viewport. */
  *x = 120;
  *y = 67;
  return frame >= 50 && frame < 54;
}

static void before_frame(void *context) {
  smoke_state *state = context;
  uint32_t frame = atomic_fetch_add(&state->frame, 1);
  m5_pocket_bridge_set_number("battery", 100 - (frame / 10) % 100);
  m5_pocket_bridge_set_bool("charging", (frame / 30) % 2);
  char clock[8];
  snprintf(clock, sizeof(clock), "12:%02" PRIu32, (frame / 30) % 60);
  m5_pocket_bridge_set_text("clock", clock);
  float spectrum[16];
  for (int i = 0; i < 16; i++)
    spectrum[i] = (float)((frame + i) % 16) / 16.0f;
  m5_pocket_bridge_set_array("spectrum", spectrum, 16);
  if (frame == 40)
    m5_pocket_bridge_post_event("alarm", 1);
}

static void on_request(void *context, const m5_pocket_request *request) {
  smoke_state *state = context;
  atomic_fetch_add(&state->requests, 1);
  ESP_LOGI(TAG, "request %.*s argc=%u arg0=%g", (int)request->name_length,
           request->name, (unsigned)request->argc,
           request->argc ? request->args[0] : 0.0);
}

void app_main(void) {
  static smoke_state state;
  const m5_pocket_app_image *image = m5_pocket_app_embedded();
  static transfer_display display;
  ESP_ERROR_CHECK(transfer_display_init(&display, image->physical_width,
                                        image->physical_height,
                                        16) == POCKET_STATUS_OK
                      ? ESP_OK
                      : ESP_ERR_NO_MEM);
  const size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

  m5_pocket_app_config config;
  m5_pocket_app_config_defaults(&config);
  config.display.kind = M5_POCKET_DISPLAY_CUSTOM;
  config.display.custom = transfer_display_vtable();
  config.display.custom_context = &display;
  config.display.custom_buffer_bytes = display.capacity * 2;
#if CONFIG_IDF_TARGET_ESP32P4
  config.display.ppa = true;
#endif
  config.input.context = &state;
  config.input.buttons = buttons;
  config.input.touch = touch;
  config.heap_limit = 512 * 1024;
#if CONFIG_SPIRAM
  config.js_prefer_psram = true;
#else
  config.js_heap_limit = 160 * 1024;
#endif
  config.before_frame = before_frame;
  config.on_request = on_request;
  config.hook_context = &state;

  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
  while (atomic_load(&state.frame) < SMOKE_FRAMES)
    vTaskDelay(pdMS_TO_TICKS(50));
  m5_pocket_app_stats stats;
  m5_pocket_app_get_stats(app, &stats);
  const size_t internal_running = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const size_t internal_minimum =
      heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  ESP_ERROR_CHECK(m5_pocket_app_stop(app));
  const bool pass = stats.frame_errors == 0 &&
                    atomic_load(&state.requests) >= 3 &&
                    display.flushes == display.waits && display.flushes > 0;
  ESP_LOGI(TAG,
           "%s mode=%s frames=%" PRIu32 " errors=%" PRIu32
           " max_frame_us=%" PRIu32 " stack_free_min=%" PRIu32
           " requests=%u flushes=%" PRIu32 " hash=%016" PRIx64,
           pass ? "PASS" : "FAIL",
           m5_pocket_app_build_mode() == M5_POCKET_APP_MODE_AOT ? "aot" : "js",
           stats.frames, stats.frame_errors, stats.max_frame_us,
           stats.stack_free_min, atomic_load(&state.requests), display.flushes,
           display.hash);
  ESP_LOGI(TAG,
           "internal heap: free before start %u, while running %u, minimum %u",
           (unsigned)internal_before, (unsigned)internal_running,
           (unsigned)internal_minimum);
  transfer_display_destroy(&display);
}
