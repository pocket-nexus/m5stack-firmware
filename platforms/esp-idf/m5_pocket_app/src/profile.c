/* Frame profiler: stage times of every tick, reported once per period by a
 * task on core 0 so that the log write does not run on the app task. */
#include "profile.h"

#if !CONFIG_M5_POCKET_APP_PROFILE

void m5_pocket_profile_stage(uint32_t stage) { (void)stage; }

uint32_t m5_pocket_app_scripted_buttons(void) { return 0; }

#else

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "pocket/display_esp_lcd.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "m5_prof";

#define STAGES M5_POCKET_STAGE_COUNT
#define PERIOD_US ((int64_t)CONFIG_M5_POCKET_APP_PROFILE_PERIOD_MS * 1000)
#define SLOW_US ((uint32_t)CONFIG_M5_POCKET_APP_PROFILE_SLOW_MS * 1000u)
/* Slow frames reported one by one per period. */
#define SLOW_PER_PERIOD 4
/* An allocation of at least this size is counted as large (a texture). */
#define LARGE_BYTES (16u * 1024u)

static const char *const NAMES[STAGES] = {
    "pre",  "input", "bridge", "translate", "appin", "model", "view",
    "reqs", "draw",  "damage", "raster",    "flush", "wait",  "post",
};

/* One tick. */
typedef struct {
  uint32_t stage_us[STAGES];
  uint32_t tick_us, hook_us;
  uint32_t strips, rows;
  uint32_t allocs, large, large_bytes;
} frame_record;

/* One period, one slow frame (frames == 0) or one note (note[0] != 0). */
typedef struct {
  char note[96];
  uint32_t frames, skipped, slow;
  uint32_t span_us;
  uint32_t stage_max[STAGES];
  uint64_t stage_sum[STAGES];
  uint64_t tick_sum, hook_sum;
  uint32_t tick_max, hook_max;
  uint32_t strips, rows;
  uint32_t allocs, large, large_bytes;
  uint32_t stack_free, internal_free, frame_index;
  frame_record frame;
} report;

static struct {
  const pocket_display_output_v1 *inner;
  void *inner_context;
  /* The display when it is a pocket_display_esp_lcd, for its counters. */
  const pocket_display_esp_lcd *lcd;
  pocket_display_esp_lcd_stats presented;
  pocket_display_output_v1 table;
  QueueHandle_t queue;
  TaskHandle_t app_task;
  volatile bool in_tick;
  uint32_t stage;
  int64_t since, hook_started, tick_started, period_started;
  uint32_t skipped_before;
  uint32_t slow_reported;
  frame_record frame;
  report period;
} P;

void m5_pocket_profile_stage(uint32_t stage) {
  if (!P.in_tick || stage >= STAGES)
    return;
  if (stage == M5_POCKET_STAGE_VIEW && P.stage != M5_POCKET_STAGE_MODEL)
    return;
  const int64_t now = esp_timer_get_time();
  P.frame.stage_us[P.stage] += (uint32_t)(now - P.since);
  P.stage = stage;
  P.since = now;
}

uint32_t m5_pocket_profile_current(void) {
  return P.in_tick ? P.stage : M5_POCKET_STAGE_COUNT;
}

uint32_t m5_pocket_app_scripted_buttons(void) {
  return m5_pocket_profile_buttons();
}

/* Allocations of the app task during a tick (CONFIG_HEAP_USE_HOOKS). */
void esp_heap_trace_alloc_hook(void *ptr, size_t size, uint32_t caps) {
  (void)ptr;
  (void)caps;
  if (!P.in_tick || xTaskGetCurrentTaskHandle() != P.app_task)
    return;
  P.frame.allocs++;
  if (size >= LARGE_BYTES) {
    P.frame.large++;
    P.frame.large_bytes += (uint32_t)size;
  }
}

void esp_heap_trace_free_hook(void *ptr) { (void)ptr; }

