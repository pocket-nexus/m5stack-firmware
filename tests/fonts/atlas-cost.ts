// Atlas bytes per font slot, for budgeting (docs/FONTS.md "Budgets").
//
//   bun tests/fonts/atlas-cost.ts [--regular=F.ttf] [--bold=F.ttf] [--mono=F.ttf]
//                                 [--chars=STRING] [--density=1,2]
//
// Bakes every slot of the slot table with framework/compiler/bake-font.ts
// from the given faces (default: the upstream Inter / JetBrains Mono) and the
// charset every build bakes (ASCII 32..126) plus --chars, and prints one
// Markdown row per slot: glyphs, logical cell, bytes per raster density and
// the bytes one more glyph adds. A build bakes only the slots its classes use
// (always slot 2, the 16 px default) with the characters its sources contain.
import { resolve } from "node:path";
import { MAX_FONT_SLOTS } from "../../third_party/pocketjs/contracts/spec/spec.ts";
import { bakeAtlases, type BakedAtlas } from "../../third_party/pocketjs/framework/compiler/bake-font.ts";
import { fontSlotInfo } from "../../third_party/pocketjs/framework/compiler/tailwind.ts";

const value = (name: string) => process.argv.find(arg => arg.startsWith(`--${name}=`))?.slice(name.length + 3);
const path = (name: string) => {
  const given = value(name);
  return given === undefined ? undefined : resolve(given);
};
const faces = { regularTtf: path("regular"), boldTtf: path("bold"), monoTtf: path("mono") };
const densities = (value("density") ?? "1,2").split(",").map(Number);
const extraChars = value("chars") ?? "";

const slots: number[] = [];
for (let slot = 0; slot < MAX_FONT_SLOTS; slot++) {
  try {
    fontSlotInfo(slot);
    slots.push(slot);
  } catch {
    // unassigned slot
  }
}
const baked = new Map<number, BakedAtlas[]>();
for (const density of densities) {
  baked.set(density, await bakeAtlases({ slots, codepoints: [], extraChars, rasterDensity: density, ...faces }));
}

const name = (file: string | undefined, fallback: string) => file?.split("/").pop() ?? fallback;
console.log(`regular ${name(faces.regularTtf, "Inter-Regular.ttf")}, bold ${name(faces.boldTtf, "Inter-Bold.ttf")}, ` +
  `mono ${name(faces.monoTtf, "JetBrainsMono-Regular.ttf")}; ASCII${extraChars ? ` + ${[...extraChars].length} chars` : ""}\n`);
console.log(`| Slot | Face | px | Glyphs | Cell (logical) | ${densities.map(d => `Bytes @${d}x | +1 glyph @${d}x`).join(" | ")} |`);
console.log(`| --- | --- | --- | --- | --- | ${densities.map(() => "--- | ---").join(" | ")} |`);
const totals = densities.map(() => 0);
for (const [index, slot] of slots.entries()) {
  const info = fontSlotInfo(slot);
  const first = baked.get(densities[0]!)![index]!;
  const cells = densities.map((density, i) => {
    const atlas = baked.get(density)![index]!;
    totals[i]! += atlas.bytes.length;
    return `${atlas.bytes.length} | ${atlas.coverageW * atlas.coverageH + 8}`;
  });
  const face = info.mono ? "mono" : info.bold ? "bold" : "regular";
  console.log(`| ${slot} | ${face} | ${info.px} | ${first.glyphCount} | ${first.cellW}×${first.cellH} | ${cells.join(" | ")} |`);
}
console.log(`| all | | | | | ${totals.map(total => `${total} |`).join(" | ")} |`);
