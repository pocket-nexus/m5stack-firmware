// Springs and tweens stepped once per frame. The spring model follows the
// smooth_ui_toolkit AnimateValue options the factory uses (visual duration
// and bounce): stiffness = (2π / (1.2 · duration))², damping ratio
// 1 − bounce, unit mass.
import { clamp, sqrt, type f64, type i32 } from "@pocketjs/framework/solid/std";

export const SPRING_STATUS_BAR: i32 = 0;
export const SPRING_SECOND_DOT: i32 = 1;
export const SPRING_STOPWATCH_LEFT: i32 = 2;
export const SPRING_STOPWATCH_RIGHT: i32 = 3;
export const SPRING_COUNT: i32 = 4;

export let springPos: f64[] = [0.0, 0.0, 0.0, 0.0];
export let springVel: f64[] = [0.0, 0.0, 0.0, 0.0];
export let springTarget: f64[] = [0.0, 0.0, 0.0, 0.0];
export let springStiffness: f64[] = [100.0, 100.0, 100.0, 100.0];
export let springDamping: f64[] = [20.0, 20.0, 20.0, 20.0];

export function springConfigure(index: i32, duration: f64, bounce: f64): void {
  const root: f64 = 6.2831855 / (duration * 1.2);
  const stiffness: f64 = root * root;
  springStiffness[index] = stiffness;
  springDamping[index] = 2.0 * clamp(1.0 - bounce, 0.05, 1.0) * sqrt(stiffness);
}

export function springTeleport(index: i32, value: f64): void {
  springPos[index] = value;
  springTarget[index] = value;
  springVel[index] = 0.0;
}

export function springTo(index: i32, value: f64): void {
  springTarget[index] = value;
}

export function springDone(index: i32): boolean {
  const delta: f64 = springPos[index] - springTarget[index];
  return delta < 0.05 && delta > -0.05 && springVel[index] < 0.5 && springVel[index] > -0.5;
}

/** Advances every spring by `seconds` with semi-implicit Euler sub-steps. */
export function springStepAll(seconds: f64): void {
  const steps: i32 = 4;
  const dt: f64 = seconds / 4.0;
  for (let i = 0; i < SPRING_COUNT; i++) {
    if (springDone(i)) {
      springPos[i] = springTarget[i];
      springVel[i] = 0.0;
      continue;
    }
    for (let s = 0; s < steps; s++) {
      const force: f64 = -springStiffness[i] * (springPos[i] - springTarget[i]) - springDamping[i] * springVel[i];
      springVel[i] = springVel[i] + force * dt;
      springPos[i] = springPos[i] + springVel[i] * dt;
    }
  }
}

/** Cubic ease-out of t in 0..1. */
export function easeOut(t: f64): f64 {
  const u: f64 = 1.0 - clamp(t, 0.0, 1.0);
  return 1.0 - u * u * u;
}

/** LVGL's lv_anim_path_ease_out: a cubic Bézier (0, 0, 0.58, 1). */
export function lvEaseOut(t: f64): f64 {
  // Solve x(s) = t for the curve's parameter, then return y(s).
  let s: f64 = clamp(t, 0.0, 1.0);
  for (let n = 0; n < 6; n++) {
    const x: f64 = 3.0 * (1.0 - s) * s * s * 0.58 + s * s * s;
    const dx: f64 = 6.0 * (1.0 - s) * s * 0.58 - 3.0 * s * s * 0.58 + 3.0 * s * s;
    if (dx > -0.0001 && dx < 0.0001) {
      break;
    }
    s = clamp(s - (x - t) / dx, 0.0, 1.0);
  }
  return 3.0 * (1.0 - s) * s * s + s * s * s;
}
