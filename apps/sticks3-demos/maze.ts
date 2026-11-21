// Marble maze: a random perfect maze (depth-first search on a cell grid),
// a ball rolled by the tilt, a goal hole in the far corner and, from level 4,
// trap holes that send the ball back to the start. The timer starts when the
// ball first moves; the fastest time per level is stored.
import { abs, ceil, clamp, f64, fill, floor, idiv, len, max, min, pop, push, round, sqrt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as rng from "./rng";
import type { Box, Spot } from "./types";

const AREA_X: f64 = 2.0;
const AREA_Y: f64 = 26.0;
const AREA_W: f64 = 131.0;
const AREA_H: f64 = 212.0;
const WALL: f64 = 3.0;
const BALL_R: f64 = 5.0;
const GOAL_CATCH: f64 = 5.0;
const TRAP_CATCH: f64 = 4.0;
/** Screen acceleration per g of tilt, px/s². */
const PULL: f64 = 700.0;
const MAX_SPEED: f64 = 260.0;
const BOUNCE: f64 = 0.35;
const SINK_MS: f64 = 380.0;

const COLS: i32[] = [3, 4, 5, 5, 5];
const ROWS: i32[] = [5, 6, 7, 8, 8];

export const PLAYING: i32 = 0;
export const SINKING: i32 = 1;
export const CLEARED: i32 = 2;
export const TRAPPED: i32 = 3;

export let level: i32 = 1;
export let state: i32 = 0;
export let walls: Box[] = [];
export let traps: Spot[] = [];
export let goalX: f64 = 0.0;
export let goalY: f64 = 0.0;
export let ballX: f64 = 0.0;
export let ballY: f64 = 0.0;
export let ballScale: f64 = 1.0;
export let elapsedMs: f64 = 0.0;
export let newBest: boolean = false;
/** Counts generated mazes, so the root publishes walls only when they change. */
export let generation: i32 = 0;
/** Goal pulse phase, 0..1. */
export let pulse: f64 = 0.0;

let vx: f64 = 0.0;
let vy: f64 = 0.0;
let startX: f64 = 0.0;
let startY: f64 = 0.0;
let started: boolean = false;
let stateMs: f64 = 0.0;
let sinkFromX: f64 = 0.0;
let sinkFromY: f64 = 0.0;
let sinkToX: f64 = 0.0;
let sinkToY: f64 = 0.0;
let lastTickMs: f64 = 0.0;
let nextId: i32 = 0;

function levelIndex(): i32 {
  return min(level, 5) - 1;
}

export function bestMs(): i32 {
  return device.best[device.BEST_MAZE + levelIndex()];
}

function cellCentreX(c: i32, cols: i32): f64 {
  return AREA_X + (f64(c) + 0.5) * AREA_W / f64(cols);
}

function cellCentreY(r: i32, rows: i32): f64 {
  return AREA_Y + (f64(r) + 0.5) * AREA_H / f64(rows);
}

function addWall(x0: f64, y0: f64, x1: f64, y1: f64): void {
  const left = round(x0);
  const top = round(y0);
  nextId = nextId + 1;
  push(walls, { id: nextId, x: left, y: top, w: max(round(x1) - left, 1), h: max(round(y1) - top, 1), kind: 0 });
}

/** Builds the walls of a new maze for the current level. */
function generate(): void {
  const cols = COLS[levelIndex()];
  const rows = ROWS[levelIndex()];
  const n = cols * rows;
  generation = generation + 1;
  const visited: boolean[] = fill(n, false);
  const right: boolean[] = fill(n, true);
  const down: boolean[] = fill(n, true);
  const stack: i32[] = [0];
  visited[0] = true;
  while (len(stack) > 0) {
    const cell = stack[len(stack) - 1];
    const c = cell % cols;
    const r = idiv(cell, cols);
    const options: i32[] = [];
    if (c > 0 && !visited[cell - 1]) {
      push(options, 0);
    }
    if (c < cols - 1 && !visited[cell + 1]) {
      push(options, 1);
    }
    if (r > 0 && !visited[cell - cols]) {
      push(options, 2);
    }
    if (r < rows - 1 && !visited[cell + cols]) {
      push(options, 3);
    }
    if (len(options) === 0) {
      pop(stack);
      continue;
    }
    const dir = options[rng.int(len(options))];
    let next = cell;
    if (dir === 0) {
      next = cell - 1;
      right[cell - 1] = false;
    } else if (dir === 1) {
      next = cell + 1;
      right[cell] = false;
    } else if (dir === 2) {
      next = cell - cols;
      down[cell - cols] = false;
    } else {
      next = cell + cols;
      down[cell] = false;
    }
    visited[next] = true;
    push(stack, next);
  }

  const cw = AREA_W / f64(cols);
  const ch = AREA_H / f64(rows);
  const half = WALL / 2.0;
  walls = [];
  // Border.
  addWall(AREA_X - half, AREA_Y - half, AREA_X + AREA_W + half, AREA_Y + half);
  addWall(AREA_X - half, AREA_Y + AREA_H - half, AREA_X + AREA_W + half, AREA_Y + AREA_H + half);
  addWall(AREA_X - half, AREA_Y - half, AREA_X + half, AREA_Y + AREA_H + half);
  addWall(AREA_X + AREA_W - half, AREA_Y - half, AREA_X + AREA_W + half, AREA_Y + AREA_H + half);
  // Horizontal walls below row r-1, merged into runs.
  for (let r = 1; r < rows; r++) {
    const y = AREA_Y + f64(r) * ch;
    let c = 0;
    while (c < cols) {
      if (!down[(r - 1) * cols + c]) {
        c = c + 1;
        continue;
      }
      const from = c;
      while (c < cols && down[(r - 1) * cols + c]) {
        c = c + 1;
      }
      addWall(AREA_X + f64(from) * cw - half, y - half, AREA_X + f64(c) * cw + half, y + half);
    }
  }
  // Vertical walls right of column c-1, merged into runs.
  for (let c = 1; c < cols; c++) {
    const x = AREA_X + f64(c) * cw;
    let r = 0;
    while (r < rows) {
      if (!right[r * cols + c - 1]) {
        r = r + 1;
        continue;
      }
      const from = r;
      while (r < rows && right[r * cols + c - 1]) {
        r = r + 1;
      }
      addWall(x - half, AREA_Y + f64(from) * ch - half, x + half, AREA_Y + f64(r) * ch + half);
    }
  }

  // Start top left, goal bottom right, traps in other cells from level 4.
  startX = cellCentreX(0, cols);
  startY = cellCentreY(0, rows);
  goalX = cellCentreX(cols - 1, cols);
  goalY = cellCentreY(rows - 1, rows);
  traps = [];
  const trapCount = level >= 4 ? min(level - 2, 6) : 0;
  let attempts = 0;
  while (len(traps) < trapCount && attempts < 60) {
    attempts = attempts + 1;
    const cell = 2 + rng.int(n - 3);
    const c = cell % cols;
    const r = idiv(cell, cols);
    if (c + r < 2 || c + r > cols + rows - 4) {
      continue;
    }
    let taken = false;
    for (const other of traps) {
      if (other.kind === cell) {
        taken = true;
      }
    }
    if (taken) {
      continue;
    }
    // A trap sits off-centre in its cell so a path can pass beside it.
    const offX = rng.next() < 0.5 ? -0.22 : 0.22;
    const offY = rng.next() < 0.5 ? -0.22 : 0.22;
    nextId = nextId + 1;
    push(traps, {
      id: nextId,
      x: round(cellCentreX(c, cols) + offX * cw),
      y: round(cellCentreY(r, rows) + offY * ch),
      kind: cell,
    });
  }
}

function respawn(): void {
  ballX = startX;
  ballY = startY;
  vx = 0.0;
  vy = 0.0;
  ballScale = 1.0;
}

function startLevel(now: f64): void {
  generate();
  respawn();
  state = PLAYING;
  stateMs = now;
  elapsedMs = 0.0;
  started = false;
  newBest = false;
}

export function open(now: f64): void {
  level = 1;
  startLevel(now);
}

/** Pushes the ball out of one wall; returns the impact speed. */
function collide(w: Box): f64 {
  const left = f64(w.x);
  const top = f64(w.y);
  const right = left + f64(w.w);
  const bottom = top + f64(w.h);
  const cx = clamp(ballX, left, right);
  const cy = clamp(ballY, top, bottom);
  let dx = ballX - cx;
  let dy = ballY - cy;
  const d2 = dx * dx + dy * dy;
  if (d2 >= BALL_R * BALL_R) {
    return 0.0;
  }
  let dist = sqrt(d2);
  if (dist < 0.0001) {
    // Centre inside the wall: leave along the shallowest side.
    const toLeft = ballX - left;
    const toRight = right - ballX;
    const toTop = ballY - top;
    const toBottom = bottom - ballY;
    const least = min(min(toLeft, toRight), min(toTop, toBottom));
    dx = 0.0;
    dy = 0.0;
    if (least === toLeft) {
      dx = -1.0;
    } else if (least === toRight) {
      dx = 1.0;
    } else if (least === toTop) {
      dy = -1.0;
    } else {
      dy = 1.0;
    }
    dist = 1.0;
  }
  const nx = dx / dist;
  const ny = dy / dist;
  ballX = cx + nx * BALL_R;
  ballY = cy + ny * BALL_R;
  const vn = vx * nx + vy * ny;
  if (vn >= 0.0) {
    return 0.0;
  }
  vx = vx - (1.0 + BOUNCE) * vn * nx;
  vy = vy - (1.0 + BOUNCE) * vn * ny;
  return -vn;
}

function finish(now: f64): void {
  state = CLEARED;
  stateMs = now;
  const time = round(elapsedMs);
  const slot = device.BEST_MAZE + levelIndex();
  const previous = device.best[slot];
  newBest = previous <= 0 || time < previous;
  if (newBest) {
    device.saveBest(slot, time);
  }
  device.toneAt(1319, 70, now);
  device.toneAt(1760, 120, now + 90.0);
}

function sink(now: f64, toX: f64, toY: f64, next: i32): void {
  state = next;
  stateMs = now;
  sinkFromX = ballX;
  sinkFromY = ballY;
  sinkToX = toX;
  sinkToY = toY;
}

function physics(dt: f64, now: f64): void {
  // M5Unified frame: +X right edge, +Y top edge (assumed); the ball rolls
  // towards the lower edge.
  const ax = -device.accelX * PULL;
  const ay = device.accelY * PULL;
  vx = vx + ax * dt;
  vy = vy + ay * dt;
  const damping = max(1.0 - 1.4 * dt, 0.0);
  vx = vx * damping;
  vy = vy * damping;
  const speed = sqrt(vx * vx + vy * vy);
  if (speed > MAX_SPEED) {
    vx = vx * MAX_SPEED / speed;
    vy = vy * MAX_SPEED / speed;
  }
  const steps = clamp(ceil(speed * dt / 2.0), 1, 8);
  const h = dt / f64(steps);
  let impact = 0.0;
  for (let s = 0; s < steps; s++) {
    ballX = ballX + vx * h;
    ballY = ballY + vy * h;
    for (const w of walls) {
      impact = max(impact, collide(w));
    }
  }
  if (impact > 70.0 && now - lastTickMs > 70.0) {
    lastTickMs = now;
    device.tone(700 + round(impact * 2.0), 8);
  }
  if (!started && abs(ballX - startX) + abs(ballY - startY) > 2.0) {
    started = true;
  }
}

function lerp(a: f64, b: f64, t: f64): f64 {
  return a + (b - a) * t;
}

export function update(now: f64, dt: f64): void {
  pulse = pulse + dt * 1.2;
  if (pulse >= 1.0) {
    pulse = pulse - 1.0;
  }
  if (input.sideClick && state !== SINKING) {
    // New maze, same level.
    startLevel(now);
    device.tone(1800, 20);
    return;
  }
  if (state === CLEARED) {
    if (input.frontClick) {
      level = level + 1;
      startLevel(now);
      device.tone(2400, 25);
    }
    return;
  }
  if (state === SINKING || state === TRAPPED) {
    const progress = clamp((now - stateMs) / SINK_MS, 0.0, 1.0);
    ballX = lerp(sinkFromX, sinkToX, progress);
    ballY = lerp(sinkFromY, sinkToY, progress);
    ballScale = 1.0 - progress;
    if (state === SINKING) {
      elapsedMs = elapsedMs + dt * 1000.0;
    }
    if (progress >= 1.0) {
      if (state === SINKING) {
        finish(now);
      } else {
        respawn();
        state = PLAYING;
      }
    }
    return;
  }
  physics(dt, now);
  if (started) {
    elapsedMs = elapsedMs + dt * 1000.0;
  }
  const gx = ballX - goalX;
  const gy = ballY - goalY;
  if (gx * gx + gy * gy < GOAL_CATCH * GOAL_CATCH) {
    sink(now, goalX, goalY, SINKING);
    return;
  }
  for (const trap of traps) {
    const tx = ballX - f64(trap.x);
    const ty = ballY - f64(trap.y);
    if (tx * tx + ty * ty < TRAP_CATCH * TRAP_CATCH) {
      sink(now, f64(trap.x), f64(trap.y), TRAPPED);
      device.tone(220, 160);
      return;
    }
  }
}

/** "12.3" seconds. */
export function seconds(ms: f64): string {
  const tenths = floor(ms / 100.0);
  return `${idiv(tenths, 10)}.${tenths % 10}`;
}
