/* M5Stack CoreP4X factory demo. The board is brought up here; m5_pocket_app
 * runs apps/corep4x-factory in the mode Kconfig selects (JS by default). The
 * 480x480 picture is presented turned 180 degrees onto the ST7102 panel,
 * which is mounted upside down: the orientation the factory firmware gets
 * from LV_DISPLAY_ROTATION_180. The panel has two frame buffers: a frame is
 * written to the one the panel does not read and shown at the panel's next
 * refresh, and the refreshes are the application's cadence. */
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

/* A refresh the cadence waits for at most: six periods of the panel. */
#define COREP4X_REFRESH_TIMEOUT_MS 100U

typedef struct {
  m5_corep4x_display_t lcd;
  /* The frame buffer the panel reads at start, then the other one. */
  uint16_t *framebuffers[2];
  /* The refresh count after which the panel reads the frame buffer shown
   * last (m5_corep4x_display_show). */
  uint32_t shown_after;
  /* The refresh count the cadence returned last. */
  uint32_t refreshes;
  bool paced;
  esp_lcd_touch_handle_t touch;
} corep4x_board_t;

static corep4x_board_t board;

/* The factory app_main order: board power, the panel with its PSRAM frame
 * buffers cleared to black, the touch controller, then the backlight. The
 * factory has one frame buffer. */
static void start_board(void) {
  ESP_ERROR_CHECK(m5_corep4x_init());
  ESP_ERROR_CHECK(m5_corep4x_display_new(2, &board.lcd));
  void *framebuffers[2] = {NULL, NULL};
  ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(
      board.lcd.panel, 2, &framebuffers[0], &framebuffers[1]));
  const size_t frame_bytes =
      (size_t)M5_COREP4X_LCD_WIDTH * M5_COREP4X_LCD_HEIGHT * sizeof(uint16_t);
  for (size_t i = 0; i < 2; i++) {
    board.framebuffers[i] = framebuffers[i];
    memset(framebuffers[i], 0, frame_bytes);
    esp_cache_msync(framebuffers[i], frame_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  }
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

/* The page flip of the DPI presenter, on the app task: core 0, where the
 * panel was created. */
static bool show_framebuffer(void *context, const uint16_t *framebuffer) {
  corep4x_board_t *b = context;
  return m5_corep4x_display_show(&b->lcd, framebuffer, &b->shown_after) ==
         ESP_OK;
}

static bool wait_shown(void *context, uint32_t timeout_ms) {
  corep4x_board_t *b = context;
  return m5_corep4x_display_wait_refresh(b->shown_after, timeout_ms);
}

/* The cadence: the panel's next refresh, and the refreshes since the last
 * one this returned. */
static uint32_t wait_refresh(void *context) {
  (void)context;
  const uint32_t before = m5_corep4x_display_refreshes();
  if (!board.paced) {
    board.paced = true;
    board.refreshes = before;
  }
  if (!m5_corep4x_display_wait_refresh(before, COREP4X_REFRESH_TIMEOUT_MS)) {
    return 0;
  }
  const uint32_t now = m5_corep4x_display_refreshes();
  const uint32_t passed = now - board.refreshes;
  board.refreshes = now;
  return passed;
}

/* The DPI presenter: the whole 480x480 picture turned 180 degrees by the
 * PPA, strips of whole raster rows, into the frame buffer the panel does
 * not read. */
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
      .framebuffer = board.framebuffers[0],
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
      .back_framebuffer = board.framebuffers[1],
      .flip = {.show = show_framebuffer,
               .wait_shown = wait_shown,
               .context = &board},
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
  /* A frame that takes more than a refresh of the panel misses slots; they
   * still advance the application. */
  config.catch_up_frames = 4;
  config.wait_refresh = wait_refresh;
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
