/*
 * SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"
#include "m5_pocket/app.h"
#include "m5_pocket/bridge.h"
#include <stdint.h>

/* Loads the persistent settings and alarms (NVS namespaces "system" and
 * "alarm", factory keys), starts the RTC clock, the IMU, the audio tasks
 * and publishes the initial bridge values. */
esp_err_t stopwatch_services_start(void);

/* Publishes the per-frame values: uptime, wall clock, keys, battery (1 Hz)
 * and IMU. Runs on the app task. */
void stopwatch_services_before_frame(void);

/* Handles one application request (see apps/stopwatch-factory/device.ts). */
void stopwatch_services_request(const m5_pocket_request *request);

/* The running app, for runtime operations from the hooks (JS mode). */
void stopwatch_services_set_app(m5_pocket_app *app);

/* KEYA / KEYB as BTN.LTRIGGER / BTN.RTRIGGER bits. */
uint32_t stopwatch_services_buttons(void);
