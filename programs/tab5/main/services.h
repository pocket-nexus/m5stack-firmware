/* The board side of the device bridge: publishes the factory demo's board
 * values before each frame and carries out the application's requests. */
#pragma once
#include "esp_lcd_touch.h"
#include "m5_pocket/bridge.h"

typedef struct {
  esp_lcd_touch_handle_t touch;
  const char *panel_name;
} tab5_services_config_t;

/* Sensors, the system clock from the RTC, RS485 and sound. */
void tab5_services_start(const tab5_services_config_t *config);
/* m5_pocket_app before_frame hook. */
void tab5_services_publish(void *context);
/* m5_pocket_app on_request hook. */
void tab5_services_request(void *context, const m5_pocket_request *request);
