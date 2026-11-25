// MUSIC window (ui_function_music_window.cpp): one embedded track,
// "CandyWind" (mono 44.1 kHz 16-bit, 45 s), played through the ES8311 at
// 16 kHz. PLAY starts it from 0:00, PAUSE stops it. 24 spectrum bars follow
// a 128-point FFT of the track at the playing position (the board computes
// the target heights 18..122); the bars rise with 0.55 and fall with 0.22 of
// the difference per 50 ms. The volume slider (0..100) sets the codec's
// output volume. Closing the window does not stop the music.
import { copy, f32, i32, imod, len, push, round, type Color } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import { TICK_HZ } from "./ease";

/** device.musicState */
export const READY: i32 = 0;
export const PREPARING: i32 = 1;
export const PLAYING: i32 = 2;
export const FINISHED: i32 = 3;
export const PAUSED: i32 = 4;
export const OTHER: i32 = 5;
export const NOT_READY: i32 = 6;

/** One spectrum bar: screen x and colour. Its height is barHeights()[id]. */
export interface Bar {
  id: i32;
  left: i32;
  sky: boolean;
}

export let heights: f32[] = [];
/** The whole-pixel heights the bars show. */
let shown: i32[] = [];
/** Counts the changes of `shown`: the view takes new bars only then. */
export let revision: i32 = 0;
let fresh: boolean = true;
/** Elapsed time towards the next bar update, in 1/TICK_HZ ms. */
let tickTime: i32 = 0;

// Volume slider: 176x18 at (251,412) on the screen.
const SLIDER_LEFT: i32 = 251;
const SLIDER_WIDTH: i32 = 176;
const SLIDER_TOP: i32 = 412;
let dragging: boolean = false;
let wasDown: boolean = false;

export function open(): void {
  heights = [];
  shown = [];
  for (let i: i32 = 0; i < 24; i++) {
    push(heights, 18);
    push(shown, 18);
  }
  revision = revision + 1;
  fresh = true;
  tickTime = 0;
  device.request("music 1");
}

export function close(): void {
  device.request("music 0");
}

export function press(): void {
  const state = device.musicState;
  if (state === PLAYING) {
    device.request("musicStop");
  } else if (state !== PREPARING && state !== NOT_READY) {
    device.request("musicPlay");
  }
}

/** One frame: the bars every 50 ms, the slider. */
export function step(x: i32, y: i32, down: boolean): void {
  tickTime = tickTime + 1000;
  if (tickTime >= 50 * TICK_HZ) {
    tickTime = tickTime - 50 * TICK_HZ;
    let changed: boolean = false;
    for (let i: i32 = 0; i < 24; i++) {
      const playing = device.musicState === PLAYING;
      const target: f32 = playing && i < len(device.musicBars) ? f32(device.musicBars[i]) : f32(18);
      const h = heights[i];
      let next: f32 = fresh ? target : h + (target - h) * (target > h ? f32(0.55) : f32(0.22));
      next = next < 18 ? 18 : next > 122 ? 122 : next;
      heights[i] = next;
      const whole = i32(round(next));
      if (whole !== shown[i]) {
        shown[i] = whole;
        changed = true;
      }
    }
    fresh = false;
    if (changed) {
      revision = revision + 1;
    }
  }
  if (down && !wasDown) {
    dragging = x >= SLIDER_LEFT - 14 && x <= SLIDER_LEFT + SLIDER_WIDTH + 14 && y >= SLIDER_TOP - 10 && y <= SLIDER_TOP + 28;
  }
  if (!down) {
    dragging = false;
  }
  wasDown = down;
  if (dragging) {
    let value = i32(round((f32(x - SLIDER_LEFT) * f32(100)) / f32(SLIDER_WIDTH)));
    value = value < 0 ? 0 : value > 100 ? 100 : value;
    if (value !== device.volume) {
      device.setVolume(value);
    }
  }
}

/** Bars at x = 64 + 15 i, 8 px wide, growing up from y 273; every second
 *  pair is sky blue (i % 4 in {1, 2}). The bars do not change; their
 *  heights are a separate list, so a new height updates one binding per
 *  bar instead of every bar's row. */
export function bars(): Bar[] {
  const out: Bar[] = [];
  for (let i: i32 = 0; i < 24; i++) {
    const phase = imod(i, 4);
    push(out, { id: i, left: 64 + 15 * i, sky: phase === 1 || phase === 2 });
  }
  return out;
}

/** The bar heights in whole pixels. */
export function barHeights(): i32[] {
  return copy(shown);
}

export function knob(): f32 {
  return f32(SLIDER_LEFT) + (f32(device.volume) * f32(SLIDER_WIDTH)) / f32(100);
}

function clock(ms: i32): string {
  const seconds = i32(f32(ms) / f32(1000));
  const minutes = i32(f32(seconds) / f32(60));
  const rest = seconds - minutes * 60;
  return `${minutes}:${rest < 10 ? "0" : ""}${rest}`;
}

export function elapsedText(): string {
  return clock(device.musicElapsedMs);
}

export function durationText(): string {
  return clock(device.musicDurationMs);
}

export function progress(): f32 {
  const duration = device.musicDurationMs;
  return duration > 0 ? f32(device.musicElapsedMs) / f32(duration) : f32(0);
}

export function statusText(): string {
  const state = device.musicState;
  return state === PREPARING
    ? "Preparing audio"
    : state === PLAYING
      ? "Playing"
      : state === FINISHED
        ? "Finished"
        : state === PAUSED
          ? "Paused"
          : state === OTHER
            ? "Other audio is playing"
            : state === NOT_READY
              ? "Audio playback not ready"
              : "Ready";
}

export function statusColor(): Color {
  const state = device.musicState;
  return state === PREPARING || state === PAUSED || state === OTHER ? "#0EA5E9" : state === PLAYING ? "#16A34A" : state === FINISHED ? "#2563EB" : state === NOT_READY ? "#DC2626" : "#64748B";
}
