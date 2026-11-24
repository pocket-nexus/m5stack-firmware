// The function window frame of the factory firmware
// (ui_function_window.cpp): a 170x48 header at (300,4) holding the action
// button and Exit, a 460x418 content panel at (10,52), and a 2 px dashed
// outline (dashes 7 px, gaps 5 px, 0xCBD5E1) drawn as one rectangle per dash
// around both. A paged window has a 460x370 content panel, tabs 110x50 along
// y 421 and dashed tab separators. All values are screen pixels.
import { abs, clear, copy, min, push, type i32 } from "@pocketjs/framework/solid/std";

export interface Dash {
  id: i32;
  x: i32;
  y: i32;
  w: i32;
  h: i32;
}

const BORDER: i32 = 2;
const DASH: i32 = 7;
const GAP: i32 = 5;

export const HEADER_LEFT: i32 = 300;
export const HEADER_TOP: i32 = 4;
export const HEADER_RIGHT: i32 = 470;
export const CONTENT_LEFT: i32 = 10;
export const CONTENT_TOP: i32 = 52;
export const CONTENT_RIGHT: i32 = 470;
export const CONTENT_BOTTOM: i32 = 470;
export const TAB_TOP: i32 = 421;
export const TAB_BORDER_Y: i32 = 422;
export const TAB_WIDTH: i32 = 110;
/** Exit button centre on the screen: header (300,4) + (88,5) + 38x19. */
export const EXIT_CENTER_X: i32 = 426;
export const EXIT_CENTER_Y: i32 = 28;

// Model arrays are values: the edges append to this module array and each
// builder returns a copy of it.
let acc: Dash[] = [];
let nextId: i32 = 0;
let remaining: i32 = DASH;
let drawing: boolean = true;

function step(): void {
  drawing = !drawing;
  remaining = drawing ? DASH : GAP;
}

/** add_dashed_axis_edge: one straight edge, continuing the dash pattern. */
function edge(x1: i32, y1: i32, x2: i32, y2: i32, ox: i32, oy: i32): void {
  const horizontal = y1 === y2;
  const length: i32 = horizontal ? abs(x2 - x1) : abs(y2 - y1);
  const dir: i32 = (horizontal ? x2 - x1 : y2 - y1) > 0 ? 1 : -1;
  let cx: i32 = x1;
  let cy: i32 = y1;
  let left: i32 = length;
  while (left > 0) {
    const run: i32 = remaining < left ? remaining : left;
    const nx: i32 = horizontal ? cx + dir * run : cx;
    const ny: i32 = horizontal ? cy : cy + dir * run;
    if (drawing) {
      if (horizontal) {
        push(acc, { id: nextId, x: min(cx, nx) + ox, y: y1 + oy, w: run, h: BORDER });
      } else {
        push(acc, { id: nextId, x: x1 + ox, y: min(cy, ny) + oy, w: BORDER, h: run });
      }
      nextId = nextId + 1;
    }
    cx = nx;
    cy = ny;
    left = left - run;
    remaining = remaining - run;
    if (remaining === 0) {
      step();
    }
  }
}

function reset(): void {
  remaining = DASH;
  drawing = true;
}

/** add_function_window_outline (single window) or the static outline of a
 *  paged window with `pages` tabs: the same six edges, plus the vertical
 *  separators between tabs. */
export function outline(pages: i32): Dash[] {
  clear(acc);
  nextId = 0;
  reset();
  edge(HEADER_LEFT, HEADER_TOP, HEADER_RIGHT, HEADER_TOP, 0, 0);
  edge(HEADER_RIGHT, HEADER_TOP, CONTENT_RIGHT, CONTENT_BOTTOM, -BORDER, 0);
  edge(CONTENT_RIGHT, CONTENT_BOTTOM, CONTENT_LEFT, CONTENT_BOTTOM, 0, -BORDER);
  edge(CONTENT_LEFT, CONTENT_BOTTOM, CONTENT_LEFT, CONTENT_TOP, 0, 0);
  edge(CONTENT_LEFT, CONTENT_TOP, HEADER_LEFT, CONTENT_TOP, 0, 0);
  edge(HEADER_LEFT, CONTENT_TOP, HEADER_LEFT, HEADER_TOP, 0, 0);
  for (let i: i32 = 1; i < pages; i++) {
    reset();
    edge(tabX(i), TAB_BORDER_Y, tabX(i), CONTENT_BOTTOM, 0, 0);
  }
  return copy(acc);
}

export function tabX(index: i32): i32 {
  return CONTENT_LEFT + index * TAB_WIDTH;
}

/** The dashed top edges of the inactive tabs (update_paged_window_view). */
export function tabEdges(pages: i32, active: i32): Dash[] {
  clear(acc);
  nextId = 1000;
  for (let i: i32 = 0; i < pages; i++) {
    if (i !== active) {
      reset();
      const right: i32 = i + 1 >= pages ? CONTENT_RIGHT : tabX(i + 1);
      edge(tabX(i), TAB_BORDER_Y, right, TAB_BORDER_Y, 0, 0);
    }
  }
  return copy(acc);
}

/** The dashed outline of the 76x38 header placeholder button. */
export function placeholder(): Dash[] {
  clear(acc);
  nextId = 2000;
  reset();
  edge(0, 0, 76, 0, 0, 0);
  edge(76, 0, 76, 38, -BORDER, 0);
  edge(76, 38, 0, 38, 0, -BORDER);
  edge(0, 38, 0, 0, 0, 0);
  return copy(acc);
}
