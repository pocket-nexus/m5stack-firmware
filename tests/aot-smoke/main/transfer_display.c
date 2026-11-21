#include "transfer_display.h"
#include "esp_heap_caps.h"
#include <string.h>

int32_t transfer_display_init(transfer_display *d, uint32_t width,
                              uint32_t height, uint32_t rows) {
  if (!d || !width || !height || !rows || rows > height)
    return POCKET_STATUS_INVALID;
  memset(d, 0, sizeof(*d));
  d->width = width;
  d->height = height;
  d->rows = rows;
  d->active = UINT32_MAX;
  d->capacity = (size_t)width * rows * 2;
  for (uint32_t i = 0; i < 2; i++) {
    d->buffers[i].pixels = heap_caps_aligned_alloc(
        128, d->capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!d->buffers[i].pixels) {
      transfer_display_destroy(d);
      return POCKET_STATUS_FAILED;
    }
  }
  return POCKET_STATUS_OK;
}

void transfer_display_destroy(transfer_display *d) {
  for (uint32_t i = 0; d && i < 2; i++) {
    heap_caps_free(d->buffers[i].pixels);
    d->buffers[i].pixels = NULL;
  }
}

static int32_t descriptor(void *context, pocket_display_descriptor *out) {
  transfer_display *d = context;
  if (!d || !out)
    return POCKET_STATUS_INVALID;
  *out = (pocket_display_descriptor){.struct_size = sizeof(*out),
                                     .role = POCKET_DISPLAY_ROLE_PRIMARY,
                                     .width = d->width,
                                     .height = d->height,
                                     .pixel_format = POCKET_PIXEL_FORMAT_RGB565,
                                     .stride_bytes = (size_t)d->width * 2,
                                     .target_kind = POCKET_TARGET_CPU_VISIBLE,
                                     .flags = POCKET_DISPLAY_FRAME_STORAGE |
                                              POCKET_DISPLAY_PARTIAL_REFRESH |
                                              POCKET_DISPLAY_DOUBLE_BUFFERED |
                                              POCKET_DISPLAY_PIPELINED_ACQUIRE,
                                     .refresh_width = 1,
                                     .refresh_height = 1,
                                     .window_x_alignment = 1,
                                     .window_y_alignment = 1};
  return POCKET_STATUS_OK;
}

static int32_t acquire(void *context, const pocket_rect *region,
                       pocket_target *out) {
  transfer_display *d = context;
  if (!d || !region || !out || region->x || region->width != d->width ||
      !region->height || region->height > d->rows ||
      region->y > d->height - region->height)
    return POCKET_STATUS_INVALID;
  for (uint32_t i = 0; i < 2; i++) {
    transfer_buffer *b = &d->buffers[i];
    if (b->pending)
      continue;
    d->active = i;
    b->region = *region;
    b->surface =
        (pocket_surface){.struct_size = sizeof(b->surface),
                         .pixels = b->pixels,
                         .byte_length = (size_t)d->width * region->height * 2,
                         .region = &b->region,
                         .stride_bytes = (size_t)d->width * 2,
                         .pixel_format = POCKET_PIXEL_FORMAT_RGB565};
    *out = (pocket_target){.struct_size = sizeof(*out),
                           .kind = POCKET_TARGET_CPU_VISIBLE,
                           .region = &b->region,
                           .pixel_format = POCKET_PIXEL_FORMAT_RGB565,
                           .surface = &b->surface};
    return POCKET_STATUS_OK;
  }
  return POCKET_STATUS_FAILED;
}

static int32_t flush(void *context, const pocket_rect *regions, size_t count,
                     uint64_t *token) {
  transfer_display *d = context;
  if (!d || !regions || count != 1 || !token || d->active >= 2)
    return POCKET_STATUS_INVALID;
  transfer_buffer *b = &d->buffers[d->active];
  if (!b->region.y)
    d->hash = UINT64_C(0xcbf29ce484222325);
  for (size_t i = 0; i < b->surface.byte_length; i++)
    d->hash = (d->hash ^ b->pixels[i]) * UINT64_C(0x100000001b3);
  b->pending = true;
  *token = b->token = ++d->next;
  d->active = UINT32_MAX;
  d->flushes++;
  return POCKET_STATUS_OK;
}

static int32_t wait(void *context, uint64_t token) {
  transfer_display *d = context;
  if (!d)
    return POCKET_STATUS_INVALID;
  for (uint32_t i = 0; i < 2; i++)
    if (d->buffers[i].pending && d->buffers[i].token == token) {
      d->buffers[i].pending = false;
      d->waits++;
      return POCKET_STATUS_OK;
    }
  return POCKET_STATUS_INVALID;
}

static int32_t identity(void *context, pocket_buffer_identity *out) {
  if (!context || !out)
    return POCKET_STATUS_INVALID;
  *out = (pocket_buffer_identity){
      .struct_size = sizeof(*out), .buffer = 1, .generation = 1};
  return POCKET_STATUS_OK;
}

static const pocket_display_output_v1 VTABLE = {.struct_size = sizeof(VTABLE),
                                                .descriptor = descriptor,
                                                .acquire = acquire,
                                                .flush = flush,
                                                .wait = wait,
                                                .frame_identity = identity};

const pocket_display_output_v1 *transfer_display_vtable(void) {
  return &VTABLE;
}
