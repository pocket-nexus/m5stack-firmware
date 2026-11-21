/* M5Stack Tab5 factory demo. The board is brought up here; m5_pocket_app runs
 * apps/tab5-factory in the mode Kconfig selects (JS by default). The
 * landscape picture (1280x720 raster, 640x360 logical) is presented 90
 * degrees counter-clockwise onto the 720x1280 MIPI-DSI panel, the
 * orientation the factory firmware gets from LV_DISPLAY_ROTATION_90. */
#include "esp_async_memcpy.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5/tab5.h"
#include "m5_pocket/app.h"
#include "services.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

static const char *TAG = "tab5";

/* Each of the two internal-RAM staging strips. */
#define TAB5_STRIP_BYTES (128U * 1024U)
/* GDMA copies queued at once: strip presents plus retained moves. */
#define TAB5_COPY_BACKLOG 12U
#define TAB5_GUEST_HEAP (24U * 1024U * 1024U)
/* Evaluating a deep Solid tree recurses through the guest; the app task's
 * stack is in PSRAM and QuickJS throws a RangeError before the guard page. */
#define TAB5_TASK_STACK (192U * 1024U)
#define TAB5_GUEST_STACK_LIMIT (144U * 1024U)
#define TAB5_STATS_PERIOD_MS 5000

typedef struct {
  m5_tab5_display_t lcd;
  uint16_t *framebuffer;
  esp_lcd_touch_handle_t touch;
  float unit; /* logical pixels per raster pixel */
} tab5_board_t;

static tab5_board_t board;

/* The factory HAL's power-up: quick charge and charging on, the touch
 * controller reset, then the panel with one PSRAM frame buffer. */
