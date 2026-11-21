// SPDX-License-Identifier: MIT
//
// Wi-Fi behaviour of the UIFlow2 StickS3 startup (m5stack/modules/startup/
// __init__.py Startup and sticks3.py SetupApp): station connection from the
// credentials saved in NVS "uiflow" (ssid0/pswd0), and the Setup access point
// "S3_XXXX" with captive DNS and the Wi-Fi settings portal on 192.168.4.1.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Station status, numbered like apps/sticks3-factory/device.ts WIFI_*.
typedef enum {
  M5_STICKS3_NET_IDLE = 0,
  M5_STICKS3_NET_CONNECTING = 1,
  M5_STICKS3_NET_GOT_IP = 2,
  M5_STICKS3_NET_FAILED = 3,
} m5_sticks3_net_status_t;

typedef struct {
  m5_sticks3_net_status_t status;
  /// A station SSID is saved.
  bool configured;
  /// The Setup access point runs.
  bool setup_active;
  /// Stations associated with the access point.
  uint32_t ap_stations;
  /// Portal requests answered since the access point started.
  uint32_t portal_clients;
  /// Access point name, "S3_" and the last four hex digits of the MAC.
  char ap_ssid[16];
} m5_sticks3_net_state_t;

/// Initialises NVS, netif and Wi-Fi in station mode and connects to the saved
/// network, if any (UIFlow Startup.connect_network).
esp_err_t m5_sticks3_net_init(void);

/// Starts the access point, captive DNS and portal (SetupApp.on_launch).
esp_err_t m5_sticks3_net_setup_start(void);

/// Stops them and reconnects the station when it is idle (SetupApp.on_exit).
esp_err_t m5_sticks3_net_setup_stop(void);

/// Copies the current state.
void m5_sticks3_net_get_state(m5_sticks3_net_state_t *out);

#ifdef __cplusplus
}
#endif
