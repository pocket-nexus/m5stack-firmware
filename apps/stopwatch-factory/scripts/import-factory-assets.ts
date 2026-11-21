// Converts the M5StopWatch-UserDemo artwork (LVGL v9 C image arrays, MIT) to
// the PNG files under ../images and writes ../images.json. Run from the
// repository root:
//
//   bun apps/stopwatch-factory/scripts/import-factory-assets.ts <factory-checkout>
//
// <factory-checkout> is a clone of https://github.com/m5stack/M5StopWatch-UserDemo
// at the commit named in apps/stopwatch-factory/README.md. The script only
// reads the C sources; it never executes code from the checkout.
//
// PocketJS image entries must be power-of-two textures of at most 512 px, so
// every picture is placed on a larger transparent canvas (black for opaque
// artwork shown on black). The views size each Image to its canvas and place
// it so the factory pixels land where the factory draws them.

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { deflateSync } from "node:zlib";

const source = process.argv[2];
if (!source) {
  console.error("usage: import-factory-assets.ts <M5StopWatch-UserDemo checkout>");
  process.exit(2);
}
const imageRoot = join(resolve(source), "main/assets/images");
const appDir = resolve(import.meta.dir, "..");
const out = join(appDir, "images");
rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });

const PSM_5650 = 0;
const PSM_4444 = 2;

interface Raster {
  width: number;
  height: number;
  rgba: Uint8Array;
}

interface Placement {
  /** Factory image name (without .c) under main/assets/images. */
  source: string;
  /** Output PNG name. */
  name: string;
  /** Artwork offset on the canvas; defaults to centred. */
  offset?: [number, number];
  /** Opaque artwork on a black canvas, stored as RGB565. */
  opaque?: boolean;
  /** Bilinear sampling for rotated artwork. */
  linear?: boolean;
}

const icons = [
  "icon_clock",
  "icon_watch_face",
  "icon_stopwatch",
  "icon_badge",
  "icon_imu",
  "icon_fft",
  "icon_lucky_wheel",
  "icon_setup",
  "fluidbox_icon",
];

const placements: Placement[] = [
  ...icons.map(name => ({ source: name, name, opaque: true })),
  { source: "icon_indicator_left", name: "icon_indicator_left" },
  { source: "icon_indicator_right", name: "icon_indicator_right" },
  { source: "icon_bat_lightning", name: "icon_bat_lightning" },
  { source: "icon_edit_badge", name: "icon_edit_badge" },
  { source: "alarm_icon", name: "alarm_icon" },
  // Rotated artwork keeps its top-left corner at the canvas origin; the views
  // set the rotation origin to the factory pivot.
  { source: "lucky_wheel_pointer", name: "lucky_wheel_pointer", offset: [0, 0], linear: true },
  { source: "watch_face/classic/classic_hour_hand", name: "classic_hour_hand", offset: [0, 0], linear: true },
  { source: "watch_face/classic/classic_minute_hand", name: "classic_minute_hand", offset: [0, 0], linear: true },
  { source: "watch_face/classic/classic_second_hand", name: "classic_second_hand", offset: [0, 0], linear: true },
];

// The 375 × 278 guide becomes four opaque tiles: 256 + 128 columns, 256 + 32 rows.
const guideTiles: [number, number, number, number][] = [
  [0, 0, 256, 256],
  [256, 0, 128, 256],
  [0, 256, 256, 32],
  [256, 256, 128, 32],
];

// Big-number digits are recoloured by the factory with one palette colour per
// digit position (watch_face/view/big_number.cpp). Each needed (digit,
// colour) pair is exported as a 128 × 128 RGBA4444 tile: the ink of every
// digit lies in x 8..111, y 10..125 of the 120 × 140 artwork, which the tile
// holds shifted by (+4, -4).
const bigPalettes = [
  [0xcbdb8c, 0xd3ea71, 0xe9f5b8, 0x94a350],
  [0xd1f3bf, 0xcdfcb9, 0xf8fee9, 0x84d86d],
  [0xdb9d7d, 0xe8d780, 0xf4ebc0, 0xf2b050],
  [0xb49edb, 0x9bc1ff, 0xc4c9fa, 0xaf94e7],
];
/** Digits each position can show: hour tens 0-2, minute tens 0-5. */
const bigDigitsPerPosition = [3, 10, 6, 10];

function chunk(type: string, data: Uint8Array): Uint8Array {
  const body = new Uint8Array(4 + data.length);
  body.set(new TextEncoder().encode(type), 0);
  body.set(data, 4);
  const result = new Uint8Array(12 + data.length);
  const view = new DataView(result.buffer);
  view.setUint32(0, data.length);
  result.set(body, 4);
  view.setUint32(8 + data.length, Bun.hash.crc32(body) >>> 0);
  return result;
}

