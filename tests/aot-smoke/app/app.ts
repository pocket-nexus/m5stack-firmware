import { createSignal } from "solid-js";
import { len, type f32, type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const [count, setCount] = createSignal<i32>(0);
export const [taps, setTaps] = createSignal<i32>(0);

export function increment(): void {
  setCount(count() + 1);
  device.request(`tone 880 ${count() * 10}`);
}

export function tap(): void {
  setTaps(taps() + 1);
  device.request("vibrate 30");
}

export function status(): string {
  return `${device.clock} ${device.battery}%${device.charging ? "+" : ""} bands ${len(device.spectrum)} alarms ${device.alarms}`;
}

export function pointer(): string {
  return device.pointerDown ? `touch ${device.pointerX},${device.pointerY}` : "no touch";
}

// Root exports that main.tsx (JS mode) uses to reach device.ts.
export function hostNumber(name: string, value: f64): void {
  device.hostNumber(name, value);
}
export function hostText(name: string, value: string): void {
  device.hostText(name, value);
}
export function hostArray(name: string, values: f32[]): void {
  device.hostArray(name, values);
}
export function hostEvent(name: string, value: f64): void {
  device.hostEvent(name, value);
}
export function takeRequests(): string[] {
  return device.takeRequests();
}
