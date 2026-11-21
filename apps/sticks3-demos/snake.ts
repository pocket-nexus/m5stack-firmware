// Snake on a 15 x 24 grid of 9 px cells. The front key turns left, the side
// key turns right (relative to the heading); two turns can be queued per
// step. The step interval shrinks from 220 ms by 7 ms per food to 75 ms.
import { f64, floor, idiv, insert, len, max, pop, push, removeAt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as rng from "./rng";
import type { Spot } from "./types";

export const COLS: i32 = 15;
export const ROWS: i32 = 24;
export const CELL: i32 = 9;
export const TOP: i32 = 24;

export const READY: i32 = 0;
export const PLAYING: i32 = 1;
export const DYING: i32 = 2;
export const OVER: i32 = 3;

// Headings: 0 up, 1 right, 2 down, 3 left.
const DX: i32[] = [0, 1, 0, -1];
const DY: i32[] = [-1, 0, 1, 0];

export let state: i32 = 0;
/** Body cells, head first, as column + row * COLS. */
export let body: i32[] = [];
/** Spot id of each body cell, head first (keeps the view's rows keyed). */
let ids: i32[] = [];
export let food: i32 = 0;
export let newBest: boolean = false;
/** Blink phase while dying. */
export let visible: boolean = true;
/** Food pulse 0..1. */
export let pulse: f64 = 0.0;
/** Counts body changes, so the root publishes segments only when they move. */
export let version: i32 = 0;

let heading: i32 = 0;
let turns: i32[] = [];
let nextStepMs: f64 = 0.0;
let stateMs: f64 = 0.0;
let nextId: i32 = 0;

export function length(): i32 {
  return len(body);
}

export function bestLength(): i32 {
  return device.best[device.BEST_SNAKE];
}

function interval(): f64 {
  const ms = 220 - 7 * (len(body) - 4);
  return f64(max(ms, 75));
}

function occupied(cell: i32): boolean {
  for (const part of body) {
    if (part === cell) {
      return true;
    }
  }
  return false;
}

function placeFood(): void {
  for (let attempt = 0; attempt < 200; attempt++) {
    const cell = rng.int(COLS * ROWS);
    if (!occupied(cell)) {
      food = cell;
      return;
    }
  }
}

function reset(now: f64): void {
  body = [];
  ids = [];
  const c = idiv(COLS, 2);
  for (let i = 0; i < 4; i++) {
    push(body, c + (ROWS - 6 + i) * COLS);
    nextId = nextId + 1;
    push(ids, nextId);
  }
  heading = 0;
  turns = [];
  version = version + 1;
  placeFood();
  nextStepMs = now + 400.0;
  visible = true;
  newBest = false;
}

export function open(now: f64): void {
  state = READY;
  stateMs = now;
  reset(now);
}

function die(now: f64): void {
  state = DYING;
  stateMs = now;
  device.toneAt(392, 120, now);
  device.toneAt(262, 260, now + 140.0);
  const size = len(body);
  newBest = size > bestLength();
  if (newBest) {
    device.saveBest(device.BEST_SNAKE, size);
  }
}

function step(now: f64): void {
  if (len(turns) > 0) {
    heading = (heading + turns[0] + 4) % 4;
    removeAt(turns, 0);
  }
  const head = body[0];
  const c = head % COLS + DX[heading];
  const r = idiv(head, COLS) + DY[heading];
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS) {
    die(now);
    return;
  }
  const cell = c + r * COLS;
  const eats = cell === food;
  if (!eats) {
    // The tail moves away this step, so the head may enter its cell.
    pop(body);
    pop(ids);
  }
  if (occupied(cell)) {
    die(now);
    return;
  }
  insert(body, 0, cell);
  nextId = nextId + 1;
  insert(ids, 0, nextId);
  version = version + 1;
  if (eats) {
    device.tone(1320, 30);
    placeFood();
  }
}

function queueTurn(turn: i32): void {
  if (len(turns) < 2) {
    push(turns, turn);
  }
}

export function update(now: f64, dt: f64): void {
  pulse = pulse + dt * 2.0;
  if (pulse >= 1.0) {
    pulse = pulse - 1.0;
  }
  if (state === READY || state === OVER) {
    if (input.frontClick || input.sideClick) {
      if (state === OVER) {
        reset(now);
      }
      state = PLAYING;
      stateMs = now;
      nextStepMs = now + 250.0;
      device.tone(1047, 30);
    }
    return;
  }
  if (state === DYING) {
    visible = floor((now - stateMs) / 120.0) % 2 === 1;
    if (now - stateMs > 900.0) {
      state = OVER;
      stateMs = now;
      visible = true;
    }
    return;
  }
  if (input.frontPressed) {
    queueTurn(-1);
  }
  if (input.sidePressed) {
    queueTurn(1);
  }
  let guard = 0;
  while (now >= nextStepMs && state === PLAYING && guard < 3) {
    guard = guard + 1;
    step(now);
    nextStepMs = nextStepMs + interval();
  }
  if (now >= nextStepMs) {
    nextStepMs = now + interval();
  }
}

/** Segments for the view: x, y in pixels, kind 0 head, 1 body. */
export function segments(): Spot[] {
  const result: Spot[] = [];
  for (let i = 0; i < len(body); i++) {
    const cell = body[i];
    push(result, {
      id: ids[i],
      x: (cell % COLS) * CELL,
      y: TOP + idiv(cell, COLS) * CELL,
      kind: i === 0 ? 0 : i % 2 === 0 ? 1 : 2,
    });
  }
  return result;
}

export function foodX(): i32 {
  return (food % COLS) * CELL;
}

export function foodY(): i32 {
  return TOP + idiv(food, COLS) * CELL;
}
