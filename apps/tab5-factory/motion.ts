// Animated values in the style of the factory firmware's smooth_ui_toolkit
// AnimateValue: a spring given by its visual duration and bounce (the Motion
// parametrization), or a linear tween. Pure functions; the models own the
// state.
import { abs, f32, sqrt, type i32 } from "@pocketjs/framework/solid/std";

export interface Spring {
  /** Current value, velocity per second and target. */
  value: f32;
  velocity: f32;
  target: f32;
  stiffness: f32;
  damping: f32;
}

export interface Tween {
  value: f32;
  from: f32;
  target: f32;
  /** Elapsed and total time in seconds; total 0 jumps to the target. */
  elapsed: f32;
  duration: f32;
}

/** Motion's visualDuration/bounce spring: stiffness (2 pi / (1.2 d))^2,
 *  damping 2 (1 - bounce) sqrt(stiffness), unit mass. */
export function spring(value: f32, visualDuration: f32, bounce: f32): Spring {
  const root: f32 = f32(6.2831853) / (visualDuration * f32(1.2));
  const stiffness: f32 = root * root;
  return {
    value: value,
    velocity: f32(0),
    target: value,
    stiffness: stiffness,
    damping: f32(2) * (f32(1) - bounce) * sqrt(stiffness),
  };
}

/** The spring moved to a new target, keeping its velocity. */
export function retarget(s: Spring, target: f32): Spring {
  return { value: s.value, velocity: s.velocity, target: target, stiffness: s.stiffness, damping: s.damping };
}

/** The spring at rest on `value`. */
export function teleport(s: Spring, value: f32): Spring {
  return { value: value, velocity: f32(0), target: value, stiffness: s.stiffness, damping: s.damping };
}

export function settled(s: Spring): boolean {
  return s.value === s.target && s.velocity === f32(0);
}

/** One frame of `dt` seconds in eight semi-implicit Euler steps; the spring
 *  snaps to its target once it is within half a unit and slow. */
export function stepSpring(s: Spring, dt: f32): Spring {
  let value: f32 = s.value;
  let velocity: f32 = s.velocity;
  const h: f32 = dt / f32(8);
  for (let i: i32 = 0; i < 8; i++) {
    const force: f32 = s.stiffness * (s.target - value) - s.damping * velocity;
    velocity = velocity + force * h;
    value = value + velocity * h;
  }
  if (abs(s.target - value) < f32(0.5) && abs(velocity) < f32(10)) {
    value = s.target;
    velocity = f32(0);
  }
  return { value: value, velocity: velocity, target: s.target, stiffness: s.stiffness, damping: s.damping };
}

export function tween(value: f32, duration: f32): Tween {
  return { value: value, from: value, target: value, elapsed: duration, duration: duration };
}

export function tweenTo(t: Tween, target: f32, duration: f32): Tween {
  return { value: t.value, from: t.value, target: target, elapsed: f32(0), duration: duration };
}

export function tweenJump(t: Tween, value: f32): Tween {
  return { value: value, from: value, target: value, elapsed: t.duration, duration: t.duration };
}

export function tweenDone(t: Tween): boolean {
  return t.elapsed >= t.duration;
}

export function stepTween(t: Tween, dt: f32): Tween {
  const elapsed: f32 = t.elapsed + dt;
  if (elapsed >= t.duration) {
    return { value: t.target, from: t.from, target: t.target, elapsed: t.duration, duration: t.duration };
  }
  const value: f32 = t.from + (t.target - t.from) * (elapsed / t.duration);
  return { value: value, from: t.from, target: t.target, elapsed: elapsed, duration: t.duration };
}
