// Host-side check of per-app fonts (docs/FONTS.md). No firmware, no device.
//
//   bun tests/fonts/check.ts
//
// 1. Compiles tests/fonts/app (which has a pocket.fonts.json) with tools/pocket
//    and with the submodule CLI, each as a JS bundle and as AOT sources.
// 2. Reads every baked atlas (the bundle's pak in JS mode, font-<slot>.bin in
//    AOT mode), takes the codepoints from its cmap and bakes the slot again
//    with framework/compiler/bake-font.ts: tools/pocket's atlases must equal
//    the bake from the declared faces and differ from the bake from the
//    upstream defaults; the submodule CLI's atlases must equal the defaults.
// 3. Checks that both depfiles list pocket.fonts.json and the declared faces.
// 4. Compiles tests/aot-smoke/app (no pocket.fonts.json) both ways and checks
//    that tools/pocket bakes the same atlases as the submodule CLI.
//
// Outputs go to .pocket-build/fonts-test/ (ignored). Needs
// third_party/pocketjs/platforms/web/pocketjs.wasm for the AOT compiles
// (`bun tools/wasm.ts` in the submodule).
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { BUNDLE_SECTION, decodeBundle } from "../../third_party/pocketjs/contracts/spec/bundle.ts";
import { FONT_CMAP_ENTRY_SIZE, FONT_HEADER_SIZE, FONT_MAGIC } from "../../third_party/pocketjs/contracts/spec/spec.ts";
import { bakeAtlases } from "../../third_party/pocketjs/framework/compiler/bake-font.ts";
import { keyFont, unpack } from "../../third_party/pocketjs/framework/compiler/pak.ts";
import { readFontDeclaration } from "../../tools/fonts/declaration.ts";

const root = resolve(import.meta.dir, "../..");
const out = join(root, ".pocket-build/fonts-test");
const wrapper = join(root, "tools/pocket");
const upstream = join(root, "third_party/pocketjs/tools/pocket.ts");
const wasm = join(root, "third_party/pocketjs/platforms/web/pocketjs.wasm");
const fontsApp = join(import.meta.dir, "app");
const smokeApp = join(root, "tests/aot-smoke/app");

let failures = 0;
function check(ok: boolean, message: string): void {
  console.log(`${ok ? "ok  " : "FAIL"} ${message}`);
  if (!ok) failures++;
}

if (!existsSync(wasm)) {
  console.error(`missing ${wasm}; run \`bun tools/wasm.ts\` in third_party/pocketjs first`);
  process.exit(1);
}
rmSync(out, { recursive: true, force: true });

type Cli = "wrapper" | "upstream";
type Mode = "js" | "aot";
interface Compiled {
  readonly atlases: Map<number, Uint8Array>;
  readonly depfile: string;
}

/** `pocket compile` of the app whose host config is `<app>/pocket.host.config.ts`. */
function compile(app: string, name: string, cli: Cli, mode: Mode): Compiled {
  const dir = join(out, name, `${mode}-${cli}`);
  const depfile = `${dir}.d`;
  const config = join(app, "pocket.host.config.ts");
  const common = ["compile", "--config", config, "--plan-dir", join(dir, "plans"), "--depfile", depfile];
  const modeArgs = mode === "aot"
    ? ["--aot", "--out", join(dir, "aot")]
    : ["--outdir", join(dir, "compiler"), "--output", join(dir, "app.pkb")];
  const command = cli === "wrapper" ? [wrapper, ...common, ...modeArgs] : [process.execPath, upstream, ...common, ...modeArgs];
  // AOT compiles create <project-root>/dist (ignored); remove it if this run made it.
  const dist = join(app, "dist"), hadDist = existsSync(dist);
  const result = spawnSync(command[0]!, command.slice(1), { cwd: root, encoding: "utf8" });
  if (!hadDist) rmSync(dist, { recursive: true, force: true });
  if (result.status !== 0) {
    process.stderr.write(result.stdout + result.stderr);
    throw new Error(`${cli} ${mode} compile of ${name} failed (${result.status})`);
  }
  const atlases = new Map<number, Uint8Array>();
  if (mode === "aot") {
    for (const file of readdirSync(join(dir, "aot"))) {
      const match = /^font-(\d+)\.bin$/.exec(file);
      if (match) atlases.set(Number(match[1]), new Uint8Array(readFileSync(join(dir, "aot", file))));
    }
  } else {
    // The pak section of the bundle the build embeds.
    const bundle = decodeBundle(new Uint8Array(readFileSync(join(dir, "app.pkb"))));
    const pak = bundle.sections.find(section => section.kind === BUNDLE_SECTION.pak);
    if (!pak) throw new Error(`${name}: the bundle has no pak section`);
    const blobs = unpack(pak.bytes);
    for (let slot = 0; slot < 24; slot++) {
      const blob = blobs.find(item => item.key === keyFont(slot));
      if (blob) atlases.set(slot, blob.data);
    }
  }
  return { atlases, depfile };
}

