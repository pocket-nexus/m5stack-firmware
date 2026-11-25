/*
 * The C ABI between m5_pocket_app and an AOT application library
 * (platforms/rust/m5_pocket_aot, exported by its `export_app!` macro).
 * m5_pocket_app calls these functions in AOT mode; a program does not.
 *
 * The library adapts the C display, renderer and input vtables to its Rust
 * DisplayOutput, Renderer and InputSource traits, creates a
 * pocket_runtime::Runtime with the exported host profile, boots the compiled
 * application with Runtime::boot_aot_deferred and presents its first frame.
 * Its allocator calls pocket_host_alloc and pocket_host_free, its panic
 * handler pocket_host_panic (pocket_runtime/src/hooks.c).
 */
#pragma once
#include "pocket_drivers.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  size_t struct_size;
  const pocket_display_output_v1 *display;
  void *display_context;
  /* NULL: the software rasterizer. */
  const pocket_renderer_v1 *renderer;
  void *renderer_context;
  const pocket_input_source_v1 *const *inputs;
  void *const *input_contexts;
  size_t input_count;
  size_t heap_limit;
  size_t frame_buffer_budget;
} m5_pocket_aot_config;

typedef struct m5_pocket_aot m5_pocket_aot;

/* NULL on failure, after logging the reason through pocket_host_log. */
m5_pocket_aot *m5_pocket_aot_create(const m5_pocket_aot_config *config);
/* The rate pocket_runtime_tick-equivalent calls are scheduled at. */
uint32_t m5_pocket_aot_cadence_hz(const m5_pocket_aot *app);
/* One frame: bridge values in, input, guest turn, requests out, present.
 * Returns a POCKET_STATUS_* code. */
int32_t m5_pocket_aot_tick(m5_pocket_aot *app);
/* One frame without presentation: as m5_pocket_aot_tick up to the core
 * ticks; the next presented frame paints what it changed. */
int32_t m5_pocket_aot_skip(m5_pocket_aot *app);
void m5_pocket_aot_destroy(m5_pocket_aot *app);
/* m5_pocket_app_set_texture, between frames on the app task: upload
 * width x height texels of format `psm` into the app's Ui (a handle or -1),
 * free a handle, and register `name` (name_length bytes of UTF-8) for a
 * handle (-1 unbinds), returning a POCKET_STATUS_* code. */
int32_t m5_pocket_aot_upload_texture(m5_pocket_aot *app, const void *pixels,
                                     size_t length, uint32_t width,
                                     uint32_t height, uint32_t psm);
void m5_pocket_aot_free_texture(m5_pocket_aot *app, int32_t handle);
int32_t m5_pocket_aot_register_image(m5_pocket_aot *app, const char *name,
                                     size_t name_length, int32_t handle);

#ifdef __cplusplus
}
#endif
