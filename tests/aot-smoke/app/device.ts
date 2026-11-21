// Device state module: every host-provided value and every request to the
// host. The board publishes values under these field names
// (m5_pocket_bridge_set_*); in JS mode main.tsx delivers them through the
// host* functions, in AOT mode the Rust host writes device_<name>_mut().
import { clear, copy, i32, push, type f32, type f64 } from "@pocketjs/framework/solid/std";

// Board -> app
export let battery: i32 = 0;
export let charging: boolean = false;
export let clock: string = "--:--";
export let spectrum: f32[] = [];
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;
export let pointerDown: boolean = false;
export let alarms: i32 = 0;

// App -> board: request strings "<name> <arg>...", drained after each frame.
export let requests: string[] = [];

export function request(command: string): void {
  push(requests, command);
}

export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "battery":
      battery = i32(value);
      break;
    case "charging":
      charging = value !== 0;
      break;
    case "pointerX":
      pointerX = i32(value);
      break;
    case "pointerY":
      pointerY = i32(value);
      break;
    case "pointerDown":
      pointerDown = value !== 0;
      break;
  }
}

export function hostText(name: string, value: string): void {
  switch (name) {
    case "clock":
      clock = value;
      break;
  }
}

export function hostArray(name: string, values: f32[]): void {
  switch (name) {
    case "spectrum":
      spectrum = values;
      break;
  }
}

export function hostEvent(name: string, value: f64): void {
  switch (name) {
    case "alarm":
      alarms = alarms + i32(value);
      break;
  }
}

export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}
