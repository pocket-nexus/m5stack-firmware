// FluidBox app (factory app_fluidbox), reduced: the factory simulates 450
// particles with a 3D double-density SPH in its own 80 Hz task. This model
// runs 48 particles of a 2D double-density relaxation (Clavet et al.) in the
// round 226 px container, with gravity and shake from the accelerometer, a
// radial push on touch (with a 45 ms vibration) and the factory speed
// colours. B resets the fluid; A+B goes home.
import { clamp, f32, idiv, push, round, sqrt, type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

// The simulation is f32: the ESP32-S3 has a single-precision FPU and
// computes f64 in software.
export const COUNT: i32 = 48;
const RADIUS: f32 = 226.0 - 11.0;
const H: f32 = 46.0;
const INV_H: f32 = f32(1.0 / 46.0);
const REST: f32 = 4.0;
const STIFF: f32 = 0.004;
const NEAR: f32 = 0.01;

export let px: f32[] = [];
export let py: f32[] = [];
let vx: f32[] = [];
let vy: f32[] = [];
let ox: f32[] = [];
let oy: f32[] = [];
let density: f32[] = [];
let nearDensity: f32[] = [];
let lastAx: f32 = 0.0;
let lastAy: f32 = 0.0;

export function reset(): void {
  px = [];
  py = [];
  vx = [];
  vy = [];
  ox = [];
  oy = [];
  density = [];
  nearDensity = [];
  // Rows from the bottom of the round box, 26 px apart.
  for (let i = 0; i < COUNT; i++) {
    const column = i % 8;
    const r = f32(idiv(i, 8));
    push(px, 233.0 + (f32(column) - 3.5) * 26.0 + f32(idiv(i, 8) % 2) * 6.0);
    push(py, 400.0 - r * 26.0);
    push(vx, 0.0);
    push(vy, 0.0);
    push(ox, 0.0);
    push(oy, 0.0);
    push(density, 0.0);
    push(nearDensity, 0.0);
  }
}

export function open(): void {
  reset();
}

function step(dt: f32): void {
  // Gravity points to the ground: with the factory axes an upright watch
  // reads accelY = -1 g. The change between frames adds shake.
  const ax: f32 = f32(-device.accelX);
  const ay: f32 = f32(-device.accelY);
  const gx = ax * 900.0 + (ax - lastAx) * 4000.0;
  const gy = ay * 900.0 + (ay - lastAy) * 4000.0;
  for (let i = 0; i < COUNT; i++) {
    vx[i] = vx[i] + gx * dt;
    vy[i] = vy[i] + gy * dt;
    ox[i] = px[i];
    oy[i] = py[i];
    px[i] = px[i] + vx[i] * dt;
    py[i] = py[i] + vy[i] * dt;
  }
  for (let i = 0; i < COUNT; i++) {
    density[i] = 0.0;
    nearDensity[i] = 0.0;
  }
  for (let i = 0; i < COUNT; i++) {
    for (let j = i + 1; j < COUNT; j++) {
      const dx = px[j] - px[i];
      const dy = py[j] - py[i];
      const d2 = dx * dx + dy * dy;
      if (d2 < H * H) {
        const q = 1.0 - sqrt(d2) * INV_H;
        density[i] = density[i] + q * q;
        density[j] = density[j] + q * q;
        nearDensity[i] = nearDensity[i] + q * q * q;
        nearDensity[j] = nearDensity[j] + q * q * q;
      }
    }
  }
  for (let i = 0; i < COUNT; i++) {
    for (let j = i + 1; j < COUNT; j++) {
      const dx = px[j] - px[i];
      const dy = py[j] - py[i];
      const d2 = dx * dx + dy * dy;
      if (d2 < H * H && d2 > 0.0001) {
        const d = sqrt(d2);
        const q = 1.0 - d * INV_H;
        const pressure = STIFF * (density[i] + density[j] - 2.0 * REST) + NEAR * (nearDensity[i] + nearDensity[j]);
        // One division per pair: the shove along the unit vector.
        const shove = clamp(pressure * q * 40.0, -3.5, 3.5) / d;
        px[i] = px[i] - dx * shove;
        py[i] = py[i] - dy * shove;
        px[j] = px[j] + dx * shove;
        py[j] = py[j] + dy * shove;
      }
    }
  }
  const settle: f32 = f32(0.985) / dt;
  for (let i = 0; i < COUNT; i++) {
    const dx = px[i] - 233.0;
    const dy = py[i] - 233.0;
    const d2 = dx * dx + dy * dy;
    if (d2 > RADIUS * RADIUS) {
      const scale = RADIUS / sqrt(d2);
      px[i] = 233.0 + dx * scale;
      py[i] = 233.0 + dy * scale;
    }
    vx[i] = (px[i] - ox[i]) * settle;
    vy[i] = (py[i] - oy[i]) * settle;
  }
}

function impulse(x: f32, y: f32): void {
  let moved = false;
  for (let i = 0; i < COUNT; i++) {
    const dx = px[i] - x;
    const dy = py[i] - y;
    const d = sqrt(dx * dx + dy * dy);
    if (d < 142.0 && d > 0.5) {
      const k = 1.0 - d / 142.0;
      const strength = 900.0 * k * k / d;
      vx[i] = vx[i] + dx * strength;
      vy[i] = vy[i] + dy * strength;
      moved = true;
    }
  }
  if (moved) {
    device.vibrate(45, 70);
  }
}

export function update(dt: f64): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    return true;
  }
  if (input.keyEvent === input.KEY_NEXT) {
    reset();
  }
  if (input.pointerPressed) {
    impulse(f32(input.pointerX), f32(input.pointerY));
  }
  // dt is 0 on the frame that follows a page open: no step, no division.
  const h: f32 = f32(clamp(dt, 0.0, 0.05) / 2.0);
  if (h > 0.0) {
    step(h);
    step(h);
  }
  lastAx = f32(-device.accelX);
  lastAy = f32(-device.accelY);
  return false;
}

/** Factory speed colour step 0..3 (deep blue → white). */
export function speedLevel(i: i32): i32 {
  const speed = sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
  return clamp(round(speed / 150.0), 0, 3);
}
