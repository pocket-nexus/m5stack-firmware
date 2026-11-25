/* M5Stack CoreP4X factory demo. The board is brought up here; m5_pocket_app
 * runs apps/corep4x-factory in the mode Kconfig selects (JS by default). The
 * 480x480 picture is presented turned 180 degrees onto the ST7102 panel,
 * which is mounted upside down: the orientation the factory firmware gets
 * from LV_DISPLAY_ROTATION_180. */
#include "esp_async_memcpy.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5_corep4x/bsp.h"
#include "m5_pocket/app.h"
#include "services.h"
#include "svc.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

static const char *TAG = "corep4x";

/* Raster rows of each internal-RAM staging strip: 80 rows of 480 px are
 * 76,800 bytes, six strips a frame. A strip holds a multiple of the row
 * quantum below. */
#define COREP4X_STRIP_ROWS 80U
/* A presented block spans whole 128-byte cache lines (two 960-byte rows)
 * and is at least 8 raster rows tall, the bound pocket_display_esp_dpi
 * measured for rotated PPA presents. */
#define COREP4X_ROW_QUANTUM 8U
/* GDMA copies queued at once: retained moves. */
#define COREP4X_COPY_BACKLOG 12U
#define COREP4X_GUEST_HEAP (16U * 1024U * 1024U)
/* Evaluating a deep Solid tree recurses through the guest; the app task's
 * stack is in PSRAM and QuickJS throws a RangeError before the guard page. */
#define COREP4X_TASK_STACK (192U * 1024U)
#define COREP4X_GUEST_STACK_LIMIT (144U * 1024U)
#define COREP4X_STATS_PERIOD_MS 5000

typedef struct {
  m5_corep4x_display_t lcd;
  uint16_t *framebuffer;
  esp_lcd_touch_handle_t touch;
} corep4x_board_t;

static corep4x_board_t board;

/* The factory app_main order: board power, the panel with one PSRAM frame
 * buffer cleared to black, the touch controller, then the backlight. */