function encodePng(image: Raster): Uint8Array {
  const { width, height, rgba } = image;
  const header = new Uint8Array(13);
  const view = new DataView(header.buffer);
  view.setUint32(0, width);
  view.setUint32(4, height);
  header.set([8, 6, 0, 0, 0], 8);
  const raw = new Uint8Array((width * 4 + 1) * height);
  for (let y = 0; y < height; y++) {
    raw.set(rgba.subarray(y * width * 4, (y + 1) * width * 4), y * (width * 4 + 1) + 1);
  }
  const parts = [
    new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk("IHDR", header),
    chunk("IDAT", deflateSync(raw, { level: 9 })),
    chunk("IEND", new Uint8Array()),
  ];
  const png = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
  let offset = 0;
  for (const part of parts) {
    png.set(part, offset);
    offset += part.length;
  }
  return png;
}

function readLvglImage(name: string): Raster {
  const path = join(imageRoot, `${name}.c`);
  const text = readFileSync(path, "utf8");
  const width = Number(/\.header\.w\s*=\s*(\d+)/.exec(text)?.[1]);
  const height = Number(/\.header\.h\s*=\s*(\d+)/.exec(text)?.[1]);
  const withAlpha = text.includes("LV_COLOR_FORMAT_RGB565A8");
  const body = text.split("_map[] = {")[1]?.split("};")[0] ?? "";
  const bytes = Uint8Array.from(body.match(/0x[0-9a-fA-F]{2}/g) ?? [], hex => parseInt(hex, 16));
  const count = width * height;
  if (bytes.length !== count * (withAlpha ? 3 : 2)) throw new Error(`${path}: unexpected data size`);
  const rgba = new Uint8Array(count * 4);
  for (let i = 0; i < count; i++) {
    const v = bytes[i * 2] | (bytes[i * 2 + 1] << 8);
    rgba[i * 4] = Math.round((((v >> 11) & 31) * 255) / 31);
    rgba[i * 4 + 1] = Math.round((((v >> 5) & 63) * 255) / 63);
    rgba[i * 4 + 2] = Math.round(((v & 31) * 255) / 31);
    rgba[i * 4 + 3] = withAlpha ? bytes[count * 2 + i] : 255;
  }
  return { width, height, rgba };
}

const pow2 = (n: number) => 2 ** Math.ceil(Math.log2(n));

/** Copies `art` onto a new canvas at (dx, dy); other pixels stay clear or black. */
function place(art: Raster, width: number, height: number, dx: number, dy: number, opaque: boolean): Raster {
  const rgba = new Uint8Array(width * height * 4);
  if (opaque) for (let i = 3; i < rgba.length; i += 4) rgba[i] = 255;
  for (let y = 0; y < height; y++) {
    const sy = y - dy;
    if (sy < 0 || sy >= art.height) continue;
    for (let x = 0; x < width; x++) {
      const sx = x - dx;
      if (sx < 0 || sx >= art.width) continue;
      const from = (sy * art.width + sx) * 4;
      rgba.set(art.rgba.subarray(from, from + 4), (y * width + x) * 4);
    }
  }
  return { width, height, rgba };
}

const meta: Record<string, { psm?: number; linear?: boolean }> = {};
function write(name: string, image: Raster, entry: { psm?: number; linear?: boolean }) {
  writeFileSync(join(out, `${name}.png`), encodePng(image));
  if (entry.psm !== undefined || entry.linear) meta[`images/${name}.png`] = entry;
}

for (const p of placements) {
  const art = readLvglImage(p.source);
  const width = pow2(art.width);
  const height = pow2(art.height);
  const [dx, dy] = p.offset ?? [Math.floor((width - art.width) / 2), Math.floor((height - art.height) / 2)];
  write(p.name, place(art, width, height, dx, dy, p.opaque ?? false), {
    ...(p.opaque ? { psm: PSM_5650 } : {}),
    ...(p.linear ? { linear: true } : {}),
  });
  console.log(`${p.name}: ${art.width}x${art.height} at (${dx}, ${dy}) on ${width}x${height}`);
}

const guide = readLvglImage("go_home_guide");
guideTiles.forEach(([x, y, w, h], index) => {
  write(`go_home_guide_${index}`, place(guide, w, h, -x, -y, true), { psm: PSM_5650 });
});

for (let digit = 0; digit < 10; digit++) {
  const art = readLvglImage(`watch_face/big_number/big_number_${digit}`);
  bigPalettes.forEach((palette, p) => {
    palette.forEach((color, position) => {
      if (digit >= bigDigitsPerPosition[position]) return;
      const tile = place(art, 128, 128, 4, -4, false);
      for (let i = 0; i < tile.rgba.length; i += 4) {
        tile.rgba[i] = (color >> 16) & 255;
        tile.rgba[i + 1] = (color >> 8) & 255;
        tile.rgba[i + 2] = color & 255;
      }
      write(`bn_${p}${position}_${digit}`, tile, { psm: PSM_4444 });
    });
  });
}

writeFileSync(join(appDir, "images.json"), `${JSON.stringify(meta, null, 2)}\n`);
