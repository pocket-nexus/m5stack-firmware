// Device data shared with the firmware host. The host writes the values in
// the first block and reads the requests in the second; views and models
// reach the hardware only through this state module. Until the device data
// bridge is connected the values keep the defaults below, which describe a
// StickS3 on battery with no Wi-Fi configured and no UIFlow cloud service.
import type { i32 } from "@pocketjs/framework/solid/std";

// Wi-Fi station status, after MicroPython network.WLAN.status().
export const WIFI_IDLE: i32 = 0;
export const WIFI_CONNECTING: i32 = 1;
export const WIFI_GOT_IP: i32 = 2;
export const WIFI_FAILED: i32 = 3;

// UIFlow M5Things cloud status: -1 when the firmware has no cloud client
// (UIFlow's _HAS_SERVER = False), otherwise M5Things.status().
export const CLOUD_UNAVAILABLE: i32 = -1;
export const CLOUD_CONNECTED: i32 = 2;

// ---- Host-provided values ----

/** Battery level in percent, -1 when the PMIC cannot be read. */
export let batteryLevel: i32 = 100;
/** Charger CHG_STAT reports charging. */
export let charging: boolean = false;
/** A station SSID is stored (UIFlow NVS "uiflow"/"ssid0" is not empty). */
export let wifiConfigured: boolean = false;
/** One of the WIFI_* codes. */
export let wifiStatus: i32 = 0;
/** CLOUD_UNAVAILABLE or an M5Things status code. */
export let cloudStatus: i32 = -1;
export let accessCode: string = "";
export let nickName: string = "";
/** Setup access point name, "S3_" and the last four hex digits of the MAC. */
export let apSsid: string = "S3_0000";
/** Stations associated with the setup access point. */
export let apStations: i32 = 0;
/** Clients that requested a page from the setup portal. */
export let portalClients: i32 = 0;
/** Firmware version shown on the launcher (UIFlow: firmware_info()[3]). */
export let firmwareVersion: string = "v0.1.0";

// ---- Requests to the host ----

/** Backlight 0..255. UIFlow startup sets 90 % = 230. */
export let brightness: i32 = 230;
/** Speaker volume 0..255. The StickS3 startup sets 100. */
export let volume: i32 = 100;
/** Incremented for each tone request; the host plays toneHz for toneMs. */
export let toneSeq: i32 = 0;
export let toneHz: i32 = 0;
export let toneMs: i32 = 0;
/** True while the Setup app runs its access point and captive portal. */
export let setupActive: boolean = false;

export function requestTone(hz: i32, ms: i32): void {
  toneHz = hz;
  toneMs = ms;
  toneSeq = toneSeq + 1;
}

export function requestSetup(active: boolean): void {
  setupActive = active;
}
