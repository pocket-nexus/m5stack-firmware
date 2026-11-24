// TOUCH window (ui_function_touch_window.cpp). Opening it opens a full-screen
// canvas: a 5x5 grid the board draws (strokes in random palette colours, a
// visited cell turns 0x1E293B) and uploads as the texture
// "assets/touch-canvas.png". The pill shows the visited count; at 25/25
// Info, Clean and Exit appear. Ten seconds without a touch close the whole
// window. Info shows the controller page with the touch indicator switch.
import type { i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export let canvasShown: boolean = false;
let visitedSeen: i32 = -1;
let idleFrames: i32 = 0;

export function open(): void {
  canvasShown = true;
  idleFrames = 0;
  device.request("touchCanvas 1");
}

export function close(): void {
  canvasShown = false;
  device.request("touchCanvas 0");
}

export function showInfo(): void {
  canvasShown = false;
  device.request("touchCanvasPause 1");
}

export function showCanvas(): void {
  canvasShown = true;
  idleFrames = 0;
  device.request("touchCanvasPause 0");
}

/** One frame of the idle timeout: 10 s (300 frames) without a contact
 *  while the canvas is shown closes the window; true then. */
export function idle(down: boolean): boolean {
  if (!canvasShown || down) {
    idleFrames = 0;
    return false;
  }
  idleFrames = idleFrames + 1;
  return idleFrames >= 300;
}

export function clean(): void {
  device.request("touchCanvasClean");
}

export function complete(): boolean {
  return device.touchVisited >= 25;
}

export function pillText(): string {
  return `${device.touchVisited}/25`;
}

export function toggleIndicator(): void {
  device.setTouchIndicator(!device.touchIndicator);
}

/** The board's status lines of the info page. */
export function statusText(): string {
  return device.touchStatus;
}

export function visitedChanged(): boolean {
  const changed = device.touchVisited !== visitedSeen;
  visitedSeen = device.touchVisited;
  return changed;
}
