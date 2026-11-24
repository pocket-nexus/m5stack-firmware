/* The board services behind the device bridge of apps/corep4x-factory
 * (device.ts). Each service module owns a group of requests and the board
 * values it publishes:
 *
 *   start()    once, from app_main, before the application starts;
 *   frame()    on the app task before every frame (m5_pocket_app
 *              before_frame): publish values, upload textures;
 *   request()  for each application request, on the app host's helper task
 *              with an internal stack while the app task waits (the app
 *              task's stack is in PSRAM), so NVS, FAT and flash work are
 *              allowed there; returns true when the module handled it.
 *
 * frame() runs on the PSRAM stack: flash work from there goes through
 * m5_pocket_app_run_internal.
 *
 * Long work (scans, file tests, audio, pings) runs on tasks of the module;
 * those tasks publish through m5_pocket_bridge_set_* (thread-safe). */
#pragma once

#include "esp_lcd_touch.h"
#include "m5_pocket/app.h"
#include "m5_pocket/bridge.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Text tones of device.ts: 0 muted, 1 success, 2 warning, 3 error. */
#define SVC_MUTED 0
#define SVC_SUCCESS 1
#define SVC_WARNING 2
#define SVC_ERROR 3

/* 0xRRGGBB as a PSM_5650 pixel (red in the low 5 bits, blue in the high
 * 5), the layout program textures use. */
static inline uint16_t svc_color(uint32_t rgb) {
  const uint32_t r = (rgb >> 16) & 0xFF;
  const uint32_t g = (rgb >> 8) & 0xFF;
  const uint32_t b = rgb & 0xFF;
  return (uint16_t)(((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3));
}

/* The request's first word is `name`. */
bool svc_is(const m5_pocket_request *request, const char *name);
/* The request text after its first word and one space ("" when none). */
const char *svc_rest(const m5_pocket_request *request);
/* Argument i as an integer (0 when missing). */
int svc_arg(const m5_pocket_request *request, size_t i);

/* The running application (NULL before m5_pocket_app_start returned). */
m5_pocket_app *svc_app(void);
/* Uploads an RGB565 picture (PSM_5650: red in the low 5 bits) as the image
 * `name` the views draw and publishes the new handle as the number
 * `handle_value`. Call from frame() or request(). */
esp_err_t svc_texture(const char *name, const uint16_t *pixels, uint32_t width,
                      uint32_t height, const char *handle_value);
/* The touch controller (NULL without one). */
esp_lcd_touch_handle_t svc_touch(void);
/* Up to `capacity` current contacts in picture coordinates; returns the
 * count. Reads the data the app host's input poll fetched last (the
 * input hook reads the controller once per frame on the app task). */
size_t svc_contacts(uint16_t *xs, uint16_t *ys, size_t capacity);

/* NVS namespace "corep4_prefs" (the factory's): u8 values. Callable from
 * any hook; the write runs on an internal stack. */
bool svc_pref_get(const char *key, bool fallback);
void svc_pref_set(const char *key, bool value);

#define SVC_MODULE(name)                                                       \
  void name##_start(void);                                                     \
  void name##_frame(void);                                                     \
  bool name##_request(const m5_pocket_request *request)

/* svc_power.c: rails, backlight, charge, VBAT, M5-Bus GPIOs, power off,
 * restart, PM1 button, deep sleep. */
SVC_MODULE(power_svc);
/* svc_storage.c: microSD (detect, listing, speed test). */
SVC_MODULE(storage_svc);
/* svc_i2c.c: the I2C scans. */
SVC_MODULE(i2c_svc);
/* svc_canvas.c: the TOUCH canvas, the touch indicator and touch status. */
SVC_MODULE(canvas_svc);
/* svc_imu.c: the IMU window's fusion, rendering and gestures. */
SVC_MODULE(imu_svc);
/* svc_audio.c: startup sound, volume, MUSIC, REC. */
SVC_MODULE(audio_svc);
/* svc_net.c: Ethernet, PING, SNTP. */
SVC_MODULE(net_svc);
/* svc_rtc.c: RX8130CE time, TIMER requests. */
SVC_MODULE(rtc_svc);
/* svc_ir.c: NEC transmit and receive. */
SVC_MODULE(ir_svc);

/* svc_audio.c: the power service switched the codec supply (rail AudioIC)
 * or the amplifier (rail AudioPA) at the application's request. */
void audio_svc_rails_changed(bool codec_on, bool amplifier_on);
/* Deep sleep preparation (svc_power.c runs the factory's sleep_before_work
 * order and calls these from its sleep task, after the application stopped
 * drawing):
 * svc_audio.c: stop playback, recording and monitoring, deinit the codecs
 * and I2S, set the I2S pins (GPIO 2-6) to high impedance. */
void audio_svc_sleep(void);
/* svc_imu.c: stop the fusion task (the power service then configures the
 * sensor for sleep). */
void imu_svc_sleep(void);
/* svc_storage.c: unmount the microSD card, set the SPI pins to high
 * impedance and the detect input to no pull. */
void storage_svc_sleep(void);
/* services.c: the panel and the touch controller for deep sleep: with
 * `touch_off` the CST3530 sleep command, its reset held low and GPIO1
 * released; then the panel off and deleted, the LCD rail off and the DSI
 * PHY LDO released. */
void services_display_off(bool touch_off);
/* svc_rtc.c: stop the once-a-second RTC reads (the sleep sequence then
 * owns the I2C bus). */
void rtc_svc_sleep(void);
/* svc_net.c: stop Ethernet, drive the PHY reset (GPIO52) low, set the RMII
 * pins to high impedance. */
void net_svc_sleep(void);
/* svc_net.c: true once an SNTP sync over Ethernet set the system clock during
 * the current TIMER visit (the probe runs on "timerPage 1"). */
bool net_svc_sntp_synced(void);
/* svc_rtc.c: write the system clock (UTC+8) to the RTC. */
esp_err_t rtc_svc_save_system_time(void);