/** Slot, density and mapped codepoints of an atlas (contracts/spec/spec.ts, FONT ATLAS v3). */
function readAtlas(bytes: Uint8Array): { slot: number; density: number; codepoints: number[] } {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint32(0, true) !== FONT_MAGIC) throw new Error("not a font atlas");
  const glyphs = view.getUint16(6, true);
  const codepoints: number[] = [];
  for (let i = 0; i < glyphs; i++) {
    const entry = FONT_HEADER_SIZE + i * FONT_CMAP_ENTRY_SIZE;
    if (view.getUint16(entry + 4, true) !== 0) codepoints.push(view.getUint32(entry, true));
  }
  return { slot: bytes[12]!, density: bytes[14]!, codepoints };
}

type Faces = { regular?: string; bold?: string; mono?: string };
async function rebake(atlas: Uint8Array, faces: Faces): Promise<Uint8Array> {
  const { slot, density, codepoints } = readAtlas(atlas);
  const [baked] = await bakeAtlases({ slots: [slot], codepoints, rasterDensity: density,
    regularTtf: faces.regular, boldTtf: faces.bold, monoTtf: faces.mono });
  return baked!.bytes;
}

const equal = (a: Uint8Array, b: Uint8Array) => a.length === b.length && a.every((value, i) => value === b[i]);
const slotsOf = (compiled: Compiled) => [...compiled.atlases.keys()].sort((a, b) => a - b).join(",");

// --- the declaring app --------------------------------------------------------
const declaration = readFontDeclaration(join(fontsApp, "pocket.host.config.ts"));
if (!declaration) throw new Error("tests/fonts/app has no pocket.fonts.json");
const faces = declaration.faces;
console.log(`declared: ${Object.entries(faces).map(([role, path]) => `${role}=${path.slice(root.length + 1)}`).join(" ")}`);

for (const mode of ["js", "aot"] as const) {
  const mine = compile(fontsApp, "fonts", "wrapper", mode);
  const theirs = compile(fontsApp, "fonts", "upstream", mode);
  check(slotsOf(mine) === "2,12,17", `${mode}: tools/pocket baked slots ${slotsOf(mine)} (16 px regular, 24 px bold, 14 px mono)`);
  check(slotsOf(theirs) === slotsOf(mine), `${mode}: the submodule CLI baked the same slots`);
  for (const [slot, atlas] of mine.atlases) {
    check(equal(atlas, await rebake(atlas, faces)), `${mode}: slot ${slot} atlas equals the bake from the declared face`);
    check(!equal(atlas, await rebake(atlas, {})), `${mode}: slot ${slot} atlas differs from the bake from the default face`);
  }
  for (const [slot, atlas] of theirs.atlases) {
    check(equal(atlas, await rebake(atlas, {})), `${mode}: submodule CLI slot ${slot} atlas equals the default-face bake`);
  }
  const listed = readFileSync(mine.depfile, "utf8");
  for (const path of [declaration.file, ...Object.values(faces)]) {
    check(listed.includes(path), `${mode}: depfile lists ${path.slice(root.length + 1)}`);
  }
}

// --- an app without a declaration ---------------------------------------------
for (const mode of ["js", "aot"] as const) {
  const mine = compile(smokeApp, "aot-smoke", "wrapper", mode);
  const theirs = compile(smokeApp, "aot-smoke", "upstream", mode);
  const same = slotsOf(mine) === slotsOf(theirs) &&
    [...mine.atlases].every(([slot, atlas]) => equal(atlas, theirs.atlases.get(slot)!));
  check(same, `${mode}: aot-smoke (no pocket.fonts.json) atlases equal the submodule CLI's (slots ${slotsOf(mine)})`);
  check(readFileSync(mine.depfile, "utf8").includes(dirname(join(smokeApp, "pocket.host.config.ts"))),
    `${mode}: aot-smoke depfile lists the app directory (a new pocket.fonts.json rebuilds)`);
}

// --- declaration errors -------------------------------------------------------
const scratch = join(out, "declarations");
mkdirSync(scratch, { recursive: true });
const config = join(scratch, "pocket.host.config.ts");
const rejects = (declared: string, expected: RegExp, what: string) => {
  writeFileSync(join(scratch, "pocket.fonts.json"), declared);
  let message = "";
  try {
    readFontDeclaration(config);
  } catch (error) {
    message = (error instanceof Error ? error.message : String(error)).replaceAll(`${scratch}/`, "");
  }
  check(expected.test(message), `declaration with ${what} is rejected (${message || "accepted"})`);
};
rejects(`{ "semibold": "a.ttf" }`, /unknown field semibold/, "an unknown role");
rejects(`{ "regular": "missing.ttf" }`, /file not found/, "a missing file");
rejects(`{ "regular": "font.woff2" }`, /\.ttf or \.otf/, "a .woff2 face");
rejects(`{ "regular": 1 }`, /nonempty path/, "a non-string path");
rmSync(join(scratch, "pocket.fonts.json"));
check(readFontDeclaration(config) === undefined, "no pocket.fonts.json means no declaration (upstream defaults)");

console.log(failures ? `${failures} check(s) failed` : "all checks passed");
process.exit(failures ? 1 : 0);
