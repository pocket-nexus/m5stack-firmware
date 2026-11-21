// Metronome, 40 to 240 BPM in 4/4 with an accented first beat. Each click is
// requested up to 70 ms ahead with its start time (`tone hz ms atMs`), so the
// board starts it from an esp_timer at the beat time instead of at the next
// frame. The visuals switch beat when the clock passes that time; the
// pendulum reaches an end of its swing on every beat.
import { cos, f64, idiv, len, push, removeAt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

export const MIN_BPM: i32 = 40;
export const MAX_BPM: i32 = 240;
export const BEATS: i32 = 4;
const LOOKAHEAD_MS: f64 = 70.0;
const SWING_DEG: f64 = 28.0;
const PI: f64 = 3.141592653589793;

export let bpm: i32 = 100;
export let running: boolean = false;
/** Beat of the bar shown now, 0..BEATS-1 (-1 before the first beat). */
export let beat: i32 = -1;
/** 1 at a beat, falling to 0 over 200 ms. */
export let flash: f64 = 0.0;
/** Pendulum angle, degrees. */
export let swing: f64 = 0.0;

let nextBeatMs: f64 = 0.0;
let nextIndex: i32 = 0;
let lastBeatMs: f64 = 0.0;
let beatsShown: i32 = 0;
let pendingMs: f64[] = [];
let pendingIndex: i32[] = [];

function interval(): f64 {
  return 60000.0 / f64(bpm);
}

export function open(now: f64): void {
  running = false;
  beat = -1;
  flash = 0.0;
  swing = 0.0;
  pendingMs = [];
  pendingIndex = [];
}

export function close(): void {
  running = false;
}

function setBpm(value: i32): void {
  bpm = value;
  if (running) {
    // Keep the phase: the next beat follows the last one at the new tempo.
    nextBeatMs = lastBeatMs + interval();
  }
}

export function update(now: f64, dt: f64): void {
  if (input.frontClick) {
    running = !running;
    if (running) {
      nextBeatMs = now + 120.0;
      nextIndex = 0;
      beatsShown = 0;
      lastBeatMs = nextBeatMs - interval();
    } else {
      pendingMs = [];
      pendingIndex = [];
      beat = -1;
    }
  }
  if (input.sideClick) {
    setBpm(bpm + 5 > MAX_BPM ? MIN_BPM : bpm + 5);
    if (!running) {
      device.tone(1500, 15);
    }
  }
  if (input.sideRepeat) {
    setBpm(bpm - 5 < MIN_BPM ? MAX_BPM : bpm - 5);
    if (!running) {
      device.tone(1200, 15);
    }
  }

  if (running) {
    while (nextBeatMs < now + LOOKAHEAD_MS) {
      const accent = nextIndex === 0;
      device.toneAt(accent ? 1760 : 1175, accent ? 45 : 30, nextBeatMs);
      push(pendingMs, nextBeatMs);
      push(pendingIndex, nextIndex);
      nextBeatMs = nextBeatMs + interval();
      nextIndex = (nextIndex + 1) % BEATS;
    }
    while (len(pendingMs) > 0 && pendingMs[0] <= now) {
      lastBeatMs = pendingMs[0];
      beat = pendingIndex[0];
      beatsShown = beatsShown + 1;
      removeAt(pendingMs, 0);
      removeAt(pendingIndex, 0);
    }
    const sinceBeat = now - lastBeatMs;
    flash = beatsShown > 0 && sinceBeat < 200.0 ? 1.0 - sinceBeat / 200.0 : 0.0;
    // Ends of the swing on the beats, alternating sides.
    const side = beatsShown % 2 === 0 ? 1.0 : -1.0;
    swing = side * SWING_DEG * cos(PI * sinceBeat / interval());
  } else {
    flash = 0.0;
    swing = swing * (1.0 - dt * 6.0 > 0.0 ? 1.0 - dt * 6.0 : 0.0);
  }
}

export function tempoName(): string {
  if (bpm < 60) {
    return "Largo";
  }
  if (bpm < 66) {
    return "Larghetto";
  }
  if (bpm < 76) {
    return "Adagio";
  }
  if (bpm < 108) {
    return "Andante";
  }
  if (bpm < 120) {
    return "Moderato";
  }
  if (bpm < 168) {
    return "Allegro";
  }
  if (bpm < 200) {
    return "Presto";
  }
  return "Prestissimo";
}

/** Distance of the sliding weight from the pivot, px: as on a mechanical
 * metronome, the weight sits lower for a faster tempo. */
export function weightOffset(): i32 {
  return 100 - idiv((bpm - MIN_BPM) * 70, MAX_BPM - MIN_BPM);
}
