// Renders the factory demo in PocketJS's deterministic simulator (the wasm
// core with the JS-mode bundle) and writes one PNG per step of a scripted
// tour: the startup animation, the launcher and each panel window. The
// screenshots are a simulator check of layout and artwork, not device
// validation; without a board the model simulates the device values.
//
//   bun run pocket compile --config apps/tab5-factory/pocket.host.config.ts \
//     --outdir .pocket-build/js --output .pocket-build/js/app.pkb
//   bun apps/tab5-factory/tools/preview.ts <bundle dir> <pocketjs.wasm> <out dir>
//
// The wasm core is third_party/pocketjs/platforms/web/pocketjs.wasm, built by
// `bun tools/wasm.ts` in the submodule (an ignored build output).
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import type { SimContact } from "../../../third_party/pocketjs/programs/sim/sim.ts";
import { encodePng } from "./png.ts";

// The bundle runs 30 core ticks per second (pocket.host.config.ts tickHz).
// The simulator reads its tick rate when sim.ts is first imported (60 by
// default), so set it before the import: one core tick per 30 Hz frame, as
// on the device; otherwise baked timelines (the spinners' animate-spin) run
// twice as fast.
(globalThis as { __POCKET_TICK_HZ__?: number }).__POCKET_TICK_HZ__ = 30;
const { bootBundle } = await import("../../../third_party/pocketjs/programs/sim/sim.ts");

const [bundleDir, wasm, outDir] = process.argv.slice(2).map(path => resolve(path));
if (!bundleDir || !wasm || !outDir) throw new Error("usage: preview.ts <bundle dir> <pocketjs.wasm> <out dir>");
mkdirSync(outDir, { recursive: true });

const world = await bootBundle({
  js: join(bundleDir, "tab5-factory.js"),
  pak: join(bundleDir, "tab5-factory.pak"),
  hz: 30,
  wasm,
  viewport: { width: 640, height: 360, rasterDensity: 2 },
});

function run(frames: number, touches?: SimContact[]): void {
  for (let i = 0; i < frames; i++) {
    if (!world.step(touches ? { touches } : {})) throw new Error(world.failure?.stack ?? "guest failed");
  }
}

function shot(name: string): void {
  const pixels = world.pixels();
  writeFileSync(join(outDir, `${name}.png`), encodePng(pixels));
  console.log(`${name}.png ${pixels.width}x${pixels.height}`);
}

/** A tap at a factory screen position (1280x720 raster). */
function tap(x: number, y: number): void {
  run(2, [{ x: x / 2, y: y / 2 }]);
  run(1);
}

/** Opens a panel window, captures it once settled, closes it from outside. */
function panel(name: string, x: number, y: number, outside: [number, number] = [20, 700]): void {
  tap(x, y);
  run(30);
  shot(name);
  tap(outside[0], outside[1]);
  run(30);
}

run(12);
shot("00-startup-start");
run(14);
shot("01-startup-tab");
run(20);
shot("02-startup-five");
run(60);
shot("03-launcher");
tap(925, 287);
run(20);
shot("04-toast-charging");
run(60);
panel("05-rtc-setting", 1062, 132);
panel("06-power-off", 1202, 41);
panel("07-sleep-touch", 1202, 115);
panel("08-sleep-shake", 1202, 188);
panel("09-sleep-rtc", 1202, 262);
panel("10-camera", 1238, 370);
panel("11-dual-mic", 486, 49);
panel("12-headphone", 330, 661);
panel("13-sd-card", 594, 660);
panel("14-i2c-scan", 135, 589);
panel("15-gpio", 1053, 245, [1250, 700]);
panel("16-music", 865, 639);
panel("17-com-monitor", 1137, 365);
tap(331, 173);
run(30);
shot("18-wifi");
tap(945, 150);
run(30);
tap(1145, 536);
run(10);
shot("19-brightness-up");
console.log(JSON.stringify({ frames: world.frames, failure: world.failure, logs: world.logs.slice(-5) }));
