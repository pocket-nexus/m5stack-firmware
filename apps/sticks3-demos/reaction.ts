// Reaction timer: after a random wait of 1.5 to 4.5 s the screen turns
// green; the time from the green frame reaching the panel to the front-key
// press is the result. Pressing during the wait is a foul. The press time is
// the key interrupt's timestamp, the green time the board's `presentedMs`
// of that frame, so the result does not depend on the 33 ms frame period.
import { f64, idiv, len, push, removeAt, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as rng from "./rng";

export const READY: i32 = 0;
export const WAITING: i32 = 1;
export const GO: i32 = 2;
export const RESULT: i32 = 3;
export const FOUL: i32 = 4;

/** Results under this are anticipation, not reaction. */
const TOO_FAST_MS: i32 = 90;

export let state: i32 = 0;
export let resultMs: i32 = 0;
export let newBest: boolean = false;
/** Last five results, oldest first. */
export let history: i32[] = [];
/** Pulse of the waiting dots, 0..3. */
export let dots: i32 = 0;

let goAtMs: f64 = 0.0;
let goFrameMs: f64 = 0.0;
let goPending: boolean = false;
let stateMs: f64 = 0.0;

export function open(now: f64): void {
  state = READY;
  // The re-arm guard below applies after a result, not on opening.
  stateMs = now - 1000.0;
}

export function bestMs(): i32 {
  return device.best[device.BEST_REACTION];
}

export function average(): i32 {
  if (len(history) === 0) {
    return 0;
  }
  let sum = 0;
  for (const v of history) {
    sum = sum + v;
  }
  return idiv(sum, len(history));
}

function arm(now: f64): void {
  state = WAITING;
  stateMs = now;
  goAtMs = now + rng.range(1500.0, 4500.0);
}

export function update(now: f64): void {
  dots = idiv(round(now - stateMs), 400) % 4;
  if (state === READY || state === RESULT || state === FOUL) {
    // A press right after a result is the tail of the reaction, not a new go.
    if (input.frontPressed && now - stateMs > 350.0) {
      arm(now);
      device.tone(880, 25);
    }
    return;
  }
  if (state === WAITING) {
    if (input.frontPressed) {
      state = FOUL;
      stateMs = now;
      device.tone(196, 220);
      return;
    }
    if (now >= goAtMs) {
      state = GO;
      stateMs = now;
      goFrameMs = now;
      goPending = true;
    }
    return;
  }
  // GO: the frame that turned green was presented before this frame.
  if (goPending) {
    goPending = false;
    if (device.presentedMs > goFrameMs) {
      goFrameMs = device.presentedMs;
    }
  }
  if (!input.frontPressed) {
    return;
  }
  const ms = round(input.frontPressAt - goFrameMs);
  if (ms < TOO_FAST_MS) {
    state = FOUL;
    stateMs = now;
    device.tone(196, 220);
    return;
  }
  resultMs = ms;
  state = RESULT;
  stateMs = now;
  push(history, ms);
  if (len(history) > 5) {
    removeAt(history, 0);
  }
  const best = bestMs();
  newBest = best <= 0 || ms < best;
  if (newBest) {
    device.saveBest(device.BEST_REACTION, ms);
    device.toneAt(1568, 60, now);
    device.toneAt(2093, 110, now + 80.0);
  } else {
    device.tone(1175, 60);
  }
}

export function rating(ms: i32): string {
  if (ms < 170) {
    return "Lightning!";
  }
  if (ms < 220) {
    return "Excellent";
  }
  if (ms < 270) {
    return "Great";
  }
  if (ms < 330) {
    return "Good";
  }
  if (ms < 420) {
    return "Not bad";
  }
  return "Sleepy?";
}
