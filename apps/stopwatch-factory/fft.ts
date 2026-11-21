// Audio.FFT app (factory app_fft): the 20-band microphone spectrum drawn as
// 40 mirrored bars around a pulsing disc, with the peak frequency label.
// The board computes the bands (hal_audio.cpp); this module keeps the
// factory view smoothing, band reduction and bar motion.
import { clamp, cos, f64, idiv, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

const SHUFFLE: i32[] = [994, 285, 553, 11, 792, 707, 966, 641, 852, 827];
const WIDTHS: i32[] = [20, 8, 4, 2];

let bands: f64[] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
export let radii: f64[] = [82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0];
/** Bar radius for bar i (0..19), before mirroring. */
export let bars: f64[] = [82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0, 82.0];
export let discScale = 1.0;
export let labels: boolean = true;
let blend = 0.0;
let offset: i32 = 0;
let rotation: i32 = 0;
let cooldown: i32 = 0;
let hits: i32 = 0;

export function open(): void {
  labels = true;
  device.setMic(true);
}

export function close(): void {
  device.setMic(false);
}

export function tap(): void {
  labels = !labels;
}

export function update(): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    close();
    return true;
  }
  for (let i = 0; i < 20; i++) {
    const target = device.spectrum[i];
    const alpha = target > bands[i] ? 0.6 : 0.36;
    bands[i] = bands[i] + (target - bands[i]) * alpha;
  }
  const reduced: f64[] = [0.0, 0.0, 0.0, 0.0];
  for (let s = 0; s < 4; s++) {
    let sum = 0.0;
    let weights = 0.0;
    for (let k = 0; k < 5; k++) {
      const w = 1.0 - 0.35 * (f64(k) / 4.0);
      sum = sum + bands[s * 5 + k] * w;
      weights = weights + w;
    }
    reduced[s] = sum / weights;
  }
  const scale = 1.0 + clamp(0.08 * reduced[0] + 0.025 * reduced[1], 0.0, 0.1);
  discScale = discScale + (scale - discScale) * 0.4;
  for (let i = 0; i < 20; i++) {
    radii[i] = 82.0;
  }
  for (let s = 0; s < 4; s++) {
    const w = WIDTHS[s];
    const amplitude = round(74.0 * reduced[s]);
    for (let f = 0; f < w; f++) {
      const index = (((5 * s - idiv(w, 2) + f) % 20) + 20) % 20;
      const wave = round(180.0 * cos((f64(f * 360) / f64(w) + 180.0) * 0.017453292519943295)) + 180;
      radii[index] = radii[index] + f64((amplitude * wave) >> 9);
    }
  }
  blend = blend + 0.035;
  if (blend > 1.0) {
    blend = blend - 1.0;
    offset = (offset + 1) % 10;
  }
  if (cooldown > 0) {
    cooldown = cooldown - 1;
  }
  if (reduced[0] > 0.84 && cooldown === 0) {
    hits = hits + 1;
    cooldown = 14;
    if (hits % 3 === 0) {
      offset = (offset + 1) % 10;
    }
  }
  if (reduced[0] < 0.12) {
    rotation = (rotation + 1) % 20;
  }
  for (let i = 0; i < 20; i++) {
    const j = (i + rotation + SHUFFLE[offset % 10]) % 20;
    const k = (i + rotation + SHUFFLE[(offset + 1) % 10]) % 20;
    bars[i] = radii[k] * blend + radii[j] * (1.0 - blend);
  }
  return false;
}

/** Colour step 0..7 of a bar radius (#C19BFF → #E97AE0 → #FF66BC). */
export function level(radius: f64): i32 {
  if (radius < 80.0) {
    return 0;
  }
  if (radius < 100.0) {
    return 1 + round((radius - 80.0) / 20.0 * 2.0);
  }
  return clamp(4 + round((radius - 100.0) / 56.0 * 3.0), 4, 7);
}
