// FluidBox app (factory app_fluidbox), reduced: the factory simulates 450
// particles with a 3D double-density SPH in its own 80 Hz task. This model
// runs 48 particles of a 2D double-density relaxation (Clavet et al.) in the
// round 226 px container, with gravity and shake from the accelerometer, a
// radial push on touch (with a 45 ms vibration) and the factory speed
// colours. B resets the fluid; A+B goes home.
import { clamp, f64, idiv, push, round, sqrt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

export const COUNT: i32 = 48;
const RADIUS = 226.0 - 11.0;
const H = 46.0;
const REST = 4.0;
const STIFF = 0.004;
const NEAR = 0.01;

export let px: f64[] = [];
export let py: f64[] = [];
let vx: f64[] = [];
let vy: f64[] = [];
let ox: f64[] = [];
let oy: f64[] = [];
let density: f64[] = [];
let nearDensity: f64[] = [];
let lastAx = 0.0;
let lastAy = 0.0;

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
    const r = f64(idiv(i, 8));
    push(px, 233.0 + (f64(column) - 3.5) * 26.0 + f64(idiv(i, 8) % 2) * 6.0);
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

function step(dt: f64): void {
  // Gravity follows the accelerometer (screen x right, y down); the change
  // between frames adds shake.
  const ax = device.accelX;
  const ay = device.accelY;
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
        const q = 1.0 - sqrt(d2) / H;
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
        const q = 1.0 - d / H;
        const pressure = STIFF * (density[i] + density[j] - 2.0 * REST) + NEAR * (nearDensity[i] + nearDensity[j]);
        const shove = clamp(pressure * q * 40.0, -3.5, 3.5);
        const ux = dx / d;
        const uy = dy / d;
        px[i] = px[i] - ux * shove;
        py[i] = py[i] - uy * shove;
        px[j] = px[j] + ux * shove;
        py[j] = py[j] + uy * shove;
      }
    }
  }
  for (let i = 0; i < COUNT; i++) {
    const dx = px[i] - 233.0;
    const dy = py[i] - 233.0;
    const d = sqrt(dx * dx + dy * dy);
    if (d > RADIUS) {
      px[i] = 233.0 + dx / d * RADIUS;
      py[i] = 233.0 + dy / d * RADIUS;
    }
    vx[i] = (px[i] - ox[i]) / dt * 0.985;
    vy[i] = (py[i] - oy[i]) / dt * 0.985;
  }
}

function impulse(x: f64, y: f64): void {
  let moved = false;
  for (let i = 0; i < COUNT; i++) {
    const dx = px[i] - x;
    const dy = py[i] - y;
    const d = sqrt(dx * dx + dy * dy);
    if (d < 142.0 && d > 0.5) {
      const k = 1.0 - d / 142.0;
      const strength = 900.0 * k * k;
      vx[i] = vx[i] + dx / d * strength;
      vy[i] = vy[i] + dy / d * strength;
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
    impulse(f64(input.pointerX), f64(input.pointerY));
  }
  const h = clamp(dt, 0.0, 0.05) / 2.0;
  step(h);
  step(h);
  lastAx = device.accelX;
  lastAy = device.accelY;
  return false;
}

/** Factory speed colour step 0..3 (deep blue → white). */
export function speedLevel(i: i32): i32 {
  const speed = sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
  return clamp(round(speed / 150.0), 0, 3);
}
