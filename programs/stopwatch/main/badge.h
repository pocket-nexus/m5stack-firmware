/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 M5Stack-sample contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Badge service (factory app_badge, hal_badge.cpp, config_ap.cpp): six JPEG
 * slots on the FAT partition, the editor access point with its captive DNS
 * server and HTTP routes, and the decode of the active slot for the
 * application. Publishes `badgeSlots`, `badgeActive`, `apSsid` and
 * `apMessage` on the device bridge and posts `badgeClosed` when the phone
 * closes the editor.
 */

/* Mounts the FAT partition at /spiflash, resolves the active slot with the
 * factory fallback (stored slot, else the first occupied one) and publishes
 * the badge state and the access point name. */
esp_err_t stopwatch_badge_init(void);

/* Makes an occupied slot active, stores it and publishes the state.
 * ESP_ERR_INVALID_ARG for a slot outside 0..5, ESP_ERR_NOT_FOUND for an
 * empty slot. */
esp_err_t stopwatch_badge_select(int slot);

/* Starts (true) or stops (false) the editor access point, DNS and HTTP
 * servers. Publishes the status text as `apMessage`. */
esp_err_t stopwatch_badge_edit(bool on);

typedef struct {
  /* canvas_w x canvas_h pixels, (b5 << 11) | (g6 << 5) | r5, black outside
   * the image, which is centred. NULL when there is no active image. */
  uint16_t *pixels;
  uint32_t canvas_w, canvas_h, image_w, image_h;
} stopwatch_badge_image;

/* Called on the app task once per frame. When the active image changed
 * since the previous call, decodes it and returns true; the caller frees
 * `pixels` with heap_caps_free. Returns true with pixels == NULL when there
 * is no decodable active image, false when nothing changed. */
bool stopwatch_badge_take_image(stopwatch_badge_image *out);
