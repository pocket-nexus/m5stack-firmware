// OFF window (ui_function_off_window.cpp): the boot sound switch, Power Off
// and ESP Restart. Each action counts down 31 steps of 100 ms (the bar
// from 100 to 0, value i * 100 / 30) and can be cancelled with the same
// button; then the board turns the backlight off and sends the PM1 shutdown
// command, or restarts.
import { idiv, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const IDLE: i32 = 0;
export const POWER_OFF: i32 = 1;
export const RESTART: i32 = 2;

/** Text colours of the action line. */
export const MUTED: i32 = 0;
export const SUCCESS: i32 = 1;
export const WARNING: i32 = 2;
export const ERROR: i32 = 3;

export let running: boolean = false;
export let pending: i32 = IDLE;
let cancel: boolean = false;
let countdown: i32 = 0;
let stepFrames: i32 = 0;
export let bar: i32 = 0;
export let action: string = "Ready.";
export let actionTone: i32 = MUTED;

export function open(): void {
  running = false;
  pending = IDLE;
  cancel = false;
  bar = 0;
  action = "Ready.";
  actionTone = MUTED;
}

/** Closing the window cancels a countdown silently. */
export function close(): void {
  running = false;
  pending = IDLE;
}

export function toggleBootSound(): void {
  const next = !device.bootSound;
  device.setBootSound(next);
  action = next ? "Startup prompt sound enabled." : "Startup prompt sound disabled.";
  actionTone = SUCCESS;
}

function start(which: i32): void {
  running = true;
  cancel = false;
  pending = which;
  countdown = 30;
  stepFrames = 0;
  bar = 100;
  action = which === POWER_OFF ? "Power off in 3 s..." : "Restart in 3 s...";
  actionTone = WARNING;
}

export function pressPowerOff(): void {
  if (running && pending === POWER_OFF) {
    cancel = true;
  } else if (!running && device.pm1Ready) {
    start(POWER_OFF);
  }
}

export function pressRestart(): void {
  if (running && pending === RESTART) {
    cancel = true;
  } else if (!running) {
    start(RESTART);
  }
}

/** One frame: a countdown step every third frame (100 ms at 30 Hz). */
export function step(): void {
  if (!running || pending === IDLE) {
    return;
  }
  stepFrames = stepFrames + 1;
  if (stepFrames < 3) {
    return;
  }
  stepFrames = 0;
  if (cancel) {
    running = false;
    pending = IDLE;
    cancel = false;
    bar = 0;
    action = "Cancelled.";
    actionTone = MUTED;
    return;
  }
  countdown = countdown - 1;
  if (countdown >= 0) {
    bar = idiv(countdown * 100, 30);
    return;
  }
  const which = pending;
  pending = IDLE;
  action = "Turning display off...";
  actionTone = WARNING;
  device.request(which === POWER_OFF ? "powerOff" : "restart");
}

/** The board reports the shutdown command's result (0 ok, else the PM1
 *  error code). */
export function powerOffResult(code: i32): void {
  running = false;
  if (code === 0) {
    action = "PM1 OFF command sent. Powering down...";
    actionTone = SUCCESS;
  } else {
    action = `Power off failed: PM1 error ${code}.`;
    actionTone = ERROR;
  }
}

export function statusText(): string {
  return `PM1: ${device.pm1Ready ? "Ready" : "Not ready"}  PYB: ${device.pybReady ? "Ready" : "Not ready"}  Task: ${running ? "Running" : "Idle"}`;
}
