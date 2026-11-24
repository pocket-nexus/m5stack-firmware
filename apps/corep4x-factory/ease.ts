// LVGL 9 animation paths (lv_anim.c): linear, and the cubic Béziers of
// lv_anim_path_ease_in (0.42, 0, 1, 1) and lv_anim_path_ease_out
// (0, 0, 0.58, 1), which lv_cubic_bezier evaluates for x = elapsed/duration.
import { abs, clamp, f32, type i32 } from "@pocketjs/framework/solid/std";

/** Milliseconds per frame at the host's 30 Hz cadence. */
export const FRAME_MS: f32 = 33.333332;

function bezier(t: f32, p1: f32, p2: f32): f32 {
  const u: f32 = f32(1) - t;
  return f32(3) * u * u * t * p1 + f32(3) * u * t * t * p2 + t * t * t;
}

/** y of the cubic Bézier (0,0), (x1,y1), (x2,y2), (1,1) at x. */
function cubic(x: f32, x1: f32, y1: f32, x2: f32, y2: f32): f32 {
  let low: f32 = 0;
  let high: f32 = 1;
  let t: f32 = x;
  for (let i: i32 = 0; i < 20; i++) {
    const at: f32 = bezier(t, x1, x2);
    if (abs(at - x) < f32(0.0005)) {
      break;
    }
    if (at < x) {
      low = t;
    } else {
      high = t;
    }
    t = (low + high) * f32(0.5);
  }
  return bezier(t, y1, y2);
}

export function easeOut(x: f32): f32 {
  return cubic(clamp(x, 0, 1), 0, 0, f32(0.58), 1);
}

export function easeIn(x: f32): f32 {
  return cubic(clamp(x, 0, 1), f32(0.42), 0, 1, 1);
}

/** Progress 0..1 of an animation that started `frames` frames ago. */
export function progress(frames: i32, durationMs: f32): f32 {
  return clamp((f32(frames) * FRAME_MS) / durationMs, 0, 1);
}

export function lerp(from: f32, to: f32, t: f32): f32 {
  return from + (to - from) * t;
}
