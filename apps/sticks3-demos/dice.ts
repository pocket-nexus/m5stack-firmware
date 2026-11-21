// Dice: one or two 3D cubes. Shaking the stick (an acceleration spike away
// from 1 g) or a front click throws them: each spins, bounces with a click
// tone per landing, and eases onto the face chosen at the throw. Face shades
// follow the rotated normals under a fixed light.
import { abs, cos, f32, f64, floor, max, push, round, sin, sqrt, type Color, type Deg, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as rng from "./rng";
import type { Die } from "./types";

const RAD: f64 = 0.017453292519943295;
/** Presentation tilt of the dice tray (the views' parent rotate-x/rotate-y). */
const TILT_X: f64 = 24.0;
const TILT_Y: f64 = 30.0;
const GRAVITY: f64 = 1500.0;
const SETTLE_MS: f64 = 260.0;
const SHAKE_G: f64 = 0.9;

// Greys from shadow to lit, for the faces.
const SHADES: Color[] = [
  "#5B6170",
  "#666C7A",
  "#727884",
  "#7E838E",
  "#8B8F99",
  "#989CA4",
  "#A5A8AF",
  "#B3B5BB",
  "#C1C3C7",
  "#CFD0D3",
  "#DDDEE0",
  "#ECECED",
  "#FAFAFA",
];

// Cube rotation (rx, ry) that turns face value 1..6 towards the viewer, in
// the core's convention (engine/core/src/draw.rs Mat34::rot_x/rot_y: the
// node applies rotateY, then rotateX; positive rotateX tips the top away).
const REST_X: f64[] = [0.0, 90.0, 0.0, 0.0, -90.0, 0.0];
const REST_Y: f64[] = [0.0, 0.0, -90.0, 90.0, 0.0, 180.0];
// Outward normals of faces 1..6 in the cube frame (y down, z to the viewer).
const NORMAL_X: f64[] = [0.0, 0.0, 1.0, -1.0, 0.0, 0.0];
const NORMAL_Y: f64[] = [0.0, -1.0, 0.0, 0.0, 1.0, 0.0];
const NORMAL_Z: f64[] = [1.0, 0.0, 0.0, 0.0, 0.0, -1.0];

const IDLE: i32 = 0;
const TUMBLING: i32 = 1;
const SETTLING: i32 = 2;

export let count: i32 = 2;
export let rolling: boolean = false;
export let rolls: i32 = 0;
export let total: i32 = 0;
export let values: i32[] = [5, 2];

let phase: i32 = 0;
let phaseMs: f64 = 0.0;
let lastThrowMs: f64 = -10000.0;
// Per die (two slots).
let rx: f64[] = [-90.0, -90.0];
let ry: f64[] = [0.0, 0.0];
let spinX: f64[] = [0.0, 0.0];
let spinY: f64[] = [0.0, 0.0];
let lift: f64[] = [0.0, 0.0];
let climb: f64[] = [0.0, 0.0];
let landed: boolean[] = [true, true];
let fromX: f64[] = [0.0, 0.0];
let fromY: f64[] = [0.0, 0.0];
let toX: f64[] = [0.0, 0.0];
let toY: f64[] = [0.0, 0.0];

export function open(now: f64): void {
  phase = IDLE;
  phaseMs = now;
  rolling = false;
  for (let i = 0; i < 2; i++) {
    rx[i] = REST_X[values[i] - 1];
    ry[i] = REST_Y[values[i] - 1];
    lift[i] = 0.0;
  }
}

function nearest(angle: f64, target: f64): f64 {
  const turns = floor((angle - target) / 360.0 + 0.5);
  return target + 360.0 * f64(turns);
}

export function roll(now: f64): void {
  phase = TUMBLING;
  phaseMs = now;
  rolling = true;
  lastThrowMs = now;
  rolls = rolls + 1;
  for (let i = 0; i < 2; i++) {
    values[i] = 1 + rng.int(6);
    spinX[i] = rng.range(520.0, 900.0) * (rng.next() < 0.5 ? -1.0 : 1.0);
    spinY[i] = rng.range(520.0, 900.0) * (rng.next() < 0.5 ? -1.0 : 1.0);
    climb[i] = rng.range(230.0, 300.0);
    landed[i] = false;
  }
  device.tone(520, 25);
}

function settle(now: f64): void {
  phase = SETTLING;
  phaseMs = now;
  for (let i = 0; i < 2; i++) {
    fromX[i] = rx[i];
    fromY[i] = ry[i];
    toX[i] = nearest(rx[i], REST_X[values[i] - 1]);
    toY[i] = nearest(ry[i], REST_Y[values[i] - 1]);
  }
}

export function update(now: f64, dt: f64): void {
  if (phase === IDLE) {
    if (input.sideClick) {
      count = count === 1 ? 2 : 1;
      device.tone(1800, 18);
    }
    const ax = device.accelX;
    const ay = device.accelY;
    const az = device.accelZ;
    const jolt = abs(sqrt(ax * ax + ay * ay + az * az) - 1.0);
    if (input.frontClick || (device.imuOk && jolt > SHAKE_G && now - lastThrowMs > 700.0)) {
      roll(now);
    }
    return;
  }
  if (phase === TUMBLING) {
    let flying = false;
    for (let i = 0; i < count; i++) {
      rx[i] = rx[i] + spinX[i] * dt;
      ry[i] = ry[i] + spinY[i] * dt;
      if (landed[i]) {
        continue;
      }
      climb[i] = climb[i] - GRAVITY * dt;
      lift[i] = lift[i] + climb[i] * dt;
      if (lift[i] <= 0.0 && climb[i] < 0.0) {
        lift[i] = 0.0;
        climb[i] = -climb[i] * 0.42;
        spinX[i] = spinX[i] * 0.6;
        spinY[i] = spinY[i] * 0.6;
        device.tone(950 + rng.int(500), 10);
        if (climb[i] < 70.0) {
          landed[i] = true;
          climb[i] = 0.0;
        }
      }
      if (!landed[i]) {
        flying = true;
      }
    }
    if (!flying) {
      settle(now);
    }
    return;
  }
  // SETTLING: ease onto the chosen faces.
  let t = (now - phaseMs) / SETTLE_MS;
  if (t > 1.0) {
    t = 1.0;
  }
  const e = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
  for (let i = 0; i < 2; i++) {
    rx[i] = fromX[i] + (toX[i] - fromX[i]) * e;
    ry[i] = fromY[i] + (toY[i] - fromY[i]) * e;
  }
  if (t >= 1.0) {
    phase = IDLE;
    rolling = false;
    total = count === 2 ? values[0] + values[1] : values[0];
    device.tone(1568, 40);
  }
}

/** Shade index of face `face` (0..5) of die `i` after both rotations. */
function shade(i: i32, face: i32): i32 {
  // Cube: rotate Y by ry, then X by rx; tray: rotate Y by TILT_Y, then X by
  // TILT_X. The core's rot_x is y' = y cos + z sin, z' = -y sin + z cos,
  // which is the formula below with the angle negated.
  const a = ry[i] * RAD;
  const b = -rx[i] * RAD;
  const c = TILT_Y * RAD;
  const d = (0.0 - TILT_X) * RAD;
  let x = NORMAL_X[face];
  let y = NORMAL_Y[face];
  let z = NORMAL_Z[face];
  let x1 = x * cos(a) + z * sin(a);
  let z1 = -x * sin(a) + z * cos(a);
  x = x1;
  z = z1;
  let y1 = y * cos(b) - z * sin(b);
  z1 = y * sin(b) + z * cos(b);
  y = y1;
  z = z1;
  x1 = x * cos(c) + z * sin(c);
  z1 = -x * sin(c) + z * cos(c);
  x = x1;
  z = z1;
  y1 = y * cos(d) - z * sin(d);
  z1 = y * sin(d) + z * cos(d);
  y = y1;
  z = z1;
  // Light from the upper left, in front.
  const lit = max(x * -0.36 + y * -0.56 + z * 0.75, 0.0);
  const level = round((0.35 + 0.65 * lit) * 12.0);
  return level > 12 ? 12 : level < 0 ? 0 : level;
}

function dieX(i: i32): i32 {
  if (count === 1) {
    return 67;
  }
  return i === 0 ? 38 : 97;
}

function dieY(i: i32): i32 {
  if (count === 1) {
    return 112;
  }
  return i === 0 ? 100 : 124;
}

export function dice(): Die[] {
  const result: Die[] = [];
  for (let i = 0; i < count; i++) {
    const shades: Color[] = [];
    for (let face = 0; face < 6; face++) {
      push(shades, SHADES[shade(i, face)]);
    }
    push(result, {
      id: i,
      x: dieX(i),
      y: dieY(i),
      lift: round(lift[i]),
      rx: f32(rx[i]) as Deg,
      ry: f32(ry[i]) as Deg,
      shadow: f32(max(1.0 - lift[i] / 90.0, 0.25)),
      shades: shades,
    });
  }
  return result;
}
