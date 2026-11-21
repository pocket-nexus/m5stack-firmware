// Records the models hand to the views. Pure module: types and constants only.
import type { Color, Deg, f32, i32 } from "@pocketjs/framework/solid/std";

export type Page = "home" | "maze" | "dice" | "reaction" | "snake" | "level" | "sound" | "metronome";

/** Axis-aligned rectangle in screen pixels (maze walls, meter bars), with a
 * style variant. */
export interface Box {
  id: i32;
  x: i32;
  y: i32;
  w: i32;
  h: i32;
  kind: i32;
}

/** A dot or cell at a screen position, with a style variant. */
export interface Spot {
  id: i32;
  x: i32;
  y: i32;
  kind: i32;
}

/** One 3D die: screen centre, lift above its shadow, cube rotation and the
 * shade of each face (1, 2, 3, 4, 5, 6). */
export interface Die {
  id: i32;
  x: i32;
  y: i32;
  lift: i32;
  rx: Deg;
  ry: Deg;
  shadow: f32;
  shades: Color[];
}
