// Home carousel: one card per demo, side key to the next card, front key to
// open it. The strip position eases towards the selection; it only grows, so
// the last card wraps to the first by sliding on in the same direction.
import { exp, f64, floor, push, round, type i32 } from "@pocketjs/framework/solid/std";

export const DEMO_COUNT: i32 = 7;
export const CARD_WIDTH: i32 = 135;

/** Selected card, 0..DEMO_COUNT-1. */
export let selected: i32 = 0;
/** Target strip position (selection plus whole laps). */
let target: i32 = 0;
/** Displayed strip position. */
export let position: f64 = 0.0;
/** Seconds since the last key on the home screen. */
export let idle: f64 = 0.0;

export function next(): void {
  target = target + 1;
  selected = target % DEMO_COUNT;
  idle = 0.0;
}

export function wake(): void {
  idle = 0.0;
}

export function update(dt: f64): void {
  // Exponential approach, settled in about 200 ms.
  const k = 1.0 - exp(-dt * 18.0);
  position = position + (f64(target) - position) * k;
  if (f64(target) - position < 0.002) {
    position = f64(target);
  }
  idle = idle + dt;
}

/** Horizontal offset of card `index`, wrapped into [-3.5, 3.5) cards. */
export function offset(index: i32): i32 {
  let d: f64 = f64(index) - position;
  const laps = floor((d + 3.5) / 7.0);
  d = d - 7.0 * f64(laps);
  return round(d * f64(CARD_WIDTH));
}

/** Offsets of every card for the view. */
export function offsets(): i32[] {
  const result: i32[] = [];
  for (let i = 0; i < DEMO_COUNT; i++) {
    push(result, offset(i));
  }
  return result;
}
