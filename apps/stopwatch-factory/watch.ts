// WatchFace app (factory app_watch_face): classic, number flow, big number
// and simple faces. The face and its theme reset to the first entry on every
// open, as in the factory; nothing is saved.
import { abs, atan2, cos, f64, round, sin, sqrt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import { SPRING_SECOND_DOT, springConfigure, springPos, springTeleport, springTo } from "./motion";

export const FACE_CLASSIC: i32 = 0;
export const FACE_NUMBER_FLOW: i32 = 1;
export const FACE_BIG_NUMBER: i32 = 2;
export const FACE_SIMPLE: i32 = 3;

export let face: i32 = 0;
/** Classic display mode 0..2, number flow / simple theme 0..9, palette 0..3. */
export let classicMode: i32 = 0;
export let flowTheme: i32 = 0;
export let bigPalette: i32 = 0;
export let simpleTheme: i32 = 0;
export let simpleDot: boolean = true;
export let simpleTips: boolean = true;
let createdAt = 0.0;
let lastRefresh = -10000.0;
let lastDotAngle: f64 = 0.0;
let suppressClick: boolean = false;
let longPressArmed: boolean = false;

/** Whole-face rotation from the accelerometer, degrees. */
export let rotation: f64 = 0.0;
let filterX: f64 = 0.0;
let filterY: f64 = -1.0;
let swipeAngle: f64 = 0.0;

/** Displayed time, refreshed when more than a second has passed. */
export let shownHour: i32 = 0;
export let shownMinute: i32 = 0;
export let shownSecond: i32 = 0;

export function open(): void {
  face = FACE_CLASSIC;
  rotation = 0.0;
  filterX = 0.0;
  filterY = -1.0;
  createFace();
}

function createFace(): void {
  classicMode = 0;
  flowTheme = 0;
  bigPalette = 0;
  simpleTheme = 0;
  simpleDot = true;
  simpleTips = true;
  createdAt = device.uptimeMs;
  lastRefresh = -10000.0;
  springConfigure(SPRING_SECOND_DOT, 0.3, 0.4);
  springTeleport(SPRING_SECOND_DOT, 0.0);
  lastDotAngle = 0.0;
  suppressClick = false;
}

export function next(direction: i32): void {
  face = (face + direction + 4) % 4;
  createFace();
}

/** Tap on the face: next mode, theme or palette. */
export function tap(): void {
  if (suppressClick) {
    suppressClick = false;
    return;
  }
  if (face === FACE_CLASSIC) {
    classicMode = (classicMode + 1) % 3;
  } else if (face === FACE_NUMBER_FLOW) {
    flowTheme = (flowTheme + 1) % 10;
  } else if (face === FACE_BIG_NUMBER) {
    bigPalette = (bigPalette + 1) % 4;
  } else {
    simpleTheme = (simpleTheme + 1) % 10;
  }
}

function updateRotation(): void {
  const x = device.accelX;
  const y = device.accelY;
  const magnitude = sqrt(x * x + y * y);
  if (magnitude < 0.3) {
    return;
  }
  filterX = filterX + (x / magnitude - filterX) * 0.5;
  filterY = filterY + (y / magnitude - filterY) * 0.5;
  const target: f64 = atan2(filterX, -filterY) * 57.29578;
  let delta: f64 = target - rotation;
  while (delta > 180.0) {
    delta = delta - 360.0;
  }
  while (delta < -180.0) {
    delta = delta + 360.0;
  }
  if (abs(delta) < 0.25) {
    return;
  }
  rotation = rotation + delta;
  if (rotation > 180.0) {
    rotation = rotation - 360.0;
  }
  if (rotation < -180.0) {
    rotation = rotation + 360.0;
  }
}

/** One frame; returns true when the app should close. */
export function update(): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    return true;
  }
  if (input.keyEvent === input.KEY_PREVIOUS) {
    next(-1);
  } else if (input.keyEvent === input.KEY_NEXT) {
    next(1);
  }
  updateRotation();
  // Swipe between faces, measured in the rotated frame captured at press.
  if (input.pointerPressed) {
    swipeAngle = rotation;
    longPressArmed = face === FACE_SIMPLE;
  }
  if (input.pointerDown && longPressArmed && !input.moved && device.uptimeMs - input.startMs >= 400.0) {
    longPressArmed = false;
    simpleDot = !simpleDot;
    suppressClick = true;
  }
  if (input.pointerReleased) {
    longPressArmed = false;
    const radians: f64 = swipeAngle * 0.017453292;
    const dx: f64 = f64(input.pointerX - input.startX);
    const dy: f64 = f64(input.pointerY - input.startY);
    const c: f64 = cos(radians);
    const s: f64 = sin(radians);
    const rx: f64 = c * dx + s * dy;
    const ry: f64 = -s * dx + c * dy;
    if (abs(rx) >= 60.0 && abs(rx) > abs(ry)) {
      suppressClick = true;
      next(rx < 0.0 ? 1 : -1);
    }
  }
  if (face === FACE_SIMPLE && simpleTips && device.uptimeMs - createdAt >= 3000.0) {
    simpleTips = false;
  }
  if (device.uptimeMs - lastRefresh > 1000.0) {
    lastRefresh = device.uptimeMs;
    shownHour = device.hour;
    shownMinute = device.minute;
    shownSecond = device.second;
    if (face === FACE_SIMPLE) {
      const target: f64 = f64(shownSecond * 6);
      let delta: f64 = target - lastDotAngle;
      if (springPos[SPRING_SECOND_DOT] !== 0.0 || lastDotAngle !== 0.0) {
        while (delta > 180.0) {
          delta = delta - 360.0;
        }
        while (delta < -180.0) {
          delta = delta + 360.0;
        }
      }
      lastDotAngle = lastDotAngle + delta;
      springTo(SPRING_SECOND_DOT, lastDotAngle);
    }
  }
  return false;
}

/** Classic hand angles in degrees (the artwork points to 3 o'clock). */
export function secondAngle(): f64 {
  return f64(shownSecond * 6 - 90);
}

export function minuteAngle(): f64 {
  return (f64(shownMinute) + f64(shownSecond) / 60.0) * 6.0 - 90.0;
}

export function hourAngle(): f64 {
  return (f64(shownHour % 12) + f64(shownMinute) / 60.0 + f64(shownSecond) / 3600.0) * 30.0 - 90.0;
}

/** Simple-face second dot centre offset from the screen centre. */
export function dotX(): i32 {
  const angle: f64 = (springPos[SPRING_SECOND_DOT] - 90.0) * 0.017453292;
  return round(214.0 * cos(angle));
}

export function dotY(): i32 {
  const angle: f64 = (springPos[SPRING_SECOND_DOT] - 90.0) * 0.017453292;
  return round(214.0 * sin(angle));
}