static void start_board(void) {
  ESP_ERROR_CHECK(m5_corep4x_init());
  ESP_ERROR_CHECK(m5_corep4x_display_new(1, &board.lcd));
  void *framebuffer = NULL;
  ESP_ERROR_CHECK(
      esp_lcd_dpi_panel_get_frame_buffer(board.lcd.panel, 1, &framebuffer));
  board.framebuffer = framebuffer;
  const size_t frame_bytes =
      (size_t)M5_COREP4X_LCD_WIDTH * M5_COREP4X_LCD_HEIGHT * sizeof(uint16_t);
  memset(framebuffer, 0, frame_bytes);
  esp_cache_msync(framebuffer, frame_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  if (m5_corep4x_touch_new(&board.touch) != ESP_OK) {
    ESP_LOGE(TAG, "no touch controller");
    board.touch = NULL;
  }
  ESP_ERROR_CHECK(m5_corep4x_backlight_set(100));
}

/* The first contact in logical pixels. The controller is read once per
 * frame here; the services read the same contacts (svc_contacts). */
static bool read_touch(void *context, float *x, float *y) {
  corep4x_board_t *b = context;
  if (!b->touch || esp_lcd_touch_read_data(b->touch) != ESP_OK) {
    return false;
  }
  uint16_t xs[1];
  uint16_t ys[1];
  if (svc_contacts(xs, ys, 1) == 0) {
    return false;
  }
  *x = xs[0];
  *y = ys[0];
  return true;
}

/* The DPI presenter: the whole 480x480 picture turned 180 degrees by the
 * PPA, strips of whole raster rows. */
static void display_config(m5_pocket_display_config *display, uint32_t width,
                           uint32_t height) {
  async_memcpy_handle_t copy = NULL;
  async_memcpy_config_t copy_config = ASYNC_MEMCPY_DEFAULT_CONFIG();
  copy_config.backlog = COREP4X_COPY_BACKLOG;
  copy_config.dma_burst_size = 64;
  ESP_ERROR_CHECK(esp_async_memcpy_install_gdma_axi(&copy_config, &copy));
  assert(width == M5_COREP4X_LCD_WIDTH && height == M5_COREP4X_LCD_HEIGHT);
  assert(height % COREP4X_ROW_QUANTUM == 0U &&
         COREP4X_STRIP_ROWS % COREP4X_ROW_QUANTUM == 0U);
  display->kind = M5_POCKET_DISPLAY_ESP_DPI;
  display->dpi = (pocket_display_esp_dpi_config){
      .framebuffer = board.framebuffer,
      .panel_width = M5_COREP4X_LCD_WIDTH,
      .panel_height = M5_COREP4X_LCD_HEIGHT,
      .role = POCKET_DISPLAY_ROLE_PRIMARY,
      .width = width,
      .height = height,
      .half_turn = true,
      .scale = 1,
      .strip_rows = COREP4X_STRIP_ROWS,
      .row_alignment = COREP4X_ROW_QUANTUM,
      .frame_buffer_budget = (size_t)width * 2U * COREP4X_STRIP_ROWS * 2U,
      .copy = copy,
      .move_copy = copy,
  };
  /* Two PPA renderers split every strip at a multiple of the row quantum:
   * the app task (core 0) renders the upper rows, a helper on core 1 the
   * lower ones. */
  display->ppa = true;
  display->ppa_lanes = 2;
  display->ppa_helper_core = 1;
  display->ppa_row_alignment = COREP4X_ROW_QUANTUM;
  ESP_LOGI(TAG,
           "picture %" PRIu32 "x%" PRIu32 " half-turned, strips of %u rows",
           width, height, (unsigned)COREP4X_STRIP_ROWS);
}

void app_main(void) {
  start_board();
  services_start(board.touch, &board.lcd);

  m5_pocket_app_config config;
  m5_pocket_app_config_defaults(&config);
  assert(config.image->facts && config.image->facts->display);
  const uint32_t density = config.image->facts->display->raster_density;
  assert(density == 1U);
  display_config(&config.display, config.image->logical_width * density,
                 config.image->logical_height * density);
  config.input.context = &board;
  config.input.touch = read_touch;
  config.heap_limit = COREP4X_GUEST_HEAP;
  /* A swipe or a window's motion repaints most of the panel in more than a
   * 60 Hz slot; the skipped slots still advance the application. */
  config.catch_up_frames = 4;
  config.js_heap_limit = COREP4X_GUEST_HEAP;
  config.js_stack_limit = COREP4X_GUEST_STACK_LIMIT;
  config.js_prefer_psram = true;
  config.task_stack_bytes = COREP4X_TASK_STACK;
  config.task_stack_caps = MALLOC_CAP_SPIRAM;
  /* The DSI interrupts were allocated here, on core 0; the app task joins
   * them and leaves core 1 to audio and the PPA helper. */
  config.task_core = 0;
  config.before_frame = services_publish;
  config.on_request = services_request;
  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
  services_set_app(app);

  m5_pocket_app_stats last = {0};
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(COREP4X_STATS_PERIOD_MS));
    m5_pocket_app_stats stats;
    if (m5_pocket_app_get_stats(app, &stats) != ESP_OK) {
      continue;
    }
    ESP_LOGI(TAG,
             "fps=%.1f skipped=%" PRIu32 " errors=%" PRIu32
             " frame_max_ms=%.2f stack_free=%" PRIu32
             " psram_free=%u internal_free=%u",
             (stats.frames - last.frames) * 1000.0 / COREP4X_STATS_PERIOD_MS,
             stats.frames_skipped - last.frames_skipped,
             stats.frame_errors - last.frame_errors,
             stats.max_frame_us / 1000.0, stats.stack_free_min,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    last = stats;
  }
}
