// IMU window (ui_function_imu_window.cpp). The board fuses the LSM6DS3TR-C
// (Mahony filter, gyro bias calibrated from 60 still samples at every open)
// and renders the textured device model into the texture
// "assets/imu-canvas.png" (460x418 of a 512x512 texture) at up to 30 Hz.
// One finger turns the model, two fingers twist it; the header button
// recentres it. Until the first pose the panel shows "Initializing IMU..."
// then "Keep sensor still / Calibrating IMU..."; a failed start shows the
// error and closes the window after 5 s.
import { idiv, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import { framesIn, TICK_HZ } from "./ease";

export const STARTING: i32 = 0;
export const CALIBRATING: i32 = 1;
export const READY: i32 = 2;
export const FAILED: i32 = 3;

/** Frames until a failed start closes the window, -1 before it failed. */
let closeFrames: i32 = -1;
/** Exit hides the content before the close animation. */
export let hidden: boolean = false;

export function open(): void {
  closeFrames = -1;
  hidden = false;
  device.request("imu 1");
}

export function close(): void {
  device.request("imu 0");
}

export function recenter(): void {
  device.request("imuRecenter");
}

export function exit(): void {
  hidden = true;
}

/** One frame; true when the failure countdown ran out (the window is
 *  deleted without its close animation). */
export function step(): boolean {
  if (device.imuState !== FAILED) {
    closeFrames = -1;
    return false;
  }
  if (closeFrames < 0) {
    closeFrames = framesIn(5000);
  }
  closeFrames = closeFrames - 1;
  return closeFrames <= 0;
}

export function overlayShown(): boolean {
  return !hidden && device.imuState !== READY;
}

export function overlayText(): string {
  if (device.imuState === FAILED) {
    const seconds: i32 = closeFrames < 0 ? 5 : idiv(closeFrames + TICK_HZ - 1, TICK_HZ);
    return `IMU unavailable\nInit failed: ${device.imuError}\nReturning in ${seconds < 0 ? 0 : seconds}s`;
  }
  if (device.imuState === CALIBRATING) {
    return "Keep sensor still\nCalibrating IMU...";
  }
  return "Initializing IMU...";
}

export function overlayError(): boolean {
  return device.imuState === FAILED;
}
