// Park–Miller minimal standard generator (multiplier 16807, modulus 2^31 - 1)
// on f64: every product stays below 2^53, so JS and Rust give the same
// sequence. Seeded from the board's esp_random value at the first frame.
import { f64, floor, type i32 } from "@pocketjs/framework/solid/std";

const MODULUS: f64 = 2147483647.0;

let state: f64 = 20261121.0;

export function seed(value: f64): void {
  let s: f64 = value < 0.0 ? -value : value;
  s = s - 2147483646.0 * f64(floor(s / 2147483646.0));
  state = f64(floor(s)) + 1.0;
}

/** Mixes an external event time into the state (key presses). */
export function stir(value: f64): void {
  const v: f64 = value < 0.0 ? -value : value;
  const mixed: f64 = state + f64(floor(v * 7.0) % 1000003);
  state = mixed - 2147483646.0 * f64(floor(mixed / 2147483646.0)) + 1.0;
}

/** Uniform in [0, 1). */
export function next(): f64 {
  const product: f64 = state * 16807.0;
  state = product - MODULUS * f64(floor(product / MODULUS));
  if (state < 1.0) {
    state = 1.0;
  }
  return (state - 1.0) / (MODULUS - 1.0);
}

/** Uniform in [lo, hi). */
export function range(lo: f64, hi: f64): f64 {
  return lo + (hi - lo) * next();
}

/** Uniform integer in [0, n). */
export function int(n: i32): i32 {
  const v = floor(next() * f64(n));
  return v >= n ? n - 1 : v;
}
