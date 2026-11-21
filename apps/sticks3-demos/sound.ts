// Sound meter on the microphone level the board publishes each frame (RMS
// and peak of the samples since the previous frame, dBFS). The level is
// averaged in the power domain with a fast (125 ms) or slow (1 s) time
// constant, as a sound level meter's F/S settings; the peak marker holds for
// 1.5 s and then falls at 20 dB/s. The side key switches F/S, the front key
// clears the maximum. A history bar is added every 100 ms.
import { clamp, exp, f64, fill, floor, len, log, max, pow, push, removeAt, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import type { Box } from "./types";

/** Meter range: 20 segments of 3 dB. Uncalibrated: dBFS, not dB SPL. */
export const FLOOR_DB: f64 = -84.0;
export const TOP_DB: f64 = -24.0;
export const SEGMENTS: i32 = 20;
export const HISTORY: i32 = 44;
const HISTORY_H: f64 = 72.0;
const HOLD_MS: f64 = 1500.0;
const LN10: f64 = 2.302585092994046;

export let slow: boolean = false;
export let levelDb: f64 = -96.0;
export let peakDb: f64 = -96.0;
export let maxDb: f64 = -96.0;
let power: f64 = 0.0;
let holdUntil: f64 = 0.0;
let nextSampleMs: f64 = 0.0;
let history: f64[] = [];

export function open(now: f64): void {
  power = 0.0;
  levelDb = -96.0;
  peakDb = -96.0;
  maxDb = -96.0;
  history = fill(HISTORY, -96.0);
  nextSampleMs = now;
  device.setMic(true);
}

export function close(): void {
  device.setMic(false);
}

function toDb(p: f64): f64 {
  if (p <= 0.0000000001) {
    return -96.0;
  }
  return max(10.0 * log(p) / LN10, -96.0);
}

export function update(now: f64, dt: f64): void {
  if (input.sideClick) {
    slow = !slow;
    device.tone(1500, 20);
  }
  if (input.frontClick) {
    maxDb = levelDb;
    device.tone(1900, 20);
  }
  if (!device.micOn) {
    return;
  }
  const p = pow(10.0, device.micDb / 10.0);
  const tau = slow ? 1.0 : 0.125;
  power = power + (p - power) * (1.0 - exp(-dt / tau));
  levelDb = toDb(power);
  if (levelDb > maxDb) {
    maxDb = levelDb;
  }
  const peak = device.micPeakDb;
  if (peak >= peakDb) {
    peakDb = peak;
    holdUntil = now + HOLD_MS;
  } else if (now > holdUntil) {
    peakDb = max(peakDb - 20.0 * dt, -96.0);
  }
  if (now >= nextSampleMs) {
    nextSampleMs = nextSampleMs + 100.0;
    if (nextSampleMs < now) {
      nextSampleMs = now + 100.0;
    }
    push(history, levelDb);
    while (len(history) > HISTORY) {
      removeAt(history, 0);
    }
  }
}

function fraction(db: f64): f64 {
  return clamp((db - FLOOR_DB) / (TOP_DB - FLOOR_DB), 0.0, 1.0);
}

/** Lit segments of the bar meter, 0..SEGMENTS. */
export function lit(): i32 {
  return round(fraction(levelDb) * f64(SEGMENTS));
}

/** Segment index of the held peak, -1 below the range. */
export function peakSegment(): i32 {
  if (peakDb <= FLOOR_DB) {
    return -1;
  }
  return min1(floor(fraction(peakDb) * f64(SEGMENTS)), SEGMENTS - 1);
}

function min1(a: i32, b: i32): i32 {
  return a < b ? a : b;
}

/** Zone of a level: 0 green, 1 yellow (top third), 2 red (top 3 segments). */
function zone(f: f64): i32 {
  if (f >= 0.85) {
    return 2;
  }
  if (f >= 0.6) {
    return 1;
  }
  return 0;
}

/** Meter segments: kind 0..2 lit zones, 3..5 unlit zones, 6 peak. */
export function segments(): Box[] {
  const result: Box[] = [];
  const count = lit();
  const peak = peakSegment();
  for (let i = 0; i < SEGMENTS; i++) {
    const z = zone((f64(i) + 0.5) / f64(SEGMENTS));
    const kind = i === peak && i >= count ? 6 : i < count ? z : z + 3;
    push(result, { id: i, x: 4 + i * 6, y: 0, w: 5, h: 22, kind: kind });
  }
  return result;
}

/** History bars, newest on the right, anchored at the bottom. */
export function bars(): Box[] {
  const result: Box[] = [];
  for (let i = 0; i < len(history); i++) {
    const f = fraction(history[i]);
    const h = max(round(f * HISTORY_H), 1);
    push(result, { id: i, x: 2 + i * 3, y: round(HISTORY_H) - h, w: 2, h: h, kind: zone(f) });
  }
  return result;
}

/** "-42" */
export function db(value: f64): string {
  return `${round(value)}`;
}
