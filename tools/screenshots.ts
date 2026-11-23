// Renders the README screenshots (docs/images/) in PocketJS's deterministic
// simulator: each application's JS-mode bundle runs in the wasm core at its
// board's logical viewport and raster density, a scripted tour drives it,
// and the tool writes one PNG per stop. The images show layout and artwork as
// the simulator draws them; they are not photographs of a device.
//
//   bun tools/screenshots.ts [tab5|stopwatch|sticks3|atoms3]...
//
// Needs the wasm core third_party/pocketjs/platforms/web/pocketjs.wasm
// (`bun tools/wasm.ts` inside the submodule). Bundles are compiled with
// tools/pocket into .pocket-build/screenshots/<app>/.
//
// Without a board the applications simulate their device values (clock,
// battery, scan results). A stand-in for the C host's `globalThis.m5`
// (platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts) delivers the key levels
// and sensor values a tour sets, by the device.ts field names; it never
// publishes `uptimeMs`, so each app keeps its own clock.
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { encodePng, type Picture } from "../apps/tab5-factory/tools/png.ts";
import type { BundleWorld, SimContact } from "../third_party/pocketjs/programs/sim/sim.ts";

const root = resolve(import.meta.dir, "..");
const wasm = join(root, "third_party/pocketjs/platforms/web/pocketjs.wasm");
const images = join(root, "docs/images");

// The bundles run 30 core ticks per second (each pocket.host.config.ts
// tickHz). sim.ts reads the tick rate when it is first imported.
(globalThis as { __POCKET_TICK_HZ__?: number }).__POCKET_TICK_HZ__ = 30;
const { bootBundle } = await import("../third_party/pocketjs/programs/sim/sim.ts");

// contracts/spec/spec.ts BTN.
const BTN = { LTRIGGER: 0x0100, RTRIGGER: 0x0200, CIRCLE: 0x2000, CROSS: 0x4000 } as const;

/** Board → app values waiting for the next frame's m5.sync. */
const numbers = new Map<string, number>();
const arrays = new Map<string, number[]>();
(globalThis as Record<string, unknown>).m5 = {
  mode: "js",
  sync(
    number: (name: string, value: number) => void,
    _text: (name: string, value: string) => void,
    array: (name: string, values: number[]) => void,
  ): number {
    const calls = numbers.size + arrays.size;
    for (const [name, value] of numbers) number(name, value);
    for (const [name, values] of arrays) array(name, values);
    numbers.clear();
    arrays.clear();
    return calls;
  },
  request: () => true,
  textures: () => 0,
};

interface Tour {
  app: string;
  viewport: { width: number; height: number; rasterDensity: number };
  /** Nearest-neighbour enlargement of the written pictures. */
  scale: number;
  /** Transparent outside the inscribed circle (round panel). */
  round: boolean;
  run(t: Driver): void;
}

interface Driver {
  /** Steps frames with the given buttons held and contacts down. */
  run(frames: number, buttons?: number, touches?: SimContact[]): void;
  /** A one-frame contact at a logical position, then two frames at rest. */
  tap(x: number, y: number): void;
  /** Holds a button mask for `frames` frames, then one frame at rest. */
  press(buttons: number, frames?: number): void;
  /** Sets a device.ts field for the next frame. */
  set(name: string, value: number | number[]): void;
  /** Writes docs/images/<board>-<name>.png. */
  shot(name: string): void;
}

function compile(app: string): string {
  const out = join(root, ".pocket-build/screenshots", app);
  mkdirSync(out, { recursive: true });
  const config = join(root, "apps", app, "pocket.host.config.ts");
  const result = Bun.spawnSync([join(root, "tools/pocket"), "compile", "--config", config, "--outdir", out, "--output", join(out, "app.pkb")], {
    stdout: "ignore",
    stderr: "inherit",
  });
  if (!result.success) throw new Error(`pocket compile failed for ${app}`);
  return out;
}