static void start_board(void) {
  ESP_ERROR_CHECK(m5_tab5_init());
  ESP_ERROR_CHECK(m5_tab5_set_switch(M5_TAB5_SWITCH_QUICK_CHARGE, true));
  vTaskDelay(pdMS_TO_TICKS(50));
  ESP_ERROR_CHECK(m5_tab5_set_switch(M5_TAB5_SWITCH_CHARGE, true));
  ESP_ERROR_CHECK(m5_tab5_touch_reset());
  ESP_ERROR_CHECK(m5_tab5_display_new(1, &board.lcd));
  void *framebuffer = NULL;
  ESP_ERROR_CHECK(
      esp_lcd_dpi_panel_get_frame_buffer(board.lcd.panel, 1, &framebuffer));
  board.framebuffer = framebuffer;
  const size_t frame_bytes =
      (size_t)M5_TAB5_LCD_WIDTH * M5_TAB5_LCD_HEIGHT * sizeof(uint16_t);
  memset(framebuffer, 0xff, frame_bytes);
  esp_cache_msync(framebuffer, frame_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  ESP_ERROR_CHECK(m5_tab5_backlight_set(100));
  ESP_ERROR_CHECK(m5_tab5_touch_new(&board.touch));
  /* Panel (x, y) lands at picture (1279 - y, x): mirror y, then swap. */
  ESP_ERROR_CHECK(esp_lcd_touch_set_mirror_y(board.touch, true));
  ESP_ERROR_CHECK(esp_lcd_touch_set_swap_xy(board.touch, true));
}

/* One contact in logical pixels. */
static bool read_touch(void *context, float *x, float *y) {
  tab5_board_t *b = context;
  esp_lcd_touch_point_data_t point;
  uint8_t count = 0;
  if (esp_lcd_touch_read_data(b->touch) != ESP_OK ||
      esp_lcd_touch_get_data(b->touch, &point, &count, 1) != ESP_OK ||
      count == 0)
    return false;
  *x = point.x * b->unit;
  *y = point.y * b->unit;
  return true;
}

/* The DPI presenter: strips of whole raster rows spanning 128-byte cache
 * lines, at least 8 rows tall for a turned picture
 * (pocket_display_esp_dpi). */
static void display_config(m5_pocket_display_config *display, uint32_t width,
                           uint32_t height) {
  async_memcpy_handle_t copy = NULL;
  async_memcpy_config_t copy_config = ASYNC_MEMCPY_DEFAULT_CONFIG();
  copy_config.backlog = TAB5_COPY_BACKLOG;
  copy_config.dma_burst_size = 64;
  ESP_ERROR_CHECK(esp_async_memcpy_install_gdma_axi(&copy_config, &copy));
  uint32_t line_rows = 1U;
  while ((line_rows * width * 2U) % 128U != 0U)
    ++line_rows;
  uint32_t quantum = line_rows;
  while (quantum % 8U != 0U || quantum % line_rows != 0U)
    ++quantum;
  assert(height % quantum == 0U);
  const uint32_t strip_rows =
      TAB5_STRIP_BYTES / (width * 2U) / quantum * quantum;
  assert(strip_rows >= quantum);
  display->kind = M5_POCKET_DISPLAY_ESP_DPI;
  display->dpi = (pocket_display_esp_dpi_config){
      .framebuffer = board.framebuffer,
      .panel_width = M5_TAB5_LCD_WIDTH,
      .panel_height = M5_TAB5_LCD_HEIGHT,
      .role = POCKET_DISPLAY_ROLE_PRIMARY,
      .width = width,
      .height = height,
      .turned = true,
      .scale = 1,
      .strip_rows = strip_rows,
      .row_alignment = quantum,
      .frame_buffer_budget = (size_t)width * 2U * strip_rows * 2U,
      .copy = copy,
      .move_copy = copy,
  };
  display->ppa = true;
  display->ppa_mask_budget = TAB5_STRIP_BYTES / 2U;
  ESP_LOGI(TAG,
           "picture %" PRIu32 "x%" PRIu32 " turned, strips of %" PRIu32 " rows",
           width, height, strip_rows);
}

void app_main(void) {
  start_board();
  const tab5_services_config_t services = {
      .touch = board.touch,
      .panel_name = m5_tab5_panel_name(board.lcd.kind),
  };
  tab5_services_start(&services);

  m5_pocket_app_config config;
  m5_pocket_app_config_defaults(&config);
  /* The picture in raster pixels: the logical viewport times the raster
   * density. The image's physical size is the portrait panel. */
  const uint32_t density =
      config.image->physical_height / config.image->logical_width;
  board.unit = 1.0f / (float)density;
  display_config(&config.display, config.image->logical_width * density,
                 config.image->logical_height * density);
  config.input.context = &board;
  config.input.touch = read_touch;
  config.heap_limit = TAB5_GUEST_HEAP;
  config.js_heap_limit = TAB5_GUEST_HEAP;
  config.js_stack_limit = TAB5_GUEST_STACK_LIMIT;
  config.js_prefer_psram = true;
  config.task_stack_bytes = TAB5_TASK_STACK;
  config.task_stack_caps = MALLOC_CAP_SPIRAM;
  /* The DSI interrupts were allocated here, on core 0; the app task joins
   * them and leaves core 1 to sound and capture. */
  config.task_core = 0;
  config.before_frame = tab5_services_publish;
  config.on_request = tab5_services_request;
  m5_pocket_app *app = NULL;
  ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));

  m5_pocket_app_stats last = {0};
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(TAB5_STATS_PERIOD_MS));
    m5_pocket_app_stats stats;
    if (m5_pocket_app_get_stats(app, &stats) != ESP_OK)
      continue;
    ESP_LOGI(TAG,
             "fps=%.1f skipped=%" PRIu32 " errors=%" PRIu32
             " frame_max_ms=%.2f stack_free=%" PRIu32
             " psram_free=%u internal_free=%u",
             (stats.frames - last.frames) * 1000.0 / TAB5_STATS_PERIOD_MS,
             stats.frames_skipped - last.frames_skipped,
             stats.frame_errors - last.frame_errors,
             stats.max_frame_us / 1000.0, stats.stack_free_min,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    last = stats;
  }
}
