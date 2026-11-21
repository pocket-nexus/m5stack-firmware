/*
 * The application host: runs the one embedded application of a program in
 * the mode the Kconfig choice selects, with the board's display and inputs.
 *
 *   CONFIG_M5_POCKET_APP_MODE_JS   bundle in the QuickJS guest
 *   CONFIG_M5_POCKET_APP_MODE_AOT  MicroTS-compiled Rust static library
 *
 * The program owns board bring-up (power, buses, panel, touch, sensors) and
 * passes handles and callbacks here. m5_pocket_app_start creates the display
 * adapter, the renderer and the input sources, then runs the runtime on a
 * task of its own. The same C display, renderer and input objects serve both
 * modes: the AOT library adapts their vtables to its Rust traits.
 */
#pragma once
#include "esp_err.h"
#include "m5_pocket/bridge.h"
#include "pocket/display_esp_lcd.h"
#include "pocket_drivers.h"
#include "pocket_runtime.h"
#include "sdkconfig.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if CONFIG_IDF_TARGET_ESP32P4
#include "pocket/display_esp_dpi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  M5_POCKET_APP_MODE_JS = 1,
  M5_POCKET_APP_MODE_AOT = 2,
} m5_pocket_app_mode;

/* The application the m5_pocket_app() CMake helper embedded into the calling
 * component, with the host contract it was built for. */
typedef struct {
  size_t struct_size;
  const char *name;
  uint32_t mode;         /* m5_pocket_app_mode */
  const uint8_t *bundle; /* JS mode: the .pkb bytes; NULL in AOT mode */
  size_t bundle_size;
  const pocket_profile_facts *facts;
  const uint8_t *profile_hash; /* 32 bytes */
  uint32_t host_abi;
  uint32_t tick_hz; /* highest cadence of the host contract */
  uint32_t physical_width, physical_height;
  uint32_t logical_width, logical_height; /* the application's viewport */
} m5_pocket_app_image;

/* Defined by the source the m5_pocket_app() helper generates into the
 * calling component. */
const m5_pocket_app_image *m5_pocket_app_embedded(void);

typedef enum {
  /* SPI/QSPI/I80 panel with GRAM: pocket_display_esp_lcd (RGB565 strips). */
  M5_POCKET_DISPLAY_ESP_LCD = 1,
  /* ESP32-P4 MIPI-DSI DPI framebuffer: pocket_display_esp_dpi. */
  M5_POCKET_DISPLAY_ESP_DPI = 2,
  /* A pocket_display_output_v1 the program created and keeps alive. */
  M5_POCKET_DISPLAY_CUSTOM = 3,
} m5_pocket_display_kind;

typedef struct {
  uint32_t kind; /* m5_pocket_display_kind */
  /* M5_POCKET_DISPLAY_ESP_LCD. struct_size is set by the host. */
  pocket_display_esp_lcd_config lcd;
#if CONFIG_IDF_TARGET_ESP32P4
  /* M5_POCKET_DISPLAY_ESP_DPI. struct_size is set by the host. */
  pocket_display_esp_dpi_config dpi;
#endif
  /* M5_POCKET_DISPLAY_CUSTOM: vtable, context and the pixel bytes it holds
   * (passed as the runtime's frame_buffer_budget). */
  const pocket_display_output_v1 *custom;
  void *custom_context;
  size_t custom_buffer_bytes;
#if CONFIG_IDF_TARGET_ESP32P4
  /* Register the P4 PPA renderer for this display (software otherwise). */
  bool ppa;
  /* Glyph mask budget of each PPA renderer (0: half the display buffer
   * bytes). */
  size_t ppa_mask_budget;
  /* 1 (or 0): one PPA renderer. 2: two renderers on two PPA devices behind
   * pocketjs_esp32p4_ppa_lanes, which split every strip: the app task
   * renders the upper rows, a helper task pinned to ppa_helper_core the
   * lower rows, at the same time. */
  uint32_t ppa_lanes;
  int ppa_helper_core; /* 0 or 1; m5_pocket_app_config_defaults sets 1 */
  /* Raster-row quantum of the split (0: dpi.row_alignment for
   * M5_POCKET_DISPLAY_ESP_DPI, else 1). */
  uint32_t ppa_row_alignment;
#endif
} m5_pocket_display_config;

/* Input callbacks run on the app task once per frame, before the frame.
 * Every callback is optional. */
