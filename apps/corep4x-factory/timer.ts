// TIMER window (ui_function_timer_window.cpp): the RX8130CE date and time
// editor. Six tiles (Year, Month, Day, Hour, Minute, Second) follow the RTC
// every second until a +/- is pressed; then they freeze until Save writes
// the whole edit buffer (both Save buttons write date and time). A value is
// clamped, never wrapped. +/- acts on release; held, it repeats after
// 400 ms and every 100 ms. The NTP button appears when an SNTP sync over
// Ethernet worked and copies the system clock (UTC+8) to the RTC.
import { i32, idiv, imod } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const YEAR: i32 = 0;
export const MONTH: i32 = 1;
export const DAY: i32 = 2;
export const HOUR: i32 = 3;
export const MINUTE: i32 = 4;
export const SECOND: i32 = 5;

/** The edit buffer: year, month 1..12, day, hour, minute, second. */
export let values: i32[] = [2026, 1, 1, 0, 0, 0];
export let dirty: boolean = false;

/** Screen rectangles of the 12 +/- buttons: field f has "-" at index 2f
 *  and "+" at 2f + 1. */
const BUTTON_LEFT: i32[] = [42, 120, 192, 252, 324, 384, 42, 108, 180, 246, 318, 384];
const BUTTON_TOP: i32[] = [198, 198, 198, 198, 198, 198, 396, 396, 396, 396, 396, 396];

let held: i32 = -1;
let heldFrames: i32 = 0;
let wasDown: boolean = false;

function leap(year: i32): boolean {
  return (imod(year, 4) === 0 && imod(year, 100) !== 0) || imod(year, 400) === 0;
}

function daysIn(year: i32, month: i32): i32 {
  if (month === 2) {
    return leap(year) ? 29 : 28;
  }
  return month === 4 || month === 6 || month === 9 || month === 11 ? 30 : 31;
}

function clampField(value: i32, low: i32, high: i32): i32 {
  return value < low ? low : value > high ? high : value;
}

/** normalize_edit_time: clamp each field into its range. */
function normalize(): void {
  values[YEAR] = clampField(values[YEAR], 2020, 2099);
  values[MONTH] = clampField(values[MONTH], 1, 12);
  values[DAY] = clampField(values[DAY], 1, daysIn(values[YEAR], values[MONTH]));
  values[HOUR] = clampField(values[HOUR], 0, 23);
  values[MINUTE] = clampField(values[MINUTE], 0, 59);
  values[SECOND] = clampField(values[SECOND], 0, 59);
}

export function open(): void {
  dirty = false;
  held = -1;
  follow();
}

/** While not edited, the tiles show the RTC (or the image's build time
 *  when there is no RTC). */
function follow(): void {
  if (dirty) {
    return;
  }
  values = [device.rtcYear, device.rtcMonth, device.rtcDay, device.rtcHour, device.rtcMinute, device.rtcSecond];
  normalize();
}

function adjust(field: i32, delta: i32): void {
  values[field] = values[field] + delta;
  dirty = true;
  normalize();
}

function hit(x: i32, y: i32): i32 {
  for (let i: i32 = 0; i < 12; i++) {
    if (x >= BUTTON_LEFT[i] && x < BUTTON_LEFT[i] + 50 && y >= BUTTON_TOP[i] && y < BUTTON_TOP[i] + 34) {
      return i;
    }
  }
  return -1;
}

function apply(button: i32): void {
  adjust(idiv(button, 2), imod(button, 2) === 1 ? 1 : -1);
}

/** One frame: follow the RTC, and the +/- buttons from the contact. */
export function step(x: i32, y: i32, down: boolean, enabled: boolean): void {
  follow();
  if (!enabled) {
    held = -1;
    wasDown = down;
    return;
  }
  if (down && !wasDown) {
    held = hit(x, y);
    heldFrames = 0;
  } else if (down && held >= 0) {
    heldFrames = heldFrames + 1;
    // LONG_PRESSED at 400 ms, LONG_PRESSED_REPEAT every 100 ms.
    if (heldFrames === 12 || (heldFrames > 12 && imod(heldFrames - 12, 3) === 0)) {
      apply(held);
    }
  } else if (!down && wasDown && held >= 0) {
    // CLICKED on release, also after a long press, if still on the button.
    if (hit(x, y) === held) {
      apply(held);
    }
    held = -1;
  }
  wasDown = down;
}

export function save(): void {
  if (!device.rtcReady) {
    return;
  }
  normalize();
  device.request(`rtcSet ${values[YEAR]} ${values[MONTH]} ${values[DAY]} ${values[HOUR]} ${values[MINUTE]} ${values[SECOND]}`);
  dirty = false;
}

export function ntp(): void {
  if (device.ntpReady) {
    device.request("rtcNtp");
    dirty = false;
  }
}

/** "%04d" for the year, "%02d" for the others. */
export function text(field: i32): string {
  const value = values[field];
  if (field === YEAR) {
    return `${value}`;
  }
  return `${value < 10 ? "0" : ""}${value}`;
}
