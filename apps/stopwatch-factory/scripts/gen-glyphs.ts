// Converts the factory's LVGL bitmap fonts at sizes outside the PocketJS slot
// table (Maple Mono Medium 28/48 px, Commissioner Medium 64/108 px; OFL 1.1,
// rendered by the factory with lv_font_conv) into glyph images, and writes
// glyphs.ts (advances, for the model's text layout) and the Glyphs*.tsx /
// FlowDigit.tsx views that draw them. Run from the repository root:
//
//   bun apps/stopwatch-factory/scripts/gen-glyphs.ts <M5StopWatch-UserDemo checkout>
//
// Every glyph is cropped to its ink, recoloured to one solid colour per use
// (images take no text colour) and stored as RGBA4444 on the smallest
// power-of-two canvas.

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { deflateSync } from "node:zlib";

const source = process.argv[2];
if (!source) {
  console.error("usage: gen-glyphs.ts <M5StopWatch-UserDemo checkout>");
  process.exit(2);
}
const fontRoot = join(resolve(source), "main/assets/fonts");
const appDir = resolve(import.meta.dir, "..");
const imageDir = join(appDir, "images");
mkdirSync(imageDir, { recursive: true });

interface Glyph {
  cp: number;
  advance: number;
  width: number;
  height: number;
  left: number;
  top: number;
  alpha: Uint8Array;
}

interface LvFont {
  lineHeight: number;
  glyphs: Map<number, Glyph>;
}

function readFont(file: string): LvFont {
  const text = readFileSync(join(fontRoot, file), "utf8");
  const bitmap = Uint8Array.from(
    (text.split("glyph_bitmap[] = {")[1].split("};")[0].match(/0x[0-9a-fA-F]{1,2}\b/g) ?? []).map(h => parseInt(h, 16)),
  );
  const dsc = [
    ...text
      .split("glyph_dsc[] = {")[1]
      .split("};")[0]
      .matchAll(/\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)/g),
  ].map(m => m.slice(1).map(Number));
  const lineHeight = Number(/\.line_height\s*=\s*(\d+)/.exec(text)![1]);
  const baseLine = Number(/\.base_line\s*=\s*(\d+)/.exec(text)![1]);
  const bpp = Number(/\.bpp\s*=\s*(\d+)/.exec(text)![1]);
  if (bpp !== 2) throw new Error(`${file}: bpp ${bpp}`);
  const glyphs = new Map<number, Glyph>();
  const ofsList = /glyph_id_ofs_list_0\[\] = \{([^}]*)\}/.exec(text)?.[1].split(",").map(v => Number(v.trim()));
  for (const m of text.matchAll(/\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),\s*\.glyph_id_start\s*=\s*(\d+),\s*\.unicode_list\s*=\s*NULL,\s*\.glyph_id_ofs_list\s*=\s*(\w+)/g)) {
    const [start, length, first] = m.slice(1, 4).map(Number);
    // FORMAT0_FULL: glyph id = first + ofs[i]; codepoints the font lacks have
    // offset 0 (only the range's first codepoint really maps to it).
    const sparse = m[4] !== "NULL" ? ofsList : undefined;
    for (let i = 0; i < length; i++) {
      if (sparse && i > 0 && sparse[i] === 0) continue;
      const [index, adv, w, h, ox, oy] = dsc[first + (sparse ? sparse[i] : i)];
      const alpha = new Uint8Array(w * h);
      for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
          // Plain bitmaps pack the rows without padding, MSB first.
          const bit = (y * w + x) * 2;
          const v = (bitmap[index + (bit >> 3)] >> (6 - (bit & 7))) & 3;
          alpha[y * w + x] = v * 85;
        }
      }
      glyphs.set(start + i, {
        cp: start + i,
        advance: Math.round(adv / 16),
        width: w,
        height: h,
        left: ox,
        top: lineHeight - baseLine - h - oy,
        alpha,
      });
    }
  }
  return { lineHeight, glyphs };
}

function chunk(type: string, data: Uint8Array): Uint8Array {
  const body = new Uint8Array(4 + data.length);
  body.set(new TextEncoder().encode(type), 0);
  body.set(data, 4);
  const out = new Uint8Array(12 + data.length);
  const view = new DataView(out.buffer);
  view.setUint32(0, data.length);
  out.set(body, 4);
  view.setUint32(8 + data.length, Bun.hash.crc32(body) >>> 0);
  return out;
}

function png(width: number, height: number, rgba: Uint8Array): Uint8Array {
  const header = new Uint8Array(13);
  new DataView(header.buffer).setUint32(0, width);
  new DataView(header.buffer).setUint32(4, height);
  header.set([8, 6, 0, 0, 0], 8);
  const raw = new Uint8Array((width * 4 + 1) * height);
  for (let y = 0; y < height; y++) raw.set(rgba.subarray(y * width * 4, (y + 1) * width * 4), y * (width * 4 + 1) + 1);
  const parts = [new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk("IHDR", header), chunk("IDAT", deflateSync(raw, { level: 9 })), chunk("IEND", new Uint8Array())];
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}