static int put(char *line, size_t size, int at, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

/* ------------------------------- display --------------------------------- */

static int32_t wrap_descriptor(void *context, pocket_display_descriptor *out) {
  (void)context;
  return P.inner->descriptor(P.inner_context, out);
}

static int32_t wrap_identity(void *context, pocket_buffer_identity *out) {
  (void)context;
  m5_pocket_profile_stage(M5_POCKET_STAGE_DAMAGE);
  return P.inner->frame_identity(P.inner_context, out);
}

static int32_t wrap_acquire(void *context, const pocket_rect *region,
                            pocket_target *out) {
  (void)context;
  /* A staged display blocks here until a render buffer is free. */
  m5_pocket_profile_stage(M5_POCKET_STAGE_WAIT);
  const int32_t status = P.inner->acquire(P.inner_context, region, out);
  if (P.in_tick)
    P.frame.strips++;
  m5_pocket_profile_stage(M5_POCKET_STAGE_RASTER);
  return status;
}

static int32_t wrap_flush(void *context, const pocket_rect *regions,
                          size_t count, uint64_t *token) {
  (void)context;
  m5_pocket_profile_stage(M5_POCKET_STAGE_FLUSH);
  if (P.in_tick && regions) {
    const pocket_rect *r = regions;
    for (size_t i = 0; i < count && r->struct_size >= sizeof(*r); i++) {
      P.frame.rows += r->height;
      r = (const pocket_rect *)((const uint8_t *)r + r->struct_size);
    }
  }
  const int32_t status = P.inner->flush(P.inner_context, regions, count, token);
  if (status != POCKET_STATUS_OK && regions) {
    /* The rows the display refused, for the frame error that follows. */
    char rows[80];
    int at = 0;
    const pocket_rect *r = regions;
    for (size_t i = 0; i < count && r->struct_size >= sizeof(*r); i++) {
      at =
          put(rows, sizeof(rows), at, " %" PRIu32 "+%" PRIu32, r->y, r->height);
      r = (const pocket_rect *)((const uint8_t *)r + r->struct_size);
    }
    m5_pocket_profile_note("flush %d rows%s", (int)status, at ? rows : "");
  }
  m5_pocket_profile_stage(M5_POCKET_STAGE_POST);
  return status;
}

static int32_t wrap_wait(void *context, uint64_t token) {
  (void)context;
  m5_pocket_profile_stage(M5_POCKET_STAGE_WAIT);
  const int32_t status = P.inner->wait(P.inner_context, token);
  m5_pocket_profile_stage(M5_POCKET_STAGE_POST);
  return status;
}

static uint32_t wrap_vsync(void *context) {
  (void)context;
  return P.inner->vsync(P.inner_context);
}

static int32_t wrap_move(void *context, const pocket_rect *destination,
                         int32_t dx, int32_t dy) {
  (void)context;
  return P.inner->move_pixels(P.inner_context, destination, dx, dy);
}

static int32_t wrap_host_damage(void *context, pocket_rect *out,
                                size_t capacity, size_t *count) {
  (void)context;
  return P.inner->host_damage(P.inner_context, out, capacity, count);
}

/* -------------------------------- report --------------------------------- */

static int put(char *line, size_t size, int at, const char *format, ...) {
  if (at < 0 || (size_t)at >= size)
    return at;
  va_list args;
  va_start(args, format);
  const int n = vsnprintf(line + at, size - (size_t)at, format, args);
  va_end(args);
  return n < 0 ? at : at + n;
}

static void print_period(const report *r) {
  static char line[640];
  const uint32_t n = r->frames ? r->frames : 1;
  const uint32_t fps10 =
      r->span_us ? (uint32_t)((uint64_t)r->frames * 10000000u / r->span_us) : 0;
  int at = put(line, sizeof(line), 0,
               "P fps=%" PRIu32 ".%" PRIu32 " frames=%" PRIu32
               " skipped=%" PRIu32 " slow=%" PRIu32 " tick=%" PRIu32 "/%" PRIu32
               " hook=%" PRIu32 "/%" PRIu32,
               fps10 / 10, fps10 % 10, r->frames, r->skipped, r->slow,
               (uint32_t)(r->tick_sum / n), r->tick_max,
               (uint32_t)(r->hook_sum / n), r->hook_max);
  for (int i = 0; i < STAGES; i++)
    at = put(line, sizeof(line), at, " %s=%" PRIu32 "/%" PRIu32, NAMES[i],
             (uint32_t)(r->stage_sum[i] / n), r->stage_max[i]);
  at = put(line, sizeof(line), at,
           " strips=%" PRIu32 " rows=%" PRIu32 " allocs=%" PRIu32
           " large=%" PRIu32 " largeB=%" PRIu32 " stack=%" PRIu32
           " istack=%" PRIu32 " int=%u/%u psram=%u/%u",
           r->strips, r->rows, r->allocs, r->large, r->large_bytes,
           r->stack_free, r->internal_free,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
  pocket_display_esp_lcd_stats now = {.struct_size = sizeof(now)};
  if (P.lcd && pocket_display_esp_lcd_get_stats(P.lcd, &now)) {
    /* Staged presentation, this period: flushes presented, without a
     * refresh report, late for the next scan; the longest time from a flush
     * to its last transfer since the start. */
    at = put(line, sizeof(line), at,
             " present=%" PRIu32 " unsynced=%" PRIu32 " late=%" PRIu32
             " ptime=%" PRIu32,
             now.frames - P.presented.frames,
             now.unsynced - P.presented.unsynced, now.late - P.presented.late,
             now.present_us_max);
    P.presented = now;
  }
  ESP_LOGI(TAG, "%s", line);
}

static void print_frame(const report *r) {
  static char line[400];
  const frame_record *f = &r->frame;
  int at = put(line, sizeof(line), 0,
               "S frame=%" PRIu32 " tick=%" PRIu32 " hook=%" PRIu32,
               r->frame_index, f->tick_us, f->hook_us);
  for (int i = 0; i < STAGES; i++)
    at = put(line, sizeof(line), at, " %s=%" PRIu32, NAMES[i], f->stage_us[i]);
  at = put(line, sizeof(line), at,
           " strips=%" PRIu32 " rows=%" PRIu32 " allocs=%" PRIu32
           " large=%" PRIu32 " largeB=%" PRIu32,
           f->strips, f->rows, f->allocs, f->large, f->large_bytes);
  ESP_LOGI(TAG, "%s", line);
}

static void reporter(void *argument) {
  (void)argument;
  static report r;
  for (;;) {
    if (xQueueReceive(P.queue, &r, portMAX_DELAY) != pdTRUE)
      continue;
    if (r.note[0])
      ESP_LOGI(TAG, "N %s", r.note);
    else if (r.frames)
      print_period(&r);
    else
      print_frame(&r);
  }
}

void m5_pocket_profile_note(const char *format, ...) {
  if (!P.queue)
    return;
  report *note = heap_caps_calloc(1, sizeof(*note), MALLOC_CAP_8BIT);
  if (!note)
    return;
  va_list args;
  va_start(args, format);
  vsnprintf(note->note, sizeof(note->note), format, args);
  va_end(args);
  (void)xQueueSend(P.queue, note, 0);
  heap_caps_free(note);
}

/* --------------------------------- host ---------------------------------- */

void m5_pocket_profile_wrap_display(m5_pocket_devices *devices) {
  memset(&P, 0, sizeof(P));
  P.queue = xQueueCreate(8, sizeof(report));
  if (!P.queue || xTaskCreatePinnedToCore(reporter, "m5_prof", 4096, NULL, 1,
                                          NULL, 0) != pdPASS) {
    ESP_LOGE(TAG, "no reporter; the profiler stays off");
    return;
  }
  const pocket_display_output_v1 *inner = devices->display;
  P.inner = inner;
  P.inner_context = devices->display_context;
  if (inner == pocket_display_esp_lcd_vtable())
    P.lcd = devices->display_context;
  P.table = (pocket_display_output_v1){
      .struct_size = sizeof(P.table),
      .descriptor = wrap_descriptor,
      .acquire = wrap_acquire,
      .flush = wrap_flush,
      .wait = wrap_wait,
      .vsync = inner->vsync ? wrap_vsync : NULL,
      .frame_identity = inner->frame_identity ? wrap_identity : NULL,
      .move_pixels = inner->move_pixels ? wrap_move : NULL,
      .host_damage = inner->host_damage ? wrap_host_damage : NULL,
  };
  devices->display = &P.table;
  m5_pocket_profile_tools_start();
  ESP_LOGI(TAG, "on: period %d ms, slow frame %d ms",
           CONFIG_M5_POCKET_APP_PROFILE_PERIOD_MS,
           CONFIG_M5_POCKET_APP_PROFILE_SLOW_MS);
}

void m5_pocket_profile_boot(uint32_t boot_us) {
  ESP_LOGI(TAG, "B boot=%" PRIu32 " int=%u psram=%u", boot_us,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void m5_pocket_profile_hook_begin(void) {
  if (!P.queue)
    return;
  P.hook_started = esp_timer_get_time();
  if (!P.period_started) {
    P.period_started = P.hook_started;
    P.app_task = xTaskGetCurrentTaskHandle();
    m5_pocket_profile_sampler_attach();
  }
}

void m5_pocket_profile_tick_begin(void) {
  if (!P.queue)
    return;
  const int64_t now = esp_timer_get_time();
  memset(&P.frame, 0, sizeof(P.frame));
  P.frame.hook_us = (uint32_t)(now - P.hook_started);
  P.tick_started = P.since = now;
  P.stage = M5_POCKET_STAGE_PRE;
  P.in_tick = true;
}

void m5_pocket_profile_tick_end(const m5_pocket_app *app) {
  if (!P.queue)
    return;
  const int64_t now = esp_timer_get_time();
  frame_record *f = &P.frame;
  f->stage_us[P.stage] += (uint32_t)(now - P.since);
  P.in_tick = false;
  f->tick_us = (uint32_t)(now - P.tick_started);

  report *r = &P.period;
  r->frames++;
  r->tick_sum += f->tick_us;
  r->hook_sum += f->hook_us;
  if (f->tick_us > r->tick_max)
    r->tick_max = f->tick_us;
  if (f->hook_us > r->hook_max)
    r->hook_max = f->hook_us;
  for (int i = 0; i < STAGES; i++) {
    r->stage_sum[i] += f->stage_us[i];
    if (f->stage_us[i] > r->stage_max[i])
      r->stage_max[i] = f->stage_us[i];
  }
  r->strips += f->strips;
  r->rows += f->rows;
  r->allocs += f->allocs;
  r->large += f->large;
  r->large_bytes += f->large_bytes;

  if (f->tick_us + f->hook_us >= SLOW_US) {
    r->slow++;
    if (P.slow_reported < SLOW_PER_PERIOD) {
      P.slow_reported++;
      static report slow;
      slow.note[0] = 0;
      slow.frames = 0;
      slow.frame_index = app->stats.frames;
      slow.frame = *f;
      (void)xQueueSend(P.queue, &slow, 0);
    }
  }
  if (now - P.period_started >= PERIOD_US) {
    r->span_us = (uint32_t)(now - P.period_started);
    r->skipped = app->stats.frames_skipped - P.skipped_before;
    r->stack_free = app->stats.stack_free_min;
    r->internal_free = m5_pocket_internal_stack_free_min();
    (void)xQueueSend(P.queue, r, 0);
    memset(r, 0, sizeof(*r));
    P.skipped_before = app->stats.frames_skipped;
    P.slow_reported = 0;
    P.period_started = now;
  }
}

#endif
