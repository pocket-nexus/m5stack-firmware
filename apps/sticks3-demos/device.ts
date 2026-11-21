// Device state module (docs/APP-HOST.md): every host-provided value and every
// request to the host. The board publishes values under these field names
// with m5_pocket_bridge_*; in JS mode main.tsx delivers them through the
// host* functions, in AOT mode the Rust host writes the fields directly and
// never calls them, so they only assign. Models reach the hardware only
// through this module.
//
// The board publishes `bridged` (1) before the application starts. Without
// it, app.ts advances `uptimeMs` itself by one frame per frame (desktop
// previews).
import { clear, copy, f64, i32, len, push, type f32 } from "@pocketjs/framework/solid/std";

// Slots of the `best` array (NVS namespace "demos", key "best").
export const BEST_REACTION: i32 = 0; // fastest reaction, ms (0: none)
export const BEST_SNAKE: i32 = 1; // longest snake, segments
export const BEST_MAZE: i32 = 2; // fastest maze per level 1..5, ms: slots 2..6
export const BEST_SLOTS: i32 = 7;

// ------------------------------ board → app --------------------------------

/** True when a board publishes the fields below (always on the StickS3). */
export let bridged: boolean = false;
/** esp_timer time of this frame's before_frame hook, ms. */
export let uptimeMs: f64 = 0.0;
/** esp_timer time at which the previous frame was handed to the panel, ms. */
export let presentedMs: f64 = 0.0;

/** KEY1 (front) and KEY2 (side) held at this frame. */
export let keyFront: boolean = false;
export let keySide: boolean = false;
/** esp_timer time of each key's last press edge (GPIO interrupt), ms. */
export let frontPressMs: f64 = 0.0;
export let sidePressMs: f64 = 0.0;
/** PMIC power key single clicks since boot (board event `powerKey`). */
export let powerClicks: i32 = 0;

/** Battery level 0..100 (-1: PMIC not readable) and charger state. */
export let batteryLevel: i32 = 76;
export let charging: boolean = false;

/** BMI270 in M5Unified's StickS3 frame: acceleration in g, rotation in °/s. */
export let imuOk: boolean = false;
export let accelX: f64 = 0.0;
export let accelY: f64 = 0.0;
export let accelZ: f64 = 1.0;
export let gyroX: f64 = 0.0;
export let gyroY: f64 = 0.0;
export let gyroZ: f64 = 0.0;

/** Microphone level of the samples since the previous frame, dBFS. */
export let micOn: boolean = false;
export let micDb: f64 = -96.0;
export let micPeakDb: f64 = -96.0;

/** Random seed from esp_random at boot. */
export let seed: f64 = 20261121.0;

/** Stored best scores, BEST_* slots. */
export let best: i32[] = [0, 0, 0, 0, 0, 0, 0];

/** Without a board: advances the clock by one frame. */
export function advance(ms: f64): void {
  uptimeMs = uptimeMs + ms;
}

// ------------------------------ app → board --------------------------------

// Request strings "<name> <arg>...", drained after each frame:
//   tone <hz> <ms> [atMs]   sine tone, started at uptime atMs when given
//   brightness <0..255>     backlight
//   volume <0..255>         speaker master volume
//   mic <0|1>               stop / start microphone capture
//   best <slot> <value>     store a best score in NVS
export let requests: string[] = [];

export function request(command: string): void {
  push(requests, command);
}

export function tone(hz: i32, ms: i32): void {
  request(`tone ${hz} ${ms}`);
}

/** A tone that starts at uptime `atMs` (board-timed, not frame-timed). */
export function toneAt(hz: i32, ms: i32, atMs: f64): void {
  request(`tone ${hz} ${ms} ${fixed1(atMs)}`);
}

export function setBrightness(value: i32): void {
  request(`brightness ${value}`);
}

export function setVolume(value: i32): void {
  request(`volume ${value}`);
}

export function setMic(on: boolean): void {
  request(on ? "mic 1" : "mic 0");
}

/** Records a best score locally and asks the board to store it. */
export function saveBest(slot: i32, value: i32): void {
  if (slot < 0 || slot >= len(best)) {
    return;
  }
  best[slot] = value;
  request(`best ${slot} ${value}`);
}

function fixed1(value: f64): string {
  const whole = i32(value);
  const tenth = i32((value - f64(whole)) * 10.0);
  return `${whole}.${tenth}`;
}

// ------------------------------ host entry points --------------------------

export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "bridged":
      bridged = value !== 0.0;
      break;
    case "uptimeMs":
      uptimeMs = value;
      break;
    case "presentedMs":
      presentedMs = value;
      break;
    case "keyFront":
      keyFront = value !== 0.0;
      break;
    case "keySide":
      keySide = value !== 0.0;
      break;
    case "frontPressMs":
      frontPressMs = value;
      break;
    case "sidePressMs":
      sidePressMs = value;
      break;
    case "batteryLevel":
      batteryLevel = i32(value);
      break;
    case "charging":
      charging = value !== 0.0;
      break;
    case "imuOk":
      imuOk = value !== 0.0;
      break;
    case "accelX":
      accelX = value;
      break;
    case "accelY":
      accelY = value;
      break;
    case "accelZ":
      accelZ = value;
      break;
    case "gyroX":
      gyroX = value;
      break;
    case "gyroY":
      gyroY = value;
      break;
    case "gyroZ":
      gyroZ = value;
      break;
    case "micOn":
      micOn = value !== 0.0;
      break;
    case "micDb":
      micDb = value;
      break;
    case "micPeakDb":
      micPeakDb = value;
      break;
    case "seed":
      seed = value;
      break;
    default:
      break;
  }
}

export function hostText(name: string, value: string): void {}

export function hostArray(name: string, values: f32[]): void {
  switch (name) {
    case "best":
      clear(best);
      for (const v of values) {
        push(best, i32(v));
      }
      break;
    default:
      break;
  }
}

export function hostEvent(name: string, value: f64): void {
  switch (name) {
    case "powerKey":
      powerClicks = powerClicks + 1;
      break;
    default:
      break;
  }
}

export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}