function enlarge(picture: Picture, scale: number, round: boolean): Picture {
  const width = picture.width * scale;
  const height = picture.height * scale;
  const rgba = new Uint8Array(width * height * 4);
  const cx = width / 2;
  const cy = height / 2;
  const radius = Math.min(width, height) / 2;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const from = ((Math.floor(y / scale) * picture.width) + Math.floor(x / scale)) * 4;
      const to = (y * width + x) * 4;
      rgba.set(picture.rgba.subarray(from, from + 4), to);
      if (round) {
        // Anti-aliased edge: coverage of the pixel centre within one pixel of the rim.
        const distance = Math.hypot(x + 0.5 - cx, y + 0.5 - cy);
        rgba[to + 3] = Math.round(255 * Math.min(1, Math.max(0, radius - distance + 0.5)));
      } else {
        rgba[to + 3] = 255;
      }
    }
  }
  return { width, height, rgba };
}

async function render(board: string, tour: Tour): Promise<void> {
  const out = compile(tour.app);
  const world: BundleWorld = await bootBundle({
    js: join(out, `${tour.app}.js`),
    pak: join(out, `${tour.app}.pak`),
    hz: 30,
    wasm,
    viewport: tour.viewport,
  });
  const step = (buttons: number, touches?: SimContact[]): void => {
    if (!world.step({ buttons, touches })) throw new Error(world.failure?.stack ?? `${tour.app}: guest failed`);
  };
  const driver: Driver = {
    run(frames, buttons = 0, touches) {
      for (let i = 0; i < frames; i++) step(buttons, touches);
    },
    tap(x, y) {
      step(0, [{ x, y }]);
      driver.run(2);
    },
    press(buttons, frames = 1) {
      driver.run(frames, buttons);
      driver.run(1);
    },
    set(name, value) {
      if (Array.isArray(value)) arrays.set(name, value);
      else numbers.set(name, value);
    },
    shot(name) {
      const file = join(images, `${board}-${name}.png`);
      writeFileSync(file, encodePng(enlarge(world.pixels(), tour.scale, tour.round)));
      console.log(`docs/images/${board}-${name}.png`);
    },
  };
  tour.run(driver);
  numbers.clear();
  arrays.clear();
}

