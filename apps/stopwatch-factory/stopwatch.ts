// Stopwatch app (factory app_stopwatch): start, pause, resume, reset and
// cumulative laps, newest first. Elapsed time comes from the monotonic
// uptime, so rendering delays do not change it.
import { codePoints, fromCodePoint, idiv, push, trunc, type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const STOPPED: i32 = 0;
export const RUNNING: i32 = 1;
export const PAUSED: i32 = 2;

export let state: i32 = 0;
let startedAt: f64 = 0.0;
let accumulated: f64 = 0.0;
/** Total elapsed milliseconds at each lap, in recording order. */
export let laps: i32[] = [];

export function elapsedMs(): i32 {
  if (state === RUNNING) {
    return trunc(accumulated + device.uptimeMs - startedAt);
  }
  return trunc(accumulated);
}

/** Left button: lap while running, reset while paused. */
export function left(): void {
  if (state === RUNNING) {
    push(laps, elapsedMs());
  } else if (state === PAUSED) {
    reset();
  }
}

/** Right button: start or resume while stopped or paused, pause while running. */
export function right(): void {
  if (state === RUNNING) {
    accumulated = accumulated + device.uptimeMs - startedAt;
    state = PAUSED;
  } else {
    startedAt = device.uptimeMs;
    state = RUNNING;
  }
}

export function reset(): void {
  state = STOPPED;
  accumulated = 0.0;
  laps = [];
}

function two(value: i32): string {
  return value < 10 ? `0${value}` : `${value}`;
}

/** "HH:MM:SS.CC" with every 0 drawn as the letter O, as in the factory. */
export function format(ms: i32): string {
  const hours = idiv(ms, 3600000);
  const minutes = idiv(ms, 60000) % 60;
  const seconds = idiv(ms, 1000) % 60;
  const hundredths = idiv(ms % 1000, 10);
  return letterO(`${two(hours)}:${two(minutes)}:${two(seconds)}.${two(hundredths)}`);
}

/** Replaces the digit 0 with the letter O (factory Maple Mono styling). */
export function letterO(text: string): string {
  let out = "";
  for (const code of codePoints(text)) {
    out = out + (code === 48 ? "O" : fromCodePoint(code));
  }
  return out;
}