typedef struct {
  void *context;
  /* POCKET_BTN_* bits held now (pocket_spec.h). */
  uint32_t (*buttons)(void *context);
  /* One contact in logical pixels; true while touched. The host publishes
   * the bridge values pointerX, pointerY (numbers) and pointerDown (0/1)
   * from it in both modes. */
  bool (*touch)(void *context, float *x, float *y);
  /* Relative axis deltas in signed millidegrees, axes 0 (primary) and 1
   * (secondary). */
  void (*axes)(void *context, int32_t deltas[2]);
  /* A fused motion estimate (input.motion.* capabilities). Fill the record
   * and point its members at storage valid until the next call; return
   * false when there is no new estimate. struct_size is set by the host. */
  bool (*motion)(void *context, pocket_motion_state *state);
  /* Further input sources, for example pocket_input_gpio. Their storage
   * stays live until m5_pocket_app_stop returns. */
  const pocket_input_source_v1 *const *sources;
  void *const *source_contexts;
  size_t source_count;
} m5_pocket_input_config;

typedef struct {
  uint32_t frames;
  uint32_t frame_errors;
  uint32_t frames_skipped;
  uint32_t max_frame_us;
  uint32_t last_frame_us;
  uint32_t stack_free_min; /* bytes of the app task stack never used */
} m5_pocket_app_stats;

typedef struct m5_pocket_app m5_pocket_app;

typedef struct {
  size_t struct_size;
  /* m5_pocket_app_embedded(). */
  const m5_pocket_app_image *image;
  m5_pocket_display_config display;
  m5_pocket_input_config input;
  /* Runtime heap limit in bytes (0: 1 MiB). */
  size_t heap_limit;
  /* JS mode guest limits (0: 2 MiB heap, 64 KiB stack). The QuickJS stack
   * limit must stay below task_stack_bytes. */
  size_t js_heap_limit;
  size_t js_stack_limit;
  bool js_prefer_psram;
  /* App task (0: Kconfig defaults). task_stack_caps 0 is internal RAM;
   * MALLOC_CAP_SPIRAM puts the stack in PSRAM (no flash writes from it). */
  uint32_t task_stack_bytes;
  uint32_t task_stack_caps;
  int task_priority;
  int task_core; /* -1: no affinity */
  /* Called on the app task before every frame: poll sensors, publish
   * bridge values. */
  void (*before_frame)(void *context);
  /* Called on the app task for each request the application made. */
  m5_pocket_request_fn on_request;
  /* Called on the app task after every frame with its PocketJS status. */
  void (*after_frame)(void *context, int32_t status);
  void *hook_context;
} m5_pocket_app_config;

/* The mode this firmware was built in. */
m5_pocket_app_mode m5_pocket_app_build_mode(void);

/* Fill defaults: struct_size, the embedded image and the app task settings
 * from Kconfig. The display and inputs remain for the program to fill.
 * Inline, so that the call to m5_pocket_app_embedded stays in the component
 * the m5_pocket_app() helper generated it for. */
static inline void m5_pocket_app_config_defaults(m5_pocket_app_config *config) {
  *config = (m5_pocket_app_config){
      .struct_size = sizeof(*config),
      .image = m5_pocket_app_embedded(),
      .heap_limit = 1024 * 1024,
      .js_heap_limit = 2 * 1024 * 1024,
      .js_stack_limit = 64 * 1024,
      .task_stack_bytes = CONFIG_M5_POCKET_APP_TASK_STACK,
      .task_priority = CONFIG_M5_POCKET_APP_TASK_PRIORITY,
      .task_core = -1,
  };
#if CONFIG_IDF_TARGET_ESP32P4
  config->display.ppa_helper_core = 1;
#endif
}

/* Create the display, renderer and inputs, boot the application and start
 * the app task. Returns after the first frame is presented or boot failed.
 * ESP_ERR_INVALID_ARG: bad configuration; ESP_ERR_NO_MEM: allocation;
 * ESP_ERR_INVALID_STATE: an app is already running; ESP_FAIL: runtime
 * creation or boot failed (see the log). */
esp_err_t m5_pocket_app_start(const m5_pocket_app_config *config,
                              m5_pocket_app **out);

/* Stop the app task, then destroy the runtime, renderer, display adapter and
 * input sources, in that order. Panel and touch handles stay the program's. */
esp_err_t m5_pocket_app_stop(m5_pocket_app *app);

esp_err_t m5_pocket_app_get_stats(m5_pocket_app *app, m5_pocket_app_stats *out);

#ifdef __cplusplus
}
#endif
