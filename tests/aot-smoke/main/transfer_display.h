/* A transfer-only RGB565 display: two strip buffers whose flushes are hashed
 * instead of sent to a panel. It follows the sample display of
 * third_party/pocketjs/tests/esp-idf/examples/common/sample_io.c. */
#pragma once
#include "pocket_drivers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *pixels;
  bool pending;
  uint64_t token;
  pocket_rect region;
  pocket_surface surface;
} transfer_buffer;

typedef struct {
  uint32_t width, height, rows, active;
  size_t capacity;
  uint64_t next, hash;
  uint32_t flushes, waits;
  transfer_buffer buffers[2];
} transfer_display;

/* Two strips of `rows` rows in internal DMA-capable RAM, 128-byte aligned. */
int32_t transfer_display_init(transfer_display *display, uint32_t width,
                              uint32_t height, uint32_t rows);
void transfer_display_destroy(transfer_display *display);
const pocket_display_output_v1 *transfer_display_vtable(void);