const tours: Record<string, Tour> = {
  tab5: {
    app: "tab5-factory",
    viewport: { width: 640, height: 360, rasterDensity: 2 },
    scale: 1,
    round: false,
    run(t) {
      // Factory coordinates are the 1280x720 raster; the viewport is half that.
      const tap = (x: number, y: number) => t.tap(x / 2, y / 2);
      const panel = (name: string, x: number, y: number) => {
        tap(x, y);
        t.run(30);
        t.shot(name);
        tap(20, 700);
        t.run(30);
      };
      t.run(106);
      t.shot("launcher");
      panel("rtc", 1062, 132);
      panel("i2c-scan", 135, 589);
      tap(1053, 245);
      t.run(30);
      t.shot("gpio");
    },
  },
  stopwatch: {
    app: "stopwatch-factory",
    viewport: { width: 466, height: 466, rasterDensity: 1 },
    scale: 1,
    round: true,
    run(t) {
      // Launcher order: FluidBox, AlarmClock, WatchFace, Stopwatch, Badge,
      // IMU, Audio.FFT, LuckyWheel, Settings.
      const next = () => {
        t.tap(433, 233);
        t.run(20);
      };
      const open = () => {
        t.tap(233, 218);
        t.run(30);
      };
      const home = () => {
        t.set("keyA", 1);
        t.set("keyB", 1);
        t.run(20);
        t.set("keyA", 0);
        t.set("keyB", 0);
        t.run(30);
      };
      t.run(40);
      next();
      next();
      t.shot("launcher");
      open();
      t.run(30);
      t.shot("watch-classic");
      t.press(BTN.RTRIGGER);
      t.run(60);
      t.shot("watch-flow");
      t.press(BTN.RTRIGGER);
      t.run(60);
      t.shot("watch-big");
      home();
      next();
      open();
      // KEYB starts the stopwatch, KEYA records a lap.
      t.press(BTN.RTRIGGER);
      t.run(95);
      t.press(BTN.LTRIGGER);
      t.run(110);
      t.press(BTN.LTRIGGER);
      t.run(40);
      t.shot("stopwatch");
      home();
      for (let i = 0; i < 4; i++) next();
      open();
      // Drag the option roller up by four rows, then OK. The rollers read
      // the pointer fields the C host publishes from the touch contact.
      for (let i = 0; i <= 12; i++) {
        t.set("pointerDown", 1);
        t.set("pointerX", 233);
        t.set("pointerY", 300 - i * 12);
        t.run(1);
      }
      t.set("pointerDown", 0);
      t.run(30);
      t.tap(233, 420);
      t.run(30);
      t.tap(233, 233);
      t.run(45);
      t.shot("wheel");
    },
  },
  sticks3: {
    app: "sticks3-demos",
    viewport: { width: 135, height: 240, rasterDensity: 1 },
    scale: 2,
    round: false,
    run(t) {
      // Home cards: Marble Maze, Dice, Reaction, Snake, Spirit Level, Sound
      // Meter, Metronome. Side key: next card; front key: open; front held
      // 700 ms: home.
      const key = (name: "keyFront" | "keySide", frames = 2) => {
        t.set(name, 1);
        t.run(frames);
        t.set(name, 0);
        t.run(2);
      };
      const home = () => {
        key("keyFront", 26);
        t.run(20);
      };
      const card = () => {
        key("keySide");
        t.run(20);
      };
      t.set("accelZ", 1);
      t.run(30);
      t.shot("home");
      key("keyFront");
      t.set("accelX", -0.25);
      t.set("accelY", -0.2);
      t.run(70);
      t.shot("maze");
      t.set("accelX", 0);
      t.set("accelY", 0);
      home();
      card();
      key("keyFront");
      t.run(80);
      key("keyFront");
      t.run(80);
      t.shot("dice");
      home();
      card();
      card();
      key("keyFront");
      t.run(70);
      key("keyFront");
      t.run(40);
      t.shot("snake");
      home();
      card();
      key("keyFront");
      t.set("accelX", 0.06);
      t.set("accelY", -0.04);
      t.set("accelZ", 0.99);
      t.run(70);
      t.shot("level");
      home();
      card();
      key("keyFront");
      for (let i = 0; i < 120; i++) {
        const level = -46 + 10 * Math.sin(i / 4);
        t.set("micOn", 1);
        t.set("micDb", level);
        t.set("micPeakDb", level + 6);
        t.run(1);
      }
      t.shot("sound");
      home();
      card();
      key("keyFront");
      t.run(70);
      key("keyFront");
      t.run(20);
      t.shot("metronome");
    },
  },
  atoms3: {
    app: "atoms3-factory",
    viewport: { width: 128, height: 128, rasterDensity: 1 },
    scale: 2,
    round: false,
    run(t) {
      // One button: a click (BTN.CIRCLE) shows the next function, a hold
      // (BTN.CROSS) enters or leaves it. Menu order: Wi-Fi, I2C, UART, PWM,
      // ADC, IR, IMU.
      const click = () => {
        t.press(BTN.CIRCLE);
        t.run(10);
      };
      const hold = () => {
        t.press(BTN.CROSS);
        t.run(10);
      };
      const visit = (name: string, frames: number) => {
        hold();
        t.run(frames);
        t.shot(name);
        hold();
        click();
      };
      t.run(90);
      t.shot("menu");
      visit("wifi", 150);
      visit("i2c", 60);
      visit("uart", 30);
      visit("pwm", 30);
      click();
      click();
      visit("imu", 60);
    },
  },
};

mkdirSync(images, { recursive: true });
const selected = Bun.argv.slice(2);
for (const board of selected.length > 0 ? selected : Object.keys(tours)) {
  const tour = tours[board];
  if (!tour) throw new Error(`unknown board ${board}; one of ${Object.keys(tours).join(", ")}`);
  await render(board, tour);
}
