/* TOUCH window: the full-screen test canvas of ui_function_touch_window.cpp
 * drawn here and uploaded as the texture "assets/touch-canvas.png" (5x5
 * grid, strokes in random palette colours, a cell turns dark when a stroke
 * first enters it), the controller status lines and the global touch
 * indicator preference. */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "m5_corep4x/bsp.h"
#include "svc.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "svc_canvas";

#define TEXTURE "assets/touch-canvas.png"
#define SIDE 512
#define CANVAS 480
#define CELLS 5
#define PITCH 96
#define LINE 3
#define CELL_INSET 3
#define CELL_RADIUS 8
#define MATCH_DISTANCE 120
#define POINTERS 5
#define UPLOAD_US 50000
#define CHIP_CST3530 0xCACA220Eu

typedef struct {
  bool active;
  bool seen;
  int x;
  int y;
  uint16_t color;
} pointer_t;

static uint16_t *s_pixels; /* SIDE x SIDE, PSM_5650 */
static bool s_open;
static bool s_paused;
static bool s_dirty;
static int64_t s_uploaded_us;
static bool s_visited[CELLS * CELLS];
static int s_visited_count;
static pointer_t s_pointers[POINTERS];

static const uint32_t PALETTE[] = {0xEF476F, 0xFFD166, 0x06D6A0, 0x118AB2,
                                   0x9B5DE5, 0xF15BB5, 0x00BBF9, 0xF78C6B,
                                   0x7DD87D, 0xFF8FAB};

static inline void put(int x, int y, uint16_t color) {
  if (x >= 0 && x < CANVAS && y >= 0 && y < CANVAS) {
    s_pixels[y * SIDE + x] = color;
  }
}

static void fill_rect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      put(x, y, color);
    }
  }
}

/* draw_canvas_grid_into_buffer: white, then 3 px lines at i * 96 and at
 * 477 for the last one. */
static void draw_grid(void) {
  const uint16_t white = svc_color(0xFFFFFF);
  const uint16_t grid = svc_color(0xCBD5E1);
  for (int i = 0; i < SIDE * SIDE; i++) {
    s_pixels[i] = white;
  }
  for (int i = 0; i <= CELLS; i++) {
    const int at = i < CELLS ? i * PITCH : CANVAS - LINE;
    fill_rect(at, 0, at + LINE - 1, CANVAS - 1, grid);
    fill_rect(0, at, CANVAS - 1, at + LINE - 1, grid);
  }
}

/* fill_canvas_cell: a radius-8 rounded rectangle inside the grid lines. */
static void fill_cell(int col, int row) {
  const uint16_t dark = svc_color(0x1E293B);
  const int x0 = col * PITCH + CELL_INSET;
  const int y0 = row * PITCH + CELL_INSET;
  const int x1 = col == CELLS - 1 ? CANVAS - 1 - LINE : (col + 1) * PITCH - 1;
  const int y1 = row == CELLS - 1 ? CANVAS - 1 - LINE : (row + 1) * PITCH - 1;
  const int r = CELL_RADIUS;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      const int cx = x < x0 + r ? x0 + r : x > x1 - r ? x1 - r : x;
      const int cy = y < y0 + r ? y0 + r : y > y1 - r ? y1 - r : y;
      const int dx = x - cx;
      const int dy = y - cy;
      if (dx * dx + dy * dy <= r * r) {
        put(x, y, dark);
      }
    }
  }
}

static void visit(int x, int y) {
  int col = x * CELLS / CANVAS;
  int row = y * CELLS / CANVAS;
  col = col < 0 ? 0 : col >= CELLS ? CELLS - 1 : col;
  row = row < 0 ? 0 : row >= CELLS ? CELLS - 1 : row;
  if (!s_visited[row * CELLS + col]) {
    s_visited[row * CELLS + col] = true;
    s_visited_count++;
    fill_cell(col, row);
  }
}

