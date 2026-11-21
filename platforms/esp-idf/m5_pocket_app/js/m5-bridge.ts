// JS-mode half of the m5_pocket_app device bridge. main.tsx (JS only) calls
// connectDevice() inside mount(); AOT builds never execute this file: their
// Rust host writes the same device.ts fields through the generated accessors.
//
// The C host installs `globalThis.m5` after boot (src/app_js.c). Every frame,
// before the app's own frame handlers, this callback
//   0. binds the image names m5_pocket_app_set_texture uploaded pictures for
//      (registerTexture), so a view that remounts on the next value draws
//      them;
//   1. delivers each board value changed since the previous frame to the root
//      model's hostNumber / hostText / hostArray, then queued board events to
//      hostEvent;
//   2. takes the request strings device.ts queued and passes each to the
//      program's request handler.
import { registerTexture } from "@pocketjs/framework";
import { onFrame } from "@pocketjs/framework/solid/lifecycle";

/** The realm namespace the C host installs. */
export interface M5Namespace {
  sync(
    number: (name: string, value: number) => void,
    text: (name: string, value: string) => void,
    array: (name: string, values: number[]) => void,
    event: (name: string, value: number) => void,
  ): number;
  request(text: string): boolean;
  textures(bind: (name: string, handle: number) => void): number;
  readonly mode: "js";
}

declare global {
  // eslint-disable-next-line no-var
  var m5: M5Namespace | undefined;
}

/** The root model exports (app.ts) that forward to device.ts. */
export interface DeviceHost {
  hostNumber(name: string, value: number): void;
  hostText(name: string, value: string): void;
  hostArray(name: string, values: number[]): void;
  hostEvent(name: string, value: number): void;
  takeRequests(): string[];
}

/** Register the per-frame bridge. Call inside mount(), before returning the root view. */
export function connectDevice(host: DeviceHost): void {
  onFrame(() => {
    const bridge = globalThis.m5;
    if (!bridge) return;
    bridge.textures(registerTexture);
    bridge.sync(host.hostNumber, host.hostText, host.hostArray, host.hostEvent);
    for (const request of host.takeRequests()) bridge.request(request);
  });
}
