// IMU window (ui_function_imu_window.cpp). The board fuses the LSM6DS3TR-C
// (Mahony filter, gyro bias calibrated from 60 still samples at every open)
// and renders the textured device model into the texture
// "assets/imu-canvas.png" (460x418 of a 512x512 texture) at up to 30 Hz.
// One finger turns the model, two fingers twist it; the header button
// recentres it. Until the first pose the panel shows "Initializing IMU..."
// then "Keep sensor still / Calibrating IMU..."; a failed start shows the
// error and closes the window after 5 s.
import { i32, type f64 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const STARTING: i32 = 0;
export const CALIBRATING: i32 = 1;
export const READY: i32 = 2;
export const FAILED: i32 = 3;

let closeMs: f64 = -1.0;
/** Exit hides the content before the close animation. */
export let hidden: boolean = false;

export function open(): void {
  closeMs = -1.0;
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
    closeMs = -1.0;
    return false;
  }
  if (closeMs < 0) {
    closeMs = 5000.0;
  }
  closeMs = closeMs - 33.333332;
  return closeMs <= 0;
}

export function overlayShown(): boolean {
  return !hidden && device.imuState !== READY;
}

export function overlayText(): string {
  if (device.imuState === FAILED) {
    const seconds = closeMs < 0 ? 5 : i32((closeMs + 999.0) / 1000.0);
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
