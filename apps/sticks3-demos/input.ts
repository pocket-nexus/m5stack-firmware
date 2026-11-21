// Key decoding on the raw device fields, once per frame.
//
// A press is the level edge of keyFront/keySide, or a new interrupt
// timestamp (frontPressMs/sidePressMs) when the key went down and up between
// two frames. `*PressAt` is the interrupt time when the board supplied one,
// else the frame time. A click is a release before HOLD_MS; holding the front
// key for LONG_MS (or a PMIC power-key click) asks for the home screen.
import { type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const HOLD_MS: f64 = 450.0;
export const LONG_MS: f64 = 700.0;
export const REPEAT_MS: f64 = 220.0;
/** Front-key hold time after which the home progress bar shows. */
export const HINT_MS: f64 = 180.0;

// ---- events of the current frame ----
export let frontPressed: boolean = false;
export let frontPressAt: f64 = 0.0;
export let frontReleased: boolean = false;
export let frontClick: boolean = false;
export let frontHeldMs: f64 = 0.0;
export let sidePressed: boolean = false;
export let sidePressAt: f64 = 0.0;
export let sideReleased: boolean = false;
export let sideClick: boolean = false;
/** The side key passed HOLD_MS (once per press). */
export let sideHold: boolean = false;
/** Fires at HOLD_MS and then every REPEAT_MS while the side key is held. */
export let sideRepeat: boolean = false;
export let homeRequested: boolean = false;
/** Any key edge this frame (wakes the dimmed backlight). */
export let anyKey: boolean = false;

// ---- state ----
let frontDown: boolean = false;
let frontSince: f64 = 0.0;
let frontLongFired: boolean = false;
let frontStamp: f64 = 0.0;
let sideDown: boolean = false;
let sideSince: f64 = 0.0;
let sideHoldFired: boolean = false;
let sideNextRepeat: f64 = 0.0;
let sideStamp: f64 = 0.0;
let powerSeen: i32 = 0;

/** Clears every event (after a page change, so a press does not act twice). */
export function swallow(): void {
  frontPressed = false;
  frontReleased = false;
  frontClick = false;
  sidePressed = false;
  sideReleased = false;
  sideClick = false;
  sideHold = false;
  sideRepeat = false;
  homeRequested = false;
  // A key still held after a page change must be released before it acts.
  frontLongFired = frontDown;
  sideHoldFired = sideDown;
}

export function update(now: f64): void {
  frontPressed = false;
  frontReleased = false;
  frontClick = false;
  sidePressed = false;
  sideReleased = false;
  sideClick = false;
  sideHold = false;
  sideRepeat = false;
  homeRequested = false;

  // Front key.
  const front = device.keyFront;
  const frontNewStamp = device.frontPressMs > frontStamp + 0.5;
  frontStamp = device.frontPressMs;
  if (!frontDown && (front || frontNewStamp)) {
    frontPressed = true;
    frontPressAt = frontNewStamp ? device.frontPressMs : now;
    frontSince = frontPressAt;
    frontLongFired = false;
    frontDown = true;
  }
  if (frontDown && !front) {
    frontReleased = true;
    frontClick = !frontLongFired && now - frontSince < HOLD_MS;
    frontDown = false;
  }
  frontHeldMs = frontDown ? now - frontSince : 0.0;
  if (frontDown && !frontLongFired && frontHeldMs >= LONG_MS) {
    frontLongFired = true;
    homeRequested = true;
  }

  // Side key.
  const side = device.keySide;
  const sideNewStamp = device.sidePressMs > sideStamp + 0.5;
  sideStamp = device.sidePressMs;
  if (!sideDown && (side || sideNewStamp)) {
    sidePressed = true;
    sidePressAt = sideNewStamp ? device.sidePressMs : now;
    sideSince = sidePressAt;
    sideHoldFired = false;
    sideDown = true;
  }
  if (sideDown && !side) {
    sideReleased = true;
    sideClick = !sideHoldFired && now - sideSince < HOLD_MS;
    sideDown = false;
  }
  if (sideDown && now - sideSince >= HOLD_MS) {
    if (!sideHoldFired) {
      sideHoldFired = true;
      sideHold = true;
      sideRepeat = true;
      sideNextRepeat = now + REPEAT_MS;
    } else if (now >= sideNextRepeat) {
      sideRepeat = true;
      sideNextRepeat = sideNextRepeat + REPEAT_MS;
    }
  }

  // PMIC power key click.
  if (device.powerClicks !== powerSeen) {
    powerSeen = device.powerClicks;
    homeRequested = true;
  }

  anyKey = frontPressed || sidePressed || homeRequested;
}

/** 0..1 progress of the front-key hold towards LONG_MS, for the home bar. */
export function homeProgress(): f64 {
  if (frontHeldMs < HINT_MS || frontLongFired) {
    return 0.0;
  }
  const p = (frontHeldMs - HINT_MS) / (LONG_MS - HINT_MS);
  return p > 1.0 ? 1.0 : p;
}
