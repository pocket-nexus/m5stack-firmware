// IMU app (factory app_imu): a ball that follows the acceleration and grows
// with the rotation rate, a dot orbiting with the integrated yaw, and the
// X/Y/Z acceleration labels; a tap hides the labels and the orbit dot.
import { clamp, cos, fixed, round, sin, sqrt, type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

export let ballX = 0.0;
export let ballY = 0.0;
export let ballSize = 80.0;
export let orbit = 0.0;
let yaw = 0.0;
export let labels: boolean = true;

export function open(): void {
  ballX = 0.0;
  ballY = 0.0;
  ballSize = 80.0;
  orbit = 0.0;
  yaw = 0.0;
  labels = true;
}

export function tap(): void {
  labels = !labels;
}

/** Linear tween re-targeted every frame: move by dt / duration of the gap. */
function follow(value: f64, target: f64, dt: f64, duration: f64): f64 {
  return value + (target - value) * clamp(dt / duration, 0.0, 1.0);
}

export function update(dt: f64): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    return true;
  }
  const gx = device.gyroX;
  const gy = device.gyroY;
  const gz = device.gyroZ;
  const spin = clamp(sqrt(gx * gx + gy * gy + gz * gz) / 180.0, 0.0, 1.0);
  const size = 80.0 + 40.0 * spin;
  const reach = 289.0 / 2.0 - size / 2.0;
  ballSize = follow(ballSize, size, dt, 0.3);
  ballX = follow(ballX, reach * clamp(device.accelX, -1.0, 1.0), dt, 0.1);
  ballY = follow(ballY, reach * clamp(device.accelY, -1.0, 1.0), dt, 0.1);
  if (labels) {
    yaw = yaw + gz * dt;
    while (yaw >= 360.0) {
      yaw = yaw - 360.0;
    }
    while (yaw < 0.0) {
      yaw = yaw + 360.0;
    }
    let delta = yaw - orbit;
    while (delta > 180.0) {
      delta = delta - 360.0;
    }
    while (delta < -180.0) {
      delta = delta + 360.0;
    }
    orbit = follow(orbit, orbit + delta, dt, 0.2);
  }
  return false;
}

export function orbitX(): i32 {
  return round(214.0 * cos((orbit - 90.0) * 0.017453292519943295));
}

export function orbitY(): i32 {
  return round(214.0 * sin((orbit - 90.0) * 0.017453292519943295));
}

export function label(axis: string, value: f64): string {
  return `${axis}: ${fixed(value, 1)}`;
}
