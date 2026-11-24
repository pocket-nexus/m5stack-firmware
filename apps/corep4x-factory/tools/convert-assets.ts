// Converts the factory firmware's LVGL screen pictures into the PNG files the
// PocketJS build reads. The CoreP4X host draws at raster density 1, so each
// picture keeps its 480x480 factory pixels. PocketJS textures are
// power-of-two sized and at most 512 px on a side: each picture is padded to
// 512x512 by repeating its right and bottom edges, and the views clip it to
// the 480x480 screen. The info and function pictures (RGB565A8) are blended
// over the LVGL default theme's screen colour 0xF5F5F5, which is what shows
// through their anti-aliased corners on the device.
//
//   bun apps/corep4x-factory/tools/convert-assets.ts \
//     <M5CoreP4X-UserDemo>/components/m5_corep4_component/corep4_lvgl_ui_out/images
//
// Source: m5stack/M5CoreP4X-UserDemo commit
// cd1e1bd0bfd4b6e51a4898999b33a9392ea11f65 (MIT),
// ui_img_corep4x_ui_{startup,info,function}_png.c.
//
// It also writes the pictures the board replaces at run time (the TOUCH
// canvas and the IMU rendering, shown as they look before the board's first
// upload) and two symbols of LVGL 9's lv_font_montserrat_14 (MIT; the
// symbols are Font Awesome 5 Free, SIL OFL 1.1), 4 bits per pixel: the IMU
// window's LV_SYMBOL_REFRESH (U+F021, 14x15, 0x111827) and the checkbox
// mark LV_SYMBOL_OK (U+F00C, 14x11, white).
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { encodePng, type Picture } from "./png.ts";

const source = process.argv[2];
if (!source) throw new Error("usage: convert-assets.ts <images directory>");
const out = join(import.meta.dir, "..", "assets");
const SCREEN: [number, number, number] = [0xf5, 0xf5, 0xf5];

/** An LVGL 9 RGB565 or RGB565A8 image source of 480x480 pixels. */
function readLvglImage(name: string): Picture {
  const text = readFileSync(join(source, `${name}.c`), "utf8");
  const format = text.match(/cf\s*=\s*(LV_COLOR_FORMAT_[A-Z0-9]+)/)?.[1];
  const width = Number(text.match(/\.w\s*=\s*(\d+)/)?.[1]);
  const height = Number(text.match(/\.h\s*=\s*(\d+)/)?.[1]);
  if (width !== 480 || height !== 480) throw new Error(`${name}: ${width}x${height}`);
  const body = text.slice(text.indexOf("{", text.indexOf("_map[]")) + 1);
  const bytes = Uint8Array.from(body.slice(0, body.indexOf("};")).match(/0x[0-9a-fA-F]{2}/g)!.map(byte => parseInt(byte, 16)));
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
    const rgb = [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)];
    const a = alpha ? bytes[pixels * 2 + i] : 255;
    for (let c = 0; c < 3; c++) rgba[i * 4 + c] = Math.round((rgb[c] * a + SCREEN[c] * (255 - a)) / 255);
    rgba[i * 4 + 3] = 255;
  }
  return { width, height, rgba };
}

/** The picture padded to size x size, repeating its last column and row. */
function pad(picture: Picture, size: number): Picture {
  const rgba = new Uint8Array(size * size * 4);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const from = (Math.min(y, picture.height - 1) * picture.width + Math.min(x, picture.width - 1)) * 4;
      rgba.set(picture.rgba.subarray(from, from + 4), (y * size + x) * 4);
    }
  }
  return { width: size, height: size, rgba };
}

/** TOUCH canvas before the first upload: white, the 5x5 grid of 3 px
 *  0xCBD5E1 lines at x, y = 0, 96, 192, 288, 384 and 477. */
function touchCanvas(): Picture {
  const size = 512;
  const rgba = new Uint8Array(size * size * 4).fill(255);
  const grid = (v: number) => v < 480 && (v % 96 < 3 || v >= 477);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      if (grid(x) && y < 480 || grid(y) && x < 480) rgba.set([0xcb, 0xd5, 0xe1, 255], (y * size + x) * 4);
    }
  }
  return { width: size, height: size, rgba };
}

