// Converts the factory firmware's LVGL image sources into the PNG pairs the
// PocketJS build reads: `<name>@2x.png` holds the factory pixels for the
// 2x raster of the Tab5 host, `<name>.png` the same picture at half size.
// PocketJS textures are power-of-two sized and at most 512 px on a side, so
// each picture is padded at its right and bottom edges and the 1280x720
// launcher background is cut into 512 px tiles.
//
//   bun apps/tab5-factory/tools/convert-assets.ts <M5Tab5-UserDemo>/app/assets/images
//
// Source: m5stack/M5Tab5-UserDemo app/assets/images/*.c (MIT).
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { encodePng, type Picture } from "./png.ts";

const source = process.argv[2];
if (!source) throw new Error("usage: convert-assets.ts <images directory>");
const out = join(import.meta.dir, "..", "assets");

/** An LVGL 9 RGB565 or RGB565A8 image descriptor and its pixel array. */
function readLvglImage(name: string): Picture {
  const text = readFileSync(join(source, `${name}.c`), "utf8");
  const field = (key: string): string => {
    const match = text.match(new RegExp(`\\.header\\.${key}\\s*=\\s*([A-Z0-9_]+)`));
    if (!match) throw new Error(`${name}: no header.${key}`);
    return match[1];
  };
  const width = Number(field("w"));
  const height = Number(field("h"));
  const format = field("cf");
  const body = text.slice(text.indexOf("{", text.indexOf("_map[]")) + 1, text.indexOf("};"));
  const bytes = Uint8Array.from(body.match(/0x[0-9a-fA-F]{2}/g)!.map(byte => parseInt(byte, 16)));
  const pixels = width * height;
  const alpha = format === "LV_COLOR_FORMAT_RGB565A8";
  if (format !== "LV_COLOR_FORMAT_RGB565" && !alpha) throw new Error(`${name}: unsupported ${format}`);
  if (bytes.length < pixels * (alpha ? 3 : 2)) throw new Error(`${name}: short pixel array`);
  const rgba = new Uint8Array(pixels * 4);
  for (let i = 0; i < pixels; i++) {
    const value = bytes[i * 2] | (bytes[i * 2 + 1] << 8);
    const r = (value >> 11) & 0x1f;
    const g = (value >> 5) & 0x3f;
    const b = value & 0x1f;
    rgba[i * 4] = (r << 3) | (r >> 2);
    rgba[i * 4 + 1] = (g << 2) | (g >> 4);
    rgba[i * 4 + 2] = (b << 3) | (b >> 2);
    rgba[i * 4 + 3] = alpha ? bytes[pixels * 2 + i] : 255;
  }
  return { width, height, rgba };
}

/** A `width` x `height` window of `picture` at (x, y); pixels outside it
 *  repeat the nearest edge pixel when `extend`, else stay transparent. */
function crop(picture: Picture, x: number, y: number, width: number, height: number, extend: boolean): Picture {
  const rgba = new Uint8Array(width * height * 4);
  for (let row = 0; row < height; row++) {
    for (let column = 0; column < width; column++) {
      let sx = x + column;
      let sy = y + row;
      const inside = sx < picture.width && sy < picture.height;
      if (!inside && !extend) continue;
      sx = Math.min(sx, picture.width - 1);
      sy = Math.min(sy, picture.height - 1);
      const from = (sy * picture.width + sx) * 4;
      rgba.set(picture.rgba.subarray(from, from + 4), (row * width + column) * 4);
    }
  }
  return { width, height, rgba };
}

/** Half size: each output pixel averages a 2x2 block (alpha-weighted). */
function half(picture: Picture): Picture {
  const width = picture.width / 2;
  const height = picture.height / 2;
  const rgba = new Uint8Array(width * height * 4);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      let r = 0, g = 0, b = 0, a = 0;
      for (const [dx, dy] of [[0, 0], [1, 0], [0, 1], [1, 1]]) {
        const i = ((y * 2 + dy) * picture.width + x * 2 + dx) * 4;
        const weight = picture.rgba[i + 3];
        r += picture.rgba[i] * weight;
        g += picture.rgba[i + 1] * weight;
        b += picture.rgba[i + 2] * weight;
        a += weight;
      }
      const o = (y * width + x) * 4;
      if (a > 0) {
        rgba[o] = Math.round(r / a);
        rgba[o + 1] = Math.round(g / a);
        rgba[o + 2] = Math.round(b / a);
      }
      rgba[o + 3] = Math.round(a / 4);
    }
  }
  return { width, height, rgba };
}

const pow2 = (value: number): number => 2 ** Math.ceil(Math.log2(value));

function write(name: string, picture: Picture): void {
  writeFileSync(join(out, `${name}@2x.png`), encodePng(picture));
  writeFileSync(join(out, `${name}.png`), encodePng(half(picture)));
  console.log(`${name}: ${picture.width}x${picture.height} @2x`);
}

/** One padded texture per picture. */
for (const [file, name, extend] of [
  ["logo_tab", "logo-tab", false],
  ["logo_5", "logo-5", false],
  ["sw_chg_on", "sw-chg-on", false],
  ["sw_chg_off", "sw-chg-off", false],
  ["sw_qc_on", "sw-qc-on", false],
  ["sw_qc_off", "sw-qc-off", false],
  ["sw_on", "sw-on", false],
  ["sw_off", "sw-off", false],
  ["sw_rf_h", "sw-rf-h", false],
  ["sw_rf_l", "sw-rf-l", false],
  ["arrow_state_on", "arrow-state-on", false],
  ["chg_arrow_up", "chg-arrow-up", false],
  ["chg_arrow_down", "chg-arrow-down", false],
  ["porta_i2c_ext5v_on", "porta-i2c-ext5v-on", false],
  ["internal_i2c_dev_chart", "internal-i2c-dev-chart", true],
  ["porta_i2c_dev_chart", "porta-i2c-dev-chart", true],
] as const) {
  const picture = readLvglImage(file);
  write(name, crop(picture, 0, 0, Math.max(4, pow2(picture.width)), Math.max(4, pow2(picture.height)), extend));
}

/** The launcher background in 512 px tiles: columns at 0, 512, 1024 and rows
 *  at 0, 512; the last row repeats the bottom edge below 720. */
const background = readLvglImage("launcher_bg");
for (const [row, y, height] of [[0, 0, 512], [1, 512, 256]] as const) {
  for (const [column, x, width] of [[0, 0, 512], [1, 512, 512], [2, 1024, 256]] as const) {
    write(`launcher-bg-${row}-${column}`, crop(background, x, y, width, height, true));
  }
}
