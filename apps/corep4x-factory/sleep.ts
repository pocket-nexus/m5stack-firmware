// SLEEP window (ui_function_sleep_window.cpp): wake sources and Deep Sleep.
// The settings live in RAM only and return to their defaults at every boot
// (BTN on, RTC off with 60 s, Touch off, IMU off). The PM1 power button
// always wakes the board; "BTN" decides whether a single click of it puts
// the board to sleep.
import type { i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const MUTED: i32 = 0;
export const SUCCESS: i32 = 1;
export const WARNING: i32 = 2;
export const ERROR: i32 = 3;

export let buttonSleep: boolean = true;
export let rtcWake: boolean = false;
export let rtcSeconds: i32 = 60;
export let touchWake: boolean = false;
export let imuWake: boolean = false;
export let running: boolean = false;
export let result: string = "Set wake sources, then tap Deep Sleep.";
export let resultTone: i32 = MUTED;

export function open(): void {
  result = "Set wake sources, then tap Deep Sleep.";
  resultTone = MUTED;
}

function report(name: string, on: boolean): void {
  result = `${name} set to ${on ? "ON" : "OFF"}.`;
  resultTone = SUCCESS;
}

/** The board keeps the wake sources for a sleep the PM1 button starts. */
function sendWake(): void {
  device.request(`sleepWake ${rtcWake ? rtcSeconds : 0} ${touchWake ? 1 : 0} ${imuWake ? 1 : 0}`);
}

export function toggleButtonSleep(): void {
  if (running) {
    return;
  }
  buttonSleep = !buttonSleep;
  device.request(`sleepButton ${buttonSleep ? 1 : 0}`);
  report("BTN sleep", buttonSleep);
}

export function toggleRtc(): void {
  if (running) {
    return;
  }
  rtcWake = !rtcWake;
  sendWake();
  report("RTC wake", rtcWake);
}

export function setSeconds(seconds: i32): void {
  if (running) {
    return;
  }
  rtcSeconds = seconds;
  rtcWake = true;
  sendWake();
  result = `RTC wake enabled after ${seconds} seconds.`;
  resultTone = SUCCESS;
}

export function toggleTouch(): void {
  if (running) {
    return;
  }
  touchWake = !touchWake;
  sendWake();
  report("Touch wake", touchWake);
}

export function toggleImu(): void {
  if (running) {
    return;
  }
  imuWake = !imuWake;
  sendWake();
  report("IMU wake", imuWake);
}

export function deepSleep(): void {
  if (running) {
    return;
  }
  running = true;
  result = "Preparing rails, touch, RTC and wake sources...";
  resultTone = WARNING;
  device.request(`deepSleep ${rtcWake ? rtcSeconds : 0} ${touchWake ? 1 : 0} ${imuWake ? 1 : 0}`);
}

/** The board could not enter deep sleep (an esp_err_t name). */
export function failed(error: string): void {
  running = false;
  result = `Deep sleep failed: ${error}.`;
  resultTone = ERROR;
}

export function statusText(): string {
  return `Task: ${running ? "preparing" : "idle"}  |  PM1 button wake always on`;
}
