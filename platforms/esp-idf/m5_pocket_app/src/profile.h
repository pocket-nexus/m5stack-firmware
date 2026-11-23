#pragma once
#include "app_internal.h"
#include "sdkconfig.h"

/* Frame profiler (CONFIG_M5_POCKET_APP_PROFILE). One tick is cut into the
 * stages below; the time between two marks belongs to the stage entered at
 * the first one. Stages 2 to 8 are entered from the AOT library
 * (platforms/rust/m5_pocket_aot/src/sys.rs); the others from the host. In
 * JS mode the guest turn, the core ticks and the draw list are all PRE. */
enum {
  M5_POCKET_STAGE_PRE = 0,       /* runtime entry, input encoding */
  M5_POCKET_STAGE_INPUT = 1,     /* input source poll (touch read) */
  M5_POCKET_STAGE_BRIDGE = 2,    /* board values and events into device.ts */
  M5_POCKET_STAGE_TRANSLATE = 3, /* guest frame into microts::Input */
  M5_POCKET_STAGE_APP_INPUT = 4, /* AppApp::frame up to the model snapshot */
  M5_POCKET_STAGE_MODEL = 5,     /* model resume, dispatch, react, settle */
  M5_POCKET_STAGE_VIEW = 6,      /* view update, lifecycle hooks, commands */
  M5_POCKET_STAGE_REQUESTS = 7,  /* requests to the program */
  M5_POCKET_STAGE_DRAW = 8,      /* core ticks, layout, draw list */
  M5_POCKET_STAGE_DAMAGE = 9,    /* damage plan, up to the first strip */
  M5_POCKET_STAGE_RASTER = 10,   /* acquire to flush of every strip */
  M5_POCKET_STAGE_FLUSH = 11,    /* byte swap and transfer submission */
  M5_POCKET_STAGE_WAIT = 12,     /* blocked on a strip transfer */
  M5_POCKET_STAGE_POST = 13,     /* between strips, damage commit */
  M5_POCKET_STAGE_COUNT = 14,
};

/* Enter a stage. Always linked: the AOT library calls it. A no-op outside a
 * tick and without the profiler. Stage VIEW is taken only from MODEL. */
void m5_pocket_profile_stage(uint32_t stage);

#if CONFIG_M5_POCKET_APP_PROFILE
/* Put the timing display table in front of devices->display. */
void m5_pocket_profile_wrap_display(m5_pocket_devices *devices);
void m5_pocket_profile_boot(uint32_t boot_us);
/* Around the program's before_frame hook, on the app task. */
void m5_pocket_profile_hook_begin(void);
void m5_pocket_profile_tick_begin(void);
void m5_pocket_profile_tick_end(const m5_pocket_app *app);
/* The stage of the running tick, M5_POCKET_STAGE_COUNT outside one. */
uint32_t m5_pocket_profile_current(void);
/* profile_tools.c: the console task; the sampler timer, attached from the
 * app task; the scripted contact and buttons, merged into one input poll. */
void m5_pocket_profile_tools_start(void);
void m5_pocket_profile_sampler_attach(void);
void m5_pocket_profile_input(uint32_t *buttons, bool *down, float *x, float *y);
uint32_t m5_pocket_profile_buttons(void);
/* One log line "N <text>" from the reporter task; callable from any task. */
void m5_pocket_profile_note(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
#else
static inline void m5_pocket_profile_wrap_display(m5_pocket_devices *devices) {
  (void)devices;
}
static inline void m5_pocket_profile_boot(uint32_t boot_us) { (void)boot_us; }
static inline void m5_pocket_profile_hook_begin(void) {}
static inline void m5_pocket_profile_tick_begin(void) {}
static inline void m5_pocket_profile_tick_end(const m5_pocket_app *app) {
  (void)app;
}
static inline void m5_pocket_profile_input(uint32_t *buttons, bool *down,
                                           float *x, float *y) {
  (void)buttons;
  (void)down;
  (void)x;
  (void)y;
}
static inline uint32_t m5_pocket_profile_buttons(void) { return 0; }
static inline void m5_pocket_profile_note(const char *format, ...) {
  (void)format;
}
#endif