/** IMU rendering before the first upload: the renderer's background, a
 *  vertical gradient from 0xF8FAFC to 0xE0E8EC over the 418-row canvas. */
function imuCanvas(): Picture {
  const size = 512;
  const rgba = new Uint8Array(size * size * 4);
  for (let y = 0; y < size; y++) {
    const t = Math.min(y, 417) / 417;
    const c = [248 + (224 - 248) * t, 250 + (232 - 250) * t, 252 + (236 - 252) * t].map(Math.round);
    for (let x = 0; x < size; x++) rgba.set([c[0], c[1], c[2], 255], (y * size + x) * 4);
  }
  return { width: size, height: size, rgba };
}

// prettier-ignore
const REFRESH = [
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x33, 0x0, 0x1, 0x8d, 0xff, 0xc6, 0x0, 0xef, 0x0, 0x4e,
  0xff, 0xff, 0xff, 0xe4, 0xdf, 0x4, 0xff, 0xb3, 0x0, 0x4c, 0xff, 0xff, 0xe, 0xf9, 0x0, 0x0,
  0x0, 0x8f, 0xff, 0x6f, 0xc0, 0x0, 0x1, 0xff, 0xff, 0xff, 0x8e, 0x50, 0x0, 0x1, 0xde, 0xee,
  0xed, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0, 0x21, 0xff,
  0xff, 0xff, 0x10, 0x0, 0x8, 0xf8, 0xff, 0xfb, 0xbc, 0x10, 0x0, 0x1e, 0xf4, 0xff, 0xfc, 0x10,
  0x0, 0x1, 0xdf, 0xc0, 0xfe, 0xef, 0xe8, 0x44, 0x8e, 0xfe, 0x10, 0xfe, 0x1a, 0xff, 0xff, 0xff,
  0xc1, 0x0, 0xfd, 0x0, 0x28, 0xbb, 0x94, 0x0, 0x0,
];

// prettier-ignore
const CHECK = [
  0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1d, 0xf8, 0x0, 0x0, 0x0, 0x0, 0x1, 0xdf,
  0xfd, 0x0, 0x0, 0x0, 0x0, 0x1d, 0xff, 0xe2, 0x2d, 0x60, 0x0, 0x1, 0xdf, 0xfe, 0x20, 0xdf, 0xf7, 0x0, 0x1d, 0xff,
  0xe2, 0x0, 0x8f, 0xff, 0x71, 0xdf, 0xfe, 0x20, 0x0, 0x8, 0xff, 0xfe, 0xff, 0xe2, 0x0, 0x0, 0x0, 0x8f, 0xff, 0xfe,
  0x20, 0x0, 0x0, 0x0, 0x8, 0xff, 0xe2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7d, 0x20, 0x0, 0x0, 0x0,
];

/** A glyph in a 16x16 texture: alpha from its 4-bit coverage. */
function glyph(bits: number[], width: number, height: number, rgb: number[]): Picture {
  const size = 16;
  const rgba = new Uint8Array(size * size * 4);
  for (let i = 0; i < width * height; i++) {
    const nibble = (bits[i >> 1] >> (i & 1 ? 0 : 4)) & 15;
    rgba.set([rgb[0], rgb[1], rgb[2], nibble * 17], (Math.floor(i / width) * size + (i % width)) * 4);
  }
  return { width: size, height: size, rgba };
}

writeFileSync(join(out, "touch-canvas.png"), encodePng(touchCanvas()));
writeFileSync(join(out, "imu-canvas.png"), encodePng(imuCanvas()));
writeFileSync(join(out, "refresh.png"), encodePng(glyph(REFRESH, 14, 15, [0x11, 0x18, 0x27])));
writeFileSync(join(out, "check.png"), encodePng(glyph(CHECK, 14, 11, [255, 255, 255])));
console.log("touch-canvas.png, imu-canvas.png, refresh.png, check.png");

for (const [file, name] of [
  ["ui_img_corep4x_ui_startup_png", "startup"],
  ["ui_img_corep4x_ui_info_png", "info"],
  ["ui_img_corep4x_ui_function_png", "function"],
] as const) {
  writeFileSync(join(out, `${name}.png`), encodePng(pad(readLvglImage(file), 512)));
  console.log(`${name}.png: 512x512 from 480x480`);
}