/* draw_canvas_brush_stamp: the pixels with dx^2 + dy^2 <= 1. */
static void stamp(int x, int y, uint16_t color) {
  visit(x, y);
  put(x, y, color);
  put(x - 1, y, color);
  put(x + 1, y, color);
  put(x, y - 1, color);
  put(x, y + 1, color);
}

static void line(int x0, int y0, int x1, int y1, uint16_t color) {
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    stamp(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

/* Points over a visible control do not draw: the pill always, Info, Clean
 * and Exit once all 25 cells are visited. */
static bool over_control(int x, int y) {
  if (x >= 10 && x < 82 && y >= 10 && y < 38) {
    return true;
  }
  if (s_visited_count >= CELLS * CELLS && y >= 8 && y < 44) {
    return (x >= 202 && x < 286) || (x >= 294 && x < 378) ||
           (x >= 386 && x < 470);
  }
  return false;
}

/* The pill's colours follow the luma of nine pixels under it. */
static bool pill_dark(void) {
  static const int xs[] = {12, 46, 80};
  static const int ys[] = {12, 24, 36};
  uint32_t sum = 0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      const uint16_t p = s_pixels[ys[j] * SIDE + xs[i]];
      /* PSM_5650: red in bits 0-4, blue in bits 11-15. */
      const uint32_t r = ((p & 0x1F) * 255 + 15) / 31;
      const uint32_t g = (((p >> 5) & 0x3F) * 255 + 31) / 63;
      const uint32_t b = (((p >> 11) & 0x1F) * 255 + 15) / 31;
      sum += (77 * r + 150 * g + 29 * b) >> 8;
    }
  }
  return sum / 9 < 128;
}

static void reset(void) {
  memset(s_visited, 0, sizeof(s_visited));
  s_visited_count = 0;
  memset(s_pointers, 0, sizeof(s_pointers));
  draw_grid();
  s_dirty = true;
}

static void publish_counts(void) {
  m5_pocket_bridge_set_number("touchVisited", s_visited_count);
  m5_pocket_bridge_set_bool("touchPillDark", pill_dark());
}

/* canvas_touch_poll_cb: each contact continues the stroke of the nearest
 * previous contact closer than 120 px, or starts a stroke in a random
 * palette colour; contacts not seen again end their strokes. */
static void draw_contacts(void) {
  uint16_t xs[POINTERS];
  uint16_t ys[POINTERS];
  const size_t count = svc_contacts(xs, ys, POINTERS);
  for (int i = 0; i < POINTERS; i++) {
    s_pointers[i].seen = false;
  }
  const int before = s_visited_count;
  for (size_t n = 0; n < count; n++) {
    const int x = xs[n];
    const int y = ys[n];
    if (over_control(x, y)) {
      continue;
    }
    int best = -1;
    int best_d = MATCH_DISTANCE * MATCH_DISTANCE;
    for (int i = 0; i < POINTERS; i++) {
      if (!s_pointers[i].active || s_pointers[i].seen) {
        continue;
      }
      const int dx = x - s_pointers[i].x;
      const int dy = y - s_pointers[i].y;
      if (dx * dx + dy * dy < best_d) {
        best_d = dx * dx + dy * dy;
        best = i;
      }
    }
    if (best >= 0) {
      pointer_t *p = &s_pointers[best];
      line(p->x, p->y, x, y, p->color);
      p->x = x;
      p->y = y;
      p->seen = true;
    } else {
      for (int i = 0; i < POINTERS; i++) {
        if (!s_pointers[i].active) {
          s_pointers[i] = (pointer_t){
              .active = true,
              .seen = true,
              .x = x,
              .y = y,
              .color = svc_color(PALETTE[esp_random() % 10]),
          };
          stamp(x, y, s_pointers[i].color);
          break;
        }
      }
    }
    s_dirty = true;
  }
  for (int i = 0; i < POINTERS; i++) {
    if (!s_pointers[i].seen) {
      s_pointers[i].active = false;
    }
  }
  if (s_dirty || s_visited_count != before) {
    publish_counts();
  }
}

