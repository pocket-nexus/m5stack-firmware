import { createSignal } from "solid-js";
import type { i32 } from "@pocketjs/framework/solid/std";

// The root model the compiled model mode requires; the view reads one signal.
export const [laps, setLaps] = createSignal<i32>(0);

export function lapText(): string {
  return `Lap ${laps()}`;
}
