// Renders the factory demo in PocketJS's deterministic simulator (the wasm
// core with the JS-mode bundle) and writes one PNG per step of a scripted
// tour: the three screens and each function window. The screenshots are a
// simulator check of layout and artwork, not device validation; without a
// board the model shows its defaults.
//
//   bun run pocket compile --config apps/corep4x-factory/pocket.host.config.ts \
//     --outdir .pocket-build/js --output .pocket-build/js/app.pkb
//   bun apps/corep4x-factory/tools/preview.ts <bundle dir> <pocketjs.wasm> <out dir> [window...]
//
// The wasm core is third_party/pocketjs/platforms/web/pocketjs.wasm, built by
// `bun tools/wasm.ts` in the submodule (an ignored build output).
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import type { SimContact } from "../../../third_party/pocketjs/programs/sim/sim.ts";
import { encodePng } from "./png.ts";

// One core tick per 30 Hz frame, as on the device (pocket.host.config.ts).
(globalThis as { __POCKET_TICK_HZ__?: number }).__POCKET_TICK_HZ__ = 30;
const { bootBundle } = await import("../../../third_party/pocketjs/programs/sim/sim.ts");

// A stand-in for the C host's globalThis.m5
// (platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts): the board publishes the
// contact as pointerX, pointerY and pointerDown every frame.
const numbers = new Map<string, number>();
(globalThis as Record<string, unknown>).m5 = {
  mode: "js",
  sync(number: (name: string, value: number) => void): number {
    const calls = numbers.size;
    for (const [name, value] of numbers) number(name, value);
    numbers.clear();
    return calls;
  },
  request: () => true,
  textures: () => 0,
};

const args = process.argv.slice(2);
const [bundleDir, wasm, outDir] = args.slice(0, 3).map(path => resolve(path));
const only = new Set(args.slice(3));
if (!bundleDir || !wasm || !outDir) throw new Error("usage: preview.ts <bundle dir> <pocketjs.wasm> <out dir> [window...]");
mkdirSync(outDir, { recursive: true });

const world = await bootBundle({
  js: join(bundleDir, "corep4x-factory.js"),
  pak: join(bundleDir, "corep4x-factory.pak"),
  hz: 30,
  wasm,
  viewport: { width: 480, height: 480, rasterDensity: 1 },
});

function run(frames: number, touches?: SimContact[]): void {
  for (let i = 0; i < frames; i++) {
    const contact = touches?.[0];
    numbers.set("pointerDown", contact ? 1 : 0);
    if (contact) {
      numbers.set("pointerX", Math.round(contact.x));
      numbers.set("pointerY", Math.round(contact.y));
    }
    if (!world.step(touches ? { touches } : {})) throw new Error(world.failure?.stack ?? "guest failed");
  }
}

function shot(name: string): void {
  const pixels = world.pixels();
  writeFileSync(join(outDir, `${name}.png`), encodePng(pixels));
  console.log(`${name}.png ${pixels.width}x${pixels.height}`);
}

function tap(x: number, y: number): void {
  run(2, [{ x, y }]);
  run(1);
}

/** A horizontal swipe from x0 to x1 at y over `frames` frames. */
function swipe(x0: number, x1: number, y: number, frames = 6): void {
  for (let i = 0; i <= frames; i++) run(1, [{ x: x0 + ((x1 - x0) * i) / frames, y }]);
  run(1);
}

/** Opens a window from its function-screen area, captures it, exits. */
function window(name: string, x: number, y: number, steps: (() => void)[] = []): void {
  if (only.size && !only.has(name)) return;
  tap(x, y);
  run(15);
  shot(name);
  steps.forEach(step => step());
  tap(426, 28);
  run(15);
}

run(10);
shot("startup");
swipe(240, 400, 240);
run(20);
shot("info");
swipe(400, 240, 240);
run(20);
swipe(400, 100, 240);
run(20);
shot("function");
window("power", 65, 110, [
  () => { tap(175, 446); run(3); shot("power-system"); },
  () => { tap(285, 446); run(3); shot("power-m5bus"); },
  () => { tap(122, 134); run(3); shot("power-gpio"); tap(325, 302); run(3); },
  () => { tap(175, 446); run(3); tap(340, 160); run(10); shot("power-lcd-warning"); tap(140, 342); run(3); },
]);
if (!only.size || only.has("touch")) {
  // The canvas closes the window after 10 s without a touch.
  tap(180, 110);
  run(15);
  shot("touch");
  run(310);
}
window("imu", 65, 245);
window("rec", 180, 245, [() => { tap(338, 28); run(3); shot("rec-set"); tap(380, 340); run(3); }]);
window("music", 410, 380);
window("ping", 295, 245);
window("wifi", 65, 380);
window("usb", 180, 380, [() => { run(40); shot("usb-checked"); }]);
window("ir", 295, 380);
window("sd", 295, 110);
window("i2c", 410, 110, [() => { tap(134, 109); run(3); shot("i2c-internal"); }]);
window("timer", 410, 245);
window("sleep", 330, 30);
window("off", 438, 30);
console.log(JSON.stringify({ frames: world.frames, failure: world.failure, logs: world.logs.slice(-5) }));
