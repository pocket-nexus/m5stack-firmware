// Key and pointer tracking on the raw device fields.
//
// Keys follow the factory KeyManager over M5Unified's Button_Class: a click
// is a release before the 500 ms hold time; holding A and B together is
// GoHome, latched until both keys are released. Every key press plays the
// key tone and vibrates when the button settings allow it.
import { abs, f64, pow, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const KEY_NONE: i32 = 0;
export const KEY_PREVIOUS: i32 = 1;
export const KEY_NEXT: i32 = 2;
export const KEY_HOME: i32 = 3;

const HOLD_MS: f64 = 500.0;

/** The key event of the current frame. */
export let keyEvent: i32 = 0;
/** A or B pressed down in this frame (for press-time actions). */
export let keyAPressed: boolean = false;
export let keyBPressed: boolean = false;
export let keyAHeld: boolean = false;
export let keyBHeld: boolean = false;

let lastA: boolean = false;
let lastB: boolean = false;
let pressA: f64 = 0.0;
let pressB: f64 = 0.0;
let homeLatched: boolean = false;
let clickDueA = 0.0;
let clickDueB = 0.0;
/** Clicks injected by the guest's ActionHandler when no bridge reports raw keys. */
let pendingClick: i32 = 0;

export function injectClick(event: i32): void {
  pendingClick = event;
}

export function keyFeedback(isA: boolean): void {
  if (device.buttonSfx) {
    device.playTone(isA ? 94 : 96, 0.02);
  }
  if (device.buttonVibrate) {
    device.vibrate(20, 60);
  }
}

export function updateKeys(): void {
  const now = device.uptimeMs;
  const a = device.keyA;
  const b = device.keyB;
  keyEvent = KEY_NONE;
  keyAPressed = a && !lastA;
  keyBPressed = b && !lastB;
  if (keyAPressed) {
    pressA = now;
    keyFeedback(true);
  }
  if (keyBPressed) {
    pressB = now;
    keyFeedback(false);
  }
  keyAHeld = a && now - pressA >= HOLD_MS;
  keyBHeld = b && now - pressB >= HOLD_MS;
  const releasedA = !a && lastA;
  const releasedB = !b && lastB;
  if (keyAHeld && keyBHeld) {
    if (!homeLatched) {
      keyEvent = KEY_HOME;
      homeLatched = true;
    }
  } else if (clickDueA > 0.0 && now >= clickDueA) {
    keyEvent = KEY_PREVIOUS;
    clickDueA = 0.0;
  } else if (clickDueB > 0.0 && now >= clickDueB) {
    keyEvent = KEY_NEXT;
    clickDueB = 0.0;
  } else if (!a && !b) {
    homeLatched = false;
  }
  // Button_Class reports a click once the hold time has passed after a
  // short press without another press (a second press makes it a
  // multi-click, which no factory screen uses).
  if (releasedA) {
    clickDueA = now - pressA < HOLD_MS && !homeLatched ? now + HOLD_MS : 0.0;
  }
  if (releasedB) {
    clickDueB = now - pressB < HOLD_MS && !homeLatched ? now + HOLD_MS : 0.0;
  }
  if (keyAPressed) {
    clickDueA = 0.0;
  }
  if (keyBPressed) {
    clickDueB = 0.0;
  }
  lastA = a;
  lastB = b;
  if (pendingClick !== KEY_NONE) {
    if (keyEvent === KEY_NONE) {
      keyEvent = pendingClick;
    }
    pendingClick = KEY_NONE;
  }
}

// ------------------------------- pointer ------------------------------------

/** Press and release edges of the current frame. */
export let pointerPressed: boolean = false;
export let pointerReleased: boolean = false;
export let pointerDown: boolean = false;
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;
export let startX: i32 = 0;
export let startY: i32 = 0;
export let startMs: f64 = 0.0;
/** The contact moved further than the 10 px LVGL scroll limit. */
export let moved: boolean = false;
/** Horizontal velocity in px per ms, averaged over the last samples. */
export let velocityX: f64 = 0.0;
export let velocityY: f64 = 0.0;

let wasDown: boolean = false;
let lastX: i32 = 0;
let lastY: i32 = 0;
let lastMs: f64 = 0.0;

export function updatePointer(): void {
  const down = device.pointerDown;
  pointerPressed = down && !wasDown;
  pointerReleased = !down && wasDown;
  pointerDown = down;
  if (down) {
    pointerX = device.pointerX;
    pointerY = device.pointerY;
  }
  const now = device.uptimeMs;
  if (pointerPressed) {
    startX = pointerX;
    startY = pointerY;
    startMs = now;
    moved = false;
    velocityX = 0.0;
    velocityY = 0.0;
    lastX = pointerX;
    lastY = pointerY;
    lastMs = now;
  } else if (down) {
    if (abs(pointerX - startX) > 10 || abs(pointerY - startY) > 10) {
      moved = true;
    }
    const dt = now - lastMs;
    if (dt > 0.0) {
      // Exponential average with LVGL's sample decay: the average keeps 0.6
      // of its value per 33 ms, whatever the time between two frames.
      const vx = f64(pointerX - lastX) / dt;
      const vy = f64(pointerY - lastY) / dt;
      const keep = pow(f64(0.6), dt / 33.333);
      velocityX = velocityX * keep + vx * (1.0 - keep);
      velocityY = velocityY * keep + vy * (1.0 - keep);
    }
    lastX = pointerX;
    lastY = pointerY;
    lastMs = now;
  }
  wasDown = down;
}

