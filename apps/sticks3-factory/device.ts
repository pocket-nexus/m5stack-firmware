// Device state module (docs/APP-HOST.md): every host-provided value and every
// request to the host. The board publishes values under these field names
// with m5_pocket_bridge_set_*; in JS mode main.tsx delivers them through the
// host* functions, in AOT mode the Rust host writes the fields directly.
// Views and models reach the hardware only through this module. Before the
// first publication the fields hold the defaults below: a StickS3 on battery
// with no Wi-Fi configured and no UIFlow cloud service.
import { clear, copy, i32, push, type f32, type f64 } from "@pocketjs/framework/solid/std";

// Wi-Fi station status, after MicroPython network.WLAN.status().
export const WIFI_IDLE: i32 = 0;
export const WIFI_CONNECTING: i32 = 1;
export const WIFI_GOT_IP: i32 = 2;
export const WIFI_FAILED: i32 = 3;

// UIFlow M5Things cloud status: -1 when the firmware has no cloud client
// (UIFlow's _HAS_SERVER = False), otherwise M5Things.status().
export const CLOUD_UNAVAILABLE: i32 = -1;
export const CLOUD_CONNECTED: i32 = 2;

// ---- Board -> app ----

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

// ---- App -> board ----

// Request strings "<name> <arg>...", drained after each frame:
//   tone <hz> <ms>      play a tone (Speaker.tone)
//   volume <0..255>     speaker master volume
//   brightness <0..255> backlight
//   setup <0|1>         stop / start the Setup access point and portal
export let requests: string[] = [];

export function request(command: string): void {
  push(requests, command);
}

export function requestTone(hz: i32, ms: i32): void {
  request(`tone ${hz} ${ms}`);
}

export function requestVolume(value: i32): void {
  request(`volume ${value}`);
}

export function requestBrightness(value: i32): void {
  request(`brightness ${value}`);
}

export function requestSetup(active: boolean): void {
  request(active ? "setup 1" : "setup 0");
}

// ---- Host entry points, re-exported by app.ts for main.tsx ----

export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "batteryLevel":
      batteryLevel = i32(value);
      break;
    case "charging":
      charging = value !== 0;
      break;
    case "wifiConfigured":
      wifiConfigured = value !== 0;
      break;
    case "wifiStatus":
      wifiStatus = i32(value);
      break;
    case "cloudStatus":
      cloudStatus = i32(value);
      break;
    case "apStations":
      apStations = i32(value);
      break;
    case "portalClients":
      portalClients = i32(value);
      break;
  }
}

export function hostText(name: string, value: string): void {
  switch (name) {
    case "accessCode":
      accessCode = value;
      break;
    case "nickName":
      nickName = value;
      break;
    case "apSsid":
      apSsid = value;
      break;
    case "firmwareVersion":
      firmwareVersion = value;
      break;
  }
}

// The StickS3 startup publishes no arrays and posts no events.
export function hostArray(name: string, values: f32[]): void {}

export function hostEvent(name: string, value: f64): void {}

export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}
