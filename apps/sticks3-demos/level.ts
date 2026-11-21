// Spirit level. Lying flat (|z| above 0.55 g) it shows a bull's-eye bubble
// and a horizontal vial with the two tilt angles; standing on an edge it
// shows the roll angle against a rotating horizon. The front key stores the
// current attitude as zero, the side key clears it. Readings are low-pass
// filtered (time constant about 120 ms).
import { abs, atan2, clamp, exp, f64, idiv, round, sqrt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

const DEG: f64 = 57.29577951308232;
/** Bubble deflection per degree, px; the vial ring radius is 44 px. */
const BUBBLE_PX: f64 = 3.0;
const BUBBLE_MAX: f64 = 44.0;
const VIAL_MAX: f64 = 46.0;
/** Within this many degrees the level reads as level. */
const LEVEL_DEG: f64 = 0.5;

export let flat: boolean = true;
/** Tilt about the screen's vertical (left/right) and horizontal axes, degrees. */
export let angleX: f64 = 0.0;
export let angleY: f64 = 0.0;
/** Roll angle when standing on an edge, degrees (0: long side vertical). */
export let roll: f64 = 0.0;
export let isLevel: boolean = false;
export let zeroed: boolean = false;

let ax: f64 = 0.0;
let ay: f64 = 0.0;
let az: f64 = 1.0;
let zeroX: f64 = 0.0;
let zeroY: f64 = 0.0;
let zeroRoll: f64 = 0.0;
let wasLevel: boolean = false;

export function open(now: f64): void {
  ax = device.accelX;
  ay = device.accelY;
  az = device.accelZ;
  wasLevel = false;
}

function wrap180(a: f64): f64 {
  let v = a;
  while (v > 180.0) {
    v = v - 360.0;
  }
  while (v <= -180.0) {
    v = v + 360.0;
  }
  return v;
}

export function update(now: f64, dt: f64): void {
  const k = 1.0 - exp(-dt / 0.12);
  ax = ax + (device.accelX - ax) * k;
  ay = ay + (device.accelY - ay) * k;
  az = az + (device.accelZ - az) * k;
  flat = abs(az) > 0.55;
  // M5Unified frame (+X right edge, +Y top edge, +Z out of the screen).
  const rawX = atan2(ax, sqrt(ay * ay + az * az)) * DEG;
  const rawY = atan2(ay, sqrt(ax * ax + az * az)) * DEG;
  const rawRoll = atan2(ax, ay) * DEG;
  if (input.frontClick) {
    zeroX = rawX;
    zeroY = rawY;
    zeroRoll = rawRoll;
    zeroed = true;
    device.tone(1760, 30);
  }
  if (input.sideClick) {
    zeroX = 0.0;
    zeroY = 0.0;
    zeroRoll = 0.0;
    zeroed = false;
    device.tone(1175, 30);
  }
  angleX = rawX - zeroX;
  angleY = rawY - zeroY;
  roll = wrap180(rawRoll - zeroRoll);
  if (flat) {
    isLevel = abs(angleX) < LEVEL_DEG && abs(angleY) < LEVEL_DEG;
  } else {
    // Level when a long or a short side is vertical.
    const r = abs(roll);
    isLevel = r < LEVEL_DEG || abs(r - 90.0) < LEVEL_DEG || abs(r - 180.0) < LEVEL_DEG;
  }
  if (isLevel && !wasLevel) {
    device.tone(2637, 25);
  }
  wasLevel = isLevel;
}

/** Bubble offset from the ring centre: it floats to the higher side. */
export function bubbleX(): i32 {
  return round(clamp(angleX * BUBBLE_PX, -BUBBLE_MAX, BUBBLE_MAX));
}

export function bubbleY(): i32 {
  // Screen y grows downwards, board +Y points to the top edge.
  return round(clamp(-angleY * BUBBLE_PX, -BUBBLE_MAX, BUBBLE_MAX));
}

export function vialX(): i32 {
  return round(clamp(angleX * BUBBLE_PX, -VIAL_MAX, VIAL_MAX));
}

/** "+1.2°" */
export function degrees(a: f64): string {
  const tenths = round(abs(a) * 10.0);
  const sign = a < 0.0 && tenths > 0 ? "-" : "+";
  return `${sign}${idiv(tenths, 10)}.${tenths % 10}°`;
}