const pow2 = (n: number) => 2 ** Math.max(0, Math.ceil(Math.log2(Math.max(1, n))));
const meta: Record<string, { psm: number }> = JSON.parse(readFileSync(join(appDir, "images.json"), "utf8"));
for (const key of Object.keys(meta)) if (key.startsWith("images/gl_")) delete meta[key];
let imageBytes = 0;
const counted = new Set<string>();

interface Tile {
  file: string;
  width: number;
  height: number;
  /** Row of the glyph where this tile starts. */
  dy: number;
}

/** Writes one glyph as one or two RGBA4444 tiles: a glyph taller than 64 px
 *  is split into a 64-row tile and a tile for the rest, which costs less than
 *  one 128-row canvas (Commissioner 108: 10 KB instead of 16 KB per glyph). */
function glyphTiles(font: string, glyph: Glyph, color: number, colorKey: string): Tile[] {
  const bands: [number, number][] =
    glyph.height > 64 && glyph.height <= 96 ? [[0, 64], [64, glyph.height - 64]] : [[0, glyph.height]];
  return bands.map(([dy, rows], index) => {
    const w = pow2(glyph.width);
    const h = pow2(rows);
    const rgba = new Uint8Array(w * h * 4);
    for (let y = 0; y < rows; y++) {
      for (let x = 0; x < glyph.width; x++) {
        const o = (y * w + x) * 4;
        rgba[o] = (color >> 16) & 255;
        rgba[o + 1] = (color >> 8) & 255;
        rgba[o + 2] = color & 255;
        rgba[o + 3] = glyph.alpha[(dy + y) * glyph.width + x];
      }
    }
    const name = `gl_${font}_${colorKey}_${glyph.cp.toString(16)}${bands.length > 1 ? `_${index}` : ""}`;
    writeFileSync(join(imageDir, `${name}.png`), png(w, h, rgba));
    meta[`images/${name}.png`] = { psm: 2 };
    if (!counted.has(name)) imageBytes += w * h * 2;
    counted.add(name);
    return { file: `images/${name}.png`, width: w, height: h, dy };
  });
}

function tileImages(glyph: Glyph, tiles: Tile[], left: number, top: number): string {
  return tiles
    .map(t => `<Image class="absolute left-[${left}] top-[${top + t.dy}] w-[${t.width}] h-[${t.height}]" src="${t.file}" />`)
    .join("");
}

interface Run {
  /** Component name. */
  name: string;
  font: string;
  lv: LvFont;
  chars: string;
  color: number;
  colorKey: string;
  slots: number;
}

const fonts = {
  m28: readFont("lv_font_maple_mono_medium_28.c"),
  m48: readFont("lv_font_maple_mono_medium_48.c"),
  c64: readFont("CommissionerMedium64.c"),
  c108: readFont("CommissionerMedium108.c"),
};

// Theme time colours of the number-flow and simple faces (factory tables).
const THEME_TIME = [0xffffff, 0x484848, 0x566e6c, 0x6a5e4d, 0x4b5e20, 0xdfdeff, 0xffefe6, 0x83627e, 0xb1fffa, 0xecf8ff];

const M28 = "0123456789O:.-LAP";
const runs: Run[] = [
  { name: "GlyphsClockWhite", font: "m28", lv: fonts.m28, chars: M28, color: 0xffffff, colorKey: "w", slots: 1 },
  { name: "GlyphsClockBlue", font: "m28", lv: fonts.m28, chars: M28, color: 0xcdebf9, colorKey: "c", slots: 1 },
  { name: "GlyphsClassicTime", font: "m28", lv: fonts.m28, chars: M28, color: 0xd9d9d9, colorKey: "d", slots: 5 },
  { name: "GlyphsLap", font: "m28", lv: fonts.m28, chars: M28, color: 0xd8f2ff, colorKey: "l", slots: 11 },
  { name: "GlyphsLapEmpty", font: "m28", lv: fonts.m28, chars: M28, color: 0x738086, colorKey: "e", slots: 3 },
  { name: "GlyphsElapsed", font: "m48", lv: fonts.m48, chars: "O123456789:.", color: 0xd8f2ff, colorKey: "l", slots: 11 },
  ...THEME_TIME.map((color, theme) => ({
    name: `GlyphsLarge${theme}`,
    font: "c108",
    lv: fonts.c108,
    chars: theme === 0 ? "0123456789:(" : "0123456789:",
    color,
    colorKey: `t${theme}`,
    slots: 5,
  })),
];

