// The function window frame of the factory firmware
// (ui_function_window.cpp): a 170x48 header at (300,4) holding the action
// button and Exit, a 460x418 content panel at (10,52), and a 2 px dashed
// outline (dashes 7 px, gaps 5 px, 0xCBD5E1) around both, which the factory
// draws as one rectangle per dash. A paged window has a 460x370 content
// panel, tabs 110x50 along y 421 and dashed tab separators. All values are
// screen pixels.
//
// The dashes of one straight edge are the periodic pattern of the strip
// textures assets/dash-h.png (512x2) and assets/dash-v.png (2x512): 7 px of
// 0xCBD5E1, 5 px clear, from the strip's first pixel. Each edge is a view
// clipped to the edge with its strip shifted onto the edge's dashes: three
// nodes per edge (Dashes.tsx), the same pixels as one rectangle per dash.
import { abs, clear, copy, imod, min, push, type i32 } from "@pocketjs/framework/solid/std";

/** One dashed edge: the clip rectangle and the strip's corner in it. */
export interface Edge {
  id: i32;
  x: i32;
  y: i32;
  w: i32;
  h: i32;
  /** The strip's offset along the edge, -11..0; 0 across it. */
  sx: i32;
  sy: i32;
  vertical: boolean;
}

const BORDER: i32 = 2;
/** Dashes of 7 px, gaps of 5 px. */
const DASH: i32 = 7;
const PERIOD: i32 = 12;

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
let acc: Edge[] = [];
let nextId: i32 = 0;
/** The dash pattern carried from one edge to the next, as the position in
 *  its 12 px period (0..6 on a dash, 7..11 in a gap). */
let phase: i32 = 0;

/** add_dashed_axis_edge: one straight edge, continuing the dash pattern. The
 *  factory steps from (x1,y1) towards (x2,y2) and draws a rectangle per dash
 *  run; the same pixels are the strip clipped to the edge, its period
 *  starting on a coordinate where a dash starts. */
function edge(x1: i32, y1: i32, x2: i32, y2: i32, ox: i32, oy: i32): void {
  const horizontal = y1 === y2;
  const from: i32 = horizontal ? x1 : y1;
  const to: i32 = horizontal ? x2 : y2;
  const length: i32 = abs(to - from);
  // Towards larger coordinates the pixel at `from` has the carried phase;
  // towards smaller ones the pixel at from - 1 has it, and the dash that
  // ends there starts DASH - 1 pixels lower.
  const dashStart: i32 = to > from ? from - phase : from + phase - DASH;
  const low: i32 = min(from, to);
  const shift: i32 = -imod(imod(low - dashStart, PERIOD) + PERIOD, PERIOD);
  push(acc, {
    id: nextId,
    x: (horizontal ? low : x1) + ox,
    y: (horizontal ? y1 : low) + oy,
    w: horizontal ? length : BORDER,
    h: horizontal ? BORDER : length,
    sx: horizontal ? shift : 0,
    sy: horizontal ? 0 : shift,
    vertical: !horizontal,
  });
  nextId = nextId + 1;
  phase = imod(phase + length, PERIOD);
}

function reset(): void {
  phase = 0;
}

/** add_function_window_outline (single window) or the static outline of a
 *  paged window with `pages` tabs: the same six edges, plus the vertical
 *  separators between tabs. */
export function outline(pages: i32): Edge[] {
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
export function tabEdges(pages: i32, active: i32): Edge[] {
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
export function placeholder(): Edge[] {
  clear(acc);
  nextId = 2000;
  reset();
  edge(0, 0, 76, 0, 0, 0);
  edge(76, 0, 76, 38, -BORDER, 0);
  edge(76, 38, 0, 38, 0, -BORDER);
  edge(0, 38, 0, 0, 0, 0);
  return copy(acc);
}