static void upload(bool force) {
  const int64_t now = esp_timer_get_time();
  if (!s_dirty || (!force && now - s_uploaded_us < UPLOAD_US)) {
    return;
  }
  if (svc_texture(TEXTURE, s_pixels, SIDE, SIDE, "touchTexture") == ESP_OK) {
    s_dirty = false;
    s_uploaded_us = now;
  }
}

/* refresh_touch_status. The factory prints "--" after "CST3XX" because its
 * initialised flag is never set; this prints "OK" once the driver runs. */
static void publish_status(void) {
  char text[192];
  esp_lcd_touch_handle_t touch = svc_touch();
  m5_corep4x_touch_info_t info;
  esp_err_t err = ESP_ERR_INVALID_STATE;
  if (!touch) {
    snprintf(text, sizeof(text),
             "Panel: CST3XX --\nChip ID: --\nFW Version: --\nHandle: --");
  } else if ((err = m5_corep4x_touch_info(touch, &info)) != ESP_OK) {
    snprintf(text, sizeof(text),
             "Panel: CST3XX OK\nChip ID: read failed\nFW Version: read "
             "failed\nError: %s",
             esp_err_to_name(err));
  } else {
    snprintf(text, sizeof(text),
             "Panel: CST3XX OK  Handle:ready\nChip ID: 0x%08lX\nFW Version: "
             "v%u.%u.%u (0x%08lX)\nResolution: %ux%u",
             (unsigned long)info.chip_id, (unsigned)(info.firmware >> 24),
             (unsigned)((info.firmware >> 16) & 0xFF),
             (unsigned)(info.firmware & 0xFFFF), (unsigned long)info.firmware,
             info.width, info.height);
  }
  m5_pocket_bridge_set_text("touchStatus", text);
}

static void open_canvas(void) {
  if (!s_pixels) {
    s_pixels = heap_caps_malloc(SIDE * SIDE * sizeof(uint16_t),
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_pixels) {
      ESP_LOGE(TAG, "no canvas buffer");
      return;
    }
  }
  s_open = true;
  s_paused = false;
  reset();
  publish_counts();
  upload(true);
  publish_status();
}

static void close_canvas(void) {
  s_open = false;
  s_paused = false;
  memset(s_pointers, 0, sizeof(s_pointers));
  m5_pocket_app *app = svc_app();
  if (app) {
    m5_pocket_app_set_texture(app, TEXTURE, NULL, SIDE, SIDE, SIDE * 2, NULL);
  }
  m5_pocket_bridge_set_number("touchTexture", -1);
  heap_caps_free(s_pixels);
  s_pixels = NULL;
}

bool canvas_svc_request(const m5_pocket_request *request) {
  if (svc_is(request, "touchCanvas")) {
    if (svc_arg(request, 0)) {
      open_canvas();
    } else {
      close_canvas();
    }
  } else if (svc_is(request, "touchCanvasPause")) {
    s_paused = svc_arg(request, 0) != 0;
    memset(s_pointers, 0, sizeof(s_pointers));
  } else if (svc_is(request, "touchCanvasClean")) {
    if (s_pixels) {
      reset();
      publish_counts();
      upload(true);
    }
  } else if (svc_is(request, "touchIndicator")) {
    const bool on = svc_arg(request, 0) != 0;
    svc_pref_set("touch_ind", on);
    m5_pocket_bridge_set_bool("touchIndicator", on);
  } else {
    return false;
  }
  return true;
}

void canvas_svc_frame(void) {
  if (!s_open || !s_pixels) {
    return;
  }
  if (!s_paused) {
    draw_contacts();
  }
  upload(false);
}

void canvas_svc_start(void) {
  m5_pocket_bridge_set_bool("touchIndicator", svc_pref_get("touch_ind", false));
  m5_pocket_bridge_set_number("touchVisited", 0);
  m5_pocket_bridge_set_bool("touchPillDark", false);
}
