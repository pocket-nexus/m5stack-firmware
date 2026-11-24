// The function windows and the 14 touch areas of the function screen
// (screens/ui_function.c, ui_function_window.cpp). A window opens from the
// centre of the touched area: its backdrop grows from 1x1 there to the
// 480x480 screen over 220 ms (ease-out) while its opacity rises from 0 to
// 255, and its parts keep their places relative to the backdrop, so they
// travel with its corner and are clipped by it. Exit shrinks the backdrop
// into the Exit button's centre over 180 ms (ease-in) while it fades out.
import { f32, i32 } from "@pocketjs/framework/solid/std";
import { easeIn, easeOut, lerp, progress } from "./ease";
import { EXIT_CENTER_X, EXIT_CENTER_Y } from "./frame";

export const POWER: i32 = 0;
export const TOUCH: i32 = 1;
export const SD: i32 = 2;
export const I2C: i32 = 3;
export const IMU: i32 = 4;
export const REC: i32 = 5;
export const PING: i32 = 6;
export const TIMER: i32 = 7;
export const WIFI: i32 = 8;
export const USB: i32 = 9;
export const IR: i32 = 10;
export const MUSIC: i32 = 11;
export const SLEEP: i32 = 12;
export const OFF: i32 = 13;
export const COUNT: i32 = 14;

/** Touch areas: left, top, width, height on the screen (LV_ALIGN_CENTER
 *  offsets of ui_function.c turned into corners). */
export const AREA_LEFT: i32[] = [15, 130, 245, 360, 15, 130, 245, 360, 15, 130, 245, 360, 290, 398];
export const AREA_TOP: i32[] = [60, 60, 60, 60, 195, 195, 195, 195, 330, 330, 330, 330, 2, 2];
export const AREA_WIDTH: i32[] = [100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 80, 80];
export const AREA_HEIGHT: i32[] = [100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 55, 55];

/** Pages of a paged window (0: single). */
export const PAGES: i32[] = [3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

const OPEN_MS: f32 = 220;
const CLOSE_MS: f32 = 180;

/** The open window, -1 for none. */
export let open: i32 = -1;
/** 1 opening, 0 open, -1 closing. */
export let phase: i32 = 0;
let frames: i32 = 0;
let originX: f32 = 240;
let originY: f32 = 240;
let fromX: f32 = 0;
let fromY: f32 = 0;
let fromW: f32 = 480;
let fromH: f32 = 480;

/** Backdrop geometry and opacity this frame. */
export let left: f32 = 0;
export let top: f32 = 0;
export let width: f32 = 480;
export let height: f32 = 480;
export let opacity: f32 = 1;

/** Set when a window finished closing this frame. */
export let closed: i32 = -1;

export function openWindow(id: i32): void {
  open = id;
  phase = 1;
  frames = 0;
  // get_trigger_center: (x1 + x2) / 2 of the area's coordinates, x2
  // inclusive.
  originX = f32((AREA_LEFT[id]) * 2 + (AREA_WIDTH[id]) - 1) * f32(0.5);
  originY = f32((AREA_TOP[id]) * 2 + (AREA_HEIGHT[id]) - 1) * f32(0.5);
  originX = f32(i32(originX));
  originY = f32(i32(originY));
  apply();
}

/** The default Exit: the close animation into the Exit button. */
export function closeWindow(): void {
  if (open < 0 || phase === -1) {
    return;
  }
  phase = -1;
  frames = 0;
  fromX = left;
  fromY = top;
  fromW = width;
  fromH = height;
  apply();
}

/** Deletes the window at once, without the close animation. */
export function dropWindow(): void {
  if (open >= 0) {
    closed = open;
  }
  open = -1;
  phase = 0;
}

function apply(): void {
  if (phase === 1) {
    const t: f32 = easeOut(progress(frames, OPEN_MS));
    left = lerp(originX, 0, t);
    top = lerp(originY, 0, t);
    width = lerp(1, 480, t);
    height = lerp(1, 480, t);
    opacity = t;
  } else if (phase === -1) {
    const t: f32 = easeIn(progress(frames, CLOSE_MS));
    left = lerp(fromX, f32(EXIT_CENTER_X), t);
    top = lerp(fromY, f32(EXIT_CENTER_Y), t);
    width = lerp(fromW, 1, t);
    height = lerp(fromH, 1, t);
    opacity = f32(1) - t;
  } else {
    left = 0;
    top = 0;
    width = 480;
    height = 480;
    opacity = 1;
  }
  // The animation writes whole pixels (int16_t) and at least 1x1.
  left = f32(i32(left));
  top = f32(i32(top));
  width = width < 1 ? 1 : f32(i32(width));
  height = height < 1 ? 1 : f32(i32(height));
}

export function step(): void {
  closed = -1;
  if (open < 0) {
    return;
  }
  if (phase === 1) {
    frames = frames + 1;
    if (progress(frames, OPEN_MS) >= 1) {
      phase = 0;
    }
  } else if (phase === -1) {
    frames = frames + 1;
    if (progress(frames, CLOSE_MS) >= 1) {
      closed = open;
      open = -1;
      phase = 0;
    }
  }
  apply();
}
