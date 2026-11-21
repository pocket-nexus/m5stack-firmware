// LuckyWheel app (factory app_lucky_wheel): pick 2..18 options on a roller,
// then spin the pointer over the coloured sectors. A spins anticlockwise,
// B clockwise, a tap in a random direction; each spin lands inside a random
// sector after 4..7 turns with a cubic ease-out over 2.4..3.6 s.
import { abs, f32, f64, fill, idiv, min, push, round, type Deg, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as roller from "./roller";

export const SELECT: i32 = 0;
export const WHEEL: i32 = 1;

export interface Sector {
  id: i32;
  /** Start angle in screen degrees (0 = 3 o'clock, clockwise). */
  start: i32;
  /** The start angle for the half-plane rotation. */
  angle: Deg;
  color: i32;
  label: string;
  /** Label rotation about the screen centre. */
  labelAngle: Deg;
}

export let screen: i32 = 0;
export let options: i32 = 2;
/** Sectors painted in the right (x >= 233) and left half of the screen. */
export let rightHalf: Sector[] = [];
export let leftHalf: Sector[] = [];
export let labels: Sector[] = [];
/** Pointer angle in degrees, accumulating across spins. */
export let angle: f64 = 0.0;
let spinning: boolean = false;
let spinFrom: f64 = 0.0;
let spinTo: f64 = 0.0;
let spinStart: f64 = 0.0;
let spinDuration: f64 = 3000.0;
let seed: i32 = 4242;

function random(low: i32, high: i32): i32 {
  seed = (seed * 75 + 74) % 65537;
  return low + (seed % (high - low + 1));
}

export function open(): void {
  screen = SELECT;
  angle = 0.0;
  spinning = false;
  // Roller 374 px wide centred at (0, -13), options "2".."18".
  roller.setup(0, 17, false, 46, 138, 374, 0);
}

function circular(a: i32, b: i32): i32 {
  const d = abs(a - b);
  return min(d, 5 - d);
}

export function confirm(): void {
  options = roller.value(0) + 2;
  roller.disableAll();
  const n = options;
  // Factory colour order: least used colour, far from the previous one,
  // never equal to the neighbours.
  const usage: i32[] = [0, 0, 0, 0, 0];
  const colors: i32[] = fill(n, 0);
  for (let i = 0; i < n; i++) {
    let best = 0;
    let bestScore = -1000000;
    for (let c = 0; c < 5; c++) {
      if (i > 0 && c === colors[i - 1]) {
        continue;
      }
      if (i === n - 1 && c === colors[0]) {
        continue;
      }
      const score = -usage[c] * 100 + (i > 0 ? circular(c, colors[i - 1]) * 10 : 0);
      if (score > bestScore) {
        bestScore = score;
        best = c;
      }
    }
    colors[i] = best;
    usage[best] = usage[best] + 1;
  }
  rightHalf = [];
  leftHalf = [];
  labels = [];
  let straddle = -1;
  for (let i = 0; i < n; i++) {
    const start = 270 + idiv(360 * i, n);
    const end = 270 + idiv(360 * (i + 1), n);
    const centre = 270 + idiv(360 * (2 * i + 1), 2 * n);
    const sector: Sector = {
      id: i,
      start: start,
      angle: f32(start) as Deg,
      color: colors[i],
      label: `${i + 1}`,
      labelAngle: f32(centre - 90) as Deg,
    };
    push(labels, sector);
    if (start < 450) {
      push(rightHalf, sector);
      if (end > 450) {
        straddle = i;
      }
    }
  }
  if (straddle >= 0) {
    push(leftHalf, labels[straddle]);
  }
  for (let i = 0; i < n; i++) {
    if (labels[i].start >= 450) {
      push(leftHalf, labels[i]);
    }
  }
  screen = WHEEL;
}

function normalise(value: f64): f64 {
  let v = value;
  while (v >= 360.0) {
    v = v - 360.0;
  }
  while (v < 0.0) {
    v = v + 360.0;
  }
  return v;
}

/** direction 1: clockwise, -1: anticlockwise, 0: random. */
export function spin(direction: i32): void {
  if (spinning || screen !== WHEEL) {
    return;
  }
  const dir = direction === 0 ? (random(0, 1) === 0 ? -1 : 1) : direction;
  const n = options;
  const target = random(0, n - 1);
  const turns = random(4, 7);
  const width = 360.0 / f64(n);
  const centre = (360.0 * f64(2 * target + 1)) / f64(2 * n);
  const edge = min(8.0, 0.22 * width);
  const spread = width / 2.0 - edge > 0.0 ? width / 2.0 - edge : 0.0;
  const offset = (f64(random(0, 2000)) / 1000.0 - 1.0) * spread;
  const goal = normalise(centre + offset);
  const current = normalise(angle);
  spinFrom = angle;
  if (dir > 0) {
    spinTo = angle + f64(turns * 360) + normalise(goal - current);
  } else {
    spinTo = angle - f64(turns * 360) - normalise(current - goal);
  }
  spinStart = device.uptimeMs;
  spinDuration = f64(random(2400, 3600));
  spinning = true;
}

/** One frame; returns true when the app should close. */
export function update(): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    roller.disableAll();
    return true;
  }
  if (screen === SELECT) {
    roller.update();
    return false;
  }
  if (input.keyEvent === input.KEY_PREVIOUS) {
    spin(-1);
  } else if (input.keyEvent === input.KEY_NEXT) {
    spin(1);
  }
  if (spinning) {
    const t = (device.uptimeMs - spinStart) / spinDuration;
    if (t >= 1.0) {
      angle = spinTo;
      spinning = false;
    } else {
      const u = 1.0 - t;
      angle = spinFrom + (spinTo - spinFrom) * (1.0 - u * u * u);
    }
  }
  return false;
}

/** Displayed pointer rotation (0 = up, clockwise), 0.1° steps as in the factory. */
export function pointerAngle(): f64 {
  return f64(round(normalise(angle) * 10.0)) / 10.0;
}
