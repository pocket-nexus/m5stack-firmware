// Screen navigation of the factory UI (SquareLine screens/ui_startup.c,
// ui_info.c, ui_function.c): Startup in the middle, Info to its left,
// Function to its right. A swipe right on Startup loads Info, a swipe left
// loads Function; the way back is the opposite swipe. Each change is
// lv_screen_load_anim MOVE_LEFT/MOVE_RIGHT over 500 ms on the linear path:
// both screens slide by the screen width. LVGL fires the gesture while the
// finger moves, once the movement since the press (summed over reads that
// move at least 3 px) exceeds 50 px, then waits for the release. LVGL reads
// the touch controller every LV_DEF_REFR_PERIOD (33 ms); the recogniser
// samples the contact at that period, whatever the frame rate.
import { abs, f32, type i32 } from "@pocketjs/framework/solid/std";
import { framesIn, progress } from "./ease";

export const STARTUP: i32 = 0;
export const INFO: i32 = 1;
export const FUNCTION: i32 = 2;

const WIDTH: f32 = 480;
const MOVE_MS: f32 = 500;
const GESTURE_LIMIT: i32 = 50;
const GESTURE_MIN_VELOCITY: i32 = 3;
const READ_MS: i32 = 33;

/** The loaded screen, and during a move the screen that is leaving. */
export let current: i32 = STARTUP;
export let leaving: i32 = -1;
/** +1: the new screen comes from the right (MOVE_LEFT); -1 from the left. */
let direction: i32 = 1;
let moveFrames: i32 = 0;

let wasDown: boolean = false;
let lastX: i32 = 0;
let lastY: i32 = 0;
/** Frames since the last sample of the contact. */
let readFrames: i32 = 0;
let sumX: i32 = 0;
let sumY: i32 = 0;
/** A gesture fired during this contact: no further gesture, no taps. */
export let gestured: boolean = false;

export function moving(): boolean {
  return leaving >= 0;
}

/** A move that is still running is replaced: the screen it brought in
 *  leaves now. */
function load(next: i32, dir: i32): void {
  leaving = current;
  current = next;
  direction = dir;
  moveFrames = 0;
}

/** The x offset of a screen this frame, in pixels; 0 when it is idle. */
export function offset(screen: i32): f32 {
  if (leaving < 0) {
    return 0;
  }
  const t: f32 = progress(moveFrames, MOVE_MS);
  if (screen === current) {
    return f32(direction) * WIDTH * (f32(1) - t);
  }
  if (screen === leaving) {
    return -f32(direction) * WIDTH * t;
  }
  return 0;
}

export function visible(screen: i32): boolean {
  return screen === current || screen === leaving;
}

function gesture(dx: i32, dy: i32): void {
  // LV_DIR_LEFT / LV_DIR_RIGHT only: up and down gestures have no handler
  // on these screens.
  if (abs(dx) <= abs(dy)) {
    return;
  }
  const left = dx < 0;
  if (current === STARTUP) {
    if (left) {
      load(FUNCTION, 1);
    } else {
      load(INFO, -1);
    }
  } else if (current === INFO && left) {
    load(STARTUP, 1);
  } else if (current === FUNCTION && !left) {
    load(STARTUP, -1);
  }
}

/** One frame: the running move and the gesture recogniser. `blocked` is
 *  true while a function window is open (its backdrop stops gestures). */
export function step(x: i32, y: i32, down: boolean, blocked: boolean): void {
  if (leaving >= 0) {
    moveFrames = moveFrames + 1;
    if (progress(moveFrames, MOVE_MS) >= 1) {
      leaving = -1;
    }
  }
  if (down && !wasDown) {
    gestured = false;
    sumX = 0;
    sumY = 0;
    lastX = x;
    lastY = y;
    readFrames = 0;
  } else if (down) {
    readFrames = readFrames + 1;
  }
  if (down && wasDown && readFrames >= framesIn(READ_MS)) {
    readFrames = 0;
    const vx: i32 = x - lastX;
    const vy: i32 = y - lastY;
    lastX = x;
    lastY = y;
    if (abs(vx) < GESTURE_MIN_VELOCITY && abs(vy) < GESTURE_MIN_VELOCITY) {
      sumX = 0;
      sumY = 0;
    } else {
      sumX = sumX + vx;
      sumY = sumY + vy;
    }
    if (!gestured && (abs(sumX) > GESTURE_LIMIT || abs(sumY) > GESTURE_LIMIT)) {
      gestured = true;
      if (!blocked) {
        gesture(sumX, sumY);
      }
    }
  }
  wasDown = down;
}