const generated = "// Generated by scripts/gen-glyphs.ts; do not edit.\n";
const written = new Set<string>();
for (const run of runs) {
  const shows: string[] = [];
  const list = [...run.chars].map(c => run.lv.glyphs.get(c.codePointAt(0)!)!);
  list.forEach(glyph => {
    const key = `${run.font}_${run.colorKey}_${glyph.cp}`;
    const tiles = glyphTiles(run.font, glyph, run.color, run.colorKey);
    written.add(key);
    shows.push(
      `          <Show when={(props.ids[props.base + SLOT] ?? -1) === ${glyph.cp}}>${tileImages(glyph, tiles, glyph.left, glyph.top)}</Show>`,
    );
  });
  const slots: string[] = [];
  for (let s = 0; s < run.slots; s++) {
    slots.push(
      `      <View class="absolute left-0 top-0 w-[${pow2(run.lv.lineHeight)}] h-[${run.lv.lineHeight}]" style={{ translateX: props.xs[${s}] ?? 0 }}>\n` +
        shows.map(line => line.replace("SLOT", String(s))).join("\n") +
        "\n      </View>",
    );
  }
  writeFileSync(
    join(appDir, `${run.name}.tsx`),
    `${generated}import { Show } from "solid-js";
import { Image, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// Factory ${run.font === "m28" ? "Maple Mono Medium 28" : run.font === "m48" ? "Maple Mono Medium 48" : "Commissioner Medium 108"} px glyphs in #${run.color.toString(16).padStart(6, "0").toUpperCase()}: slot k draws
// codepoint ids[base + k] with its pen at xs[k] on a ${run.lv.lineHeight} px line (glyphs.ts lays text out).
export default function ${run.name}(props: { ids: i32[]; base: i32; xs: i32[] }) {
  return (
    <View class="absolute left-0 top-0 w-[466] h-[${run.lv.lineHeight}]">
${slots.join("\n")}
    </View>
  );
}
`,
  );
}

// Number-flow digit strips: 9,0,1,...,9,0 on 66 px rows (the DigitFlow line
// height of the factory's Commissioner 64 is 60 px; the strip keeps the
// 66 px pitch of the PocketJS layout) in each theme's time colour.
const flowRows = [9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0];
const matches = THEME_TIME.map((color, theme) => {
  const rows = flowRows.map((digit, row) => {
    const glyph = fonts.c64.glyphs.get(48 + digit)!;
    const key = `c64_t${theme}_${glyph.cp}`;
    const tiles = glyphTiles("c64", glyph, color, `t${theme}`);
    written.add(key);
    // Centre the digit's advance box in the 50 px window, its 60 px line in the 66 px row.
    const left = Math.round((50 - glyph.advance) / 2) + glyph.left;
    const top = row * 66 + 3 + glyph.top;
    return `          ${tileImages(glyph, tiles, left, top)}`;
  });
  return `        <Match when={props.theme === ${theme}}>\n${rows.join("\n")}\n        </Match>`;
});
writeFileSync(
  join(appDir, "DigitStrip.tsx"),
  `${generated}import { Match, Switch } from "solid-js";
import { Image, View } from "@pocketjs/framework/solid/components";
import type { i32 } from "@pocketjs/framework/solid/std";

// One number-flow digit (smooth_ui_toolkit DigitFlow) in Commissioner Medium
// 64 px: the strip 9,0,1,...,9,0 of 66 px rows behind an upright 66 px
// window, moved by the spring offset of the root model, in the theme's time
// colour.
export default function DigitStrip(props: { left: i32; dx: i32; dy: i32; offset: i32; theme: i32 }) {
  return (
    <View class="absolute left-0 top-[33] w-[50] h-[66] overflow-hidden" style={{ translateX: props.left + props.dx, translateY: props.dy }}>
      <View class="absolute left-0 top-0 w-[50] h-[792]" style={{ translateY: -props.offset }}>
      <Switch>
${matches.join("\n")}
      </Switch>
      </View>
    </View>
  );
}
`,
);

// Advances for the model's layout (pure module).
const table = (lv: LvFont) => {
  const cps = [...lv.glyphs.keys()].sort((a, b) => a - b);
  return { cps, adv: cps.map(cp => lv.glyphs.get(cp)!.advance) };
};
const out: string[] = [
  `${generated}// Advances of the factory glyph images (scripts/gen-glyphs.ts).`,
  'import { len, type i32 } from "@pocketjs/framework/solid/std";',
  "",
];
for (const [key, lv] of Object.entries(fonts)) {
  const { cps, adv } = table(lv);
  const upper = key.toUpperCase();
  out.push(`export const ${upper}_CODES: i32[] = [${cps.join(", ")}];`);
  out.push(`export const ${upper}_ADVANCE: i32[] = [${adv.join(", ")}];`);
  out.push(`export const ${upper}_LINE: i32 = ${lv.lineHeight};`);
}
out.push(`
/** Advance of codepoint cp in a font's tables, 0 when the font lacks it. */
export function advance(codes: i32[], advances: i32[], cp: i32): i32 {
  for (let i = 0; i < len(codes); i++) {
    if (codes[i] === cp) {
      return advances[i];
    }
  }
  return 0;
}
`);
writeFileSync(join(appDir, "glyphs.ts"), out.join("\n"));
writeFileSync(join(appDir, "images.json"), `${JSON.stringify(meta, null, 2)}\n`);
console.log(`glyph images: ${written.size}, ${imageBytes} bytes as RGBA4444`);
