// LVGL-style rollers (lv_roller) for the alarm, settings and wheel pickers:
// a 164 px box with 46 px rows and the selected row in the middle. A
// vertical drag moves the rows with the finger, a release glides with the
// release velocity and snaps to a row; a tap on another row selects it.
// Infinite rollers wrap; normal rollers stop at both ends. Up to three
// rollers are live at once (slots 0..2).
import { clamp, f64, floor, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import { lvEaseOut } from "./motion";

export const ROW: i32 = 46;
export const HEIGHT: i32 = 164;

export let count: i32[] = [0, 0, 0];
export let infinite: boolean[] = [false, false, false];
export let left: i32[] = [0, 0, 0];
export let top: i32[] = [0, 0, 0];
export let width: i32[] = [0, 0, 0];
/** Continuous position in rows; the selected value is round(position). */
export let position: f64[] = [0.0, 0.0, 0.0];
let enabled: boolean[] = [false, false, false];
let active: i32 = -1;
let pressPosition: f64 = 0.0;
let animFrom: f64[] = [0.0, 0.0, 0.0];
let animTo: f64[] = [0.0, 0.0, 0.0];
let animStart: f64[] = [0.0, 0.0, 0.0];
let animating: boolean[] = [false, false, false];

export function setup(slot: i32, rows: i32, wrap: boolean, x: i32, y: i32, w: i32, selected: i32): void {
  count[slot] = rows;
  infinite[slot] = wrap;
  left[slot] = x;
  top[slot] = y;
  width[slot] = w;
  position[slot] = f64(selected);
  enabled[slot] = true;
  animating[slot] = false;
}

export function disable(slot: i32): void {
  enabled[slot] = false;
  if (active === slot) {
    active = -1;
  }
}

export function disableAll(): void {
  disable(0);
  disable(1);
  disable(2);
}

/** Changes the row count (day roller) and clamps the selection. */
export function resize(slot: i32, rows: i32): void {
  count[slot] = rows;
  if (value(slot) >= rows) {
    position[slot] = f64(rows - 1);
    animating[slot] = false;
  }
}

/** Selected row, 0..count-1. */
export function value(slot: i32): i32 {
  const n = count[slot];
  if (n <= 0) {
    return 0;
  }
  const v = round(position[slot]);
  return infinite[slot] ? ((v % n) + n) % n : clamp(v, 0, n - 1);
}

/** Row index shown `k` rows below the selected one, or -1 for none. */
export function rowAt(slot: i32, k: i32): i32 {
  const n = count[slot];
  const v = round(position[slot]) + k;
  if (infinite[slot]) {
    return ((v % n) + n) % n;
  }
  return v >= 0 && v < n ? v : -1;
}

/** Pixel shift of the rows from their rest position. */
export function shift(slot: i32): i32 {
  return round((f64(round(position[slot])) - position[slot]) * f64(ROW));
}

function snap(slot: i32, target: f64): void {
  let to = f64(round(target));
  if (!infinite[slot]) {
    to = clamp(to, 0.0, f64(count[slot] - 1));
  }
  animFrom[slot] = position[slot];
  animTo[slot] = to;
  animStart[slot] = device.uptimeMs;
  animating[slot] = true;
}

/** One frame; returns true while a roller owns the contact. */
export function update(): boolean {
  if (input.pointerPressed) {
    active = -1;
    for (let i = 0; i < 3; i++) {
      if (
        enabled[i] &&
        input.pointerX >= left[i] &&
        input.pointerX < left[i] + width[i] &&
        input.pointerY >= top[i] &&
        input.pointerY < top[i] + HEIGHT
      ) {
        active = i;
        animating[i] = false;
        pressPosition = position[i];
      }
    }
  }
  if (active >= 0 && input.pointerDown && input.moved) {
    let next = pressPosition - f64(input.pointerY - input.startY) / f64(ROW);
    if (!infinite[active]) {
      next = clamp(next, -0.4, f64(count[active] - 1) + 0.4);
    }
    position[active] = next;
  }
  if (active >= 0 && input.pointerReleased) {
    if (input.moved) {
      // Glide: about 300 ms of the release velocity, then align to a row.
      snap(active, position[active] - input.velocityY * 300.0 / f64(ROW));
    } else {
      const centre = top[active] + idivRow(HEIGHT);
      snap(active, position[active] + f64(round(f64(input.pointerY - centre) / f64(ROW))));
    }
    active = -1;
  }
  for (let i = 0; i < 3; i++) {
    if (animating[i]) {
      const t = (device.uptimeMs - animStart[i]) / 200.0;
      if (t >= 1.0) {
        position[i] = animTo[i];
        animating[i] = false;
        if (infinite[i]) {
          // Keep the position near the first lap.
          position[i] = f64(value(i));
        }
      } else {
        position[i] = animFrom[i] + (animTo[i] - animFrom[i]) * lvEaseOut(t);
      }
    }
  }
  return active >= 0;
}

function idivRow(height: i32): i32 {
  return floor(f64(height) / 2.0);
}

export function busy(): boolean {
  return active >= 0 || animating[0] || animating[1] || animating[2];
}
