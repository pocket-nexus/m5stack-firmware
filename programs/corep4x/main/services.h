/* Board services behind the device bridge of apps/corep4x-factory (svc.h
 * lists the modules). */
#pragma once
#include "esp_lcd_touch.h"
#include "m5_corep4x/bsp.h"
#include "m5_pocket/app.h"
#include "m5_pocket/bridge.h"
#include <stdint.h>

/* NVS and every service module; call before m5_pocket_app_start. The
 * power service deletes the display and the touch controller before deep
 * sleep. */
void services_start(esp_lcd_touch_handle_t touch, m5_corep4x_display_t *lcd);
/* The running application, for texture uploads (NULL once stopped). */
void services_set_app(m5_pocket_app *app);
/* before_frame: publishes the board values the application reads. */
void services_publish(void *context);
/* on_request: carries out one application request. */
void services_request(void *context, const m5_pocket_request *request);
