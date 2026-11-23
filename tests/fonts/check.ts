// Host-side check of per-app fonts (docs/FONTS.md). No firmware, no device.
//
//   bun tests/fonts/check.ts
//
// 1. Compiles apps/sticks3-demos, whose host config declares `app.fonts`
//    (Montserrat Medium and SemiBold), with tools/pocket as a JS bundle and
//    as AOT sources.
// 2. Reads every baked atlas (the bundle's pak in JS mode, font-<slot>.bin in
//    AOT mode), takes the codepoints from its cmap and bakes the slot again
//    with framework/compiler/bake-font.ts: each atlas must equal the bake
//    from the declared faces, and an atlas of a declared family must differ
//    from the bake from the PocketJS defaults.
// 3. Checks that both depfiles list the declared faces.
// 4. Compiles tests/aot-smoke/app (no `app.fonts`) both ways and checks that
//    its atlases equal the bake from the defaults.
//
// Outputs go to .pocket-build/fonts-test/ (ignored). Needs
// third_party/pocketjs/platforms/web/pocketjs.wasm for the AOT compiles
// (`bun tools/wasm.ts` in the submodule).
import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { BUNDLE_SECTION, decodeBundle } from "../../third_party/pocketjs/contracts/spec/bundle.ts";
import { FONT_CMAP_ENTRY_SIZE, FONT_HEADER_SIZE, FONT_MAGIC } from "../../third_party/pocketjs/contracts/spec/spec.ts";
import { bakeAtlases } from "../../third_party/pocketjs/framework/compiler/bake-font.ts";
import { keyFont, unpack } from "../../third_party/pocketjs/framework/compiler/pak.ts";
import { fontSlotInfo } from "../../third_party/pocketjs/framework/compiler/tailwind.ts";

const root = resolve(import.meta.dir, "../..");
const out = join(root, ".pocket-build/fonts-test");
const cli = join(root, "tools/pocket");
const wasm = join(root, "third_party/pocketjs/platforms/web/pocketjs.wasm");
const fontsApp = join(root, "apps/sticks3-demos");
const smokeApp = join(root, "tests/aot-smoke/app");
/** The `app.fonts` of apps/sticks3-demos/pocket.host.config.ts. */
const declared = {
  regular: join(fontsApp, "fonts/Montserrat-Medium.ttf"),
  bold: join(fontsApp, "fonts/Montserrat-SemiBold.ttf"),
};

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

type Mode = "js" | "aot";
interface Compiled {
  readonly atlases: Map<number, Uint8Array>;
  readonly depfile: string;
}

/** `pocket compile` of the app whose host config is `<app>/pocket.host.config.ts`. */
function compile(app: string, name: string, mode: Mode): Compiled {
  const dir = join(out, name, mode);
  const depfile = `${dir}.d`;
  const config = join(app, "pocket.host.config.ts");
  const common = ["compile", "--config", config, "--plan-dir", join(dir, "plans"), "--depfile", depfile];
  const modeArgs = mode === "aot"
    ? ["--aot", "--out", join(dir, "aot")]
    : ["--outdir", join(dir, "compiler"), "--output", join(dir, "app.pkb")];
  const command = [cli, ...common, ...modeArgs];
  // AOT compiles create <project-root>/dist (ignored); remove it if this run made it.
  const dist = join(app, "dist"), hadDist = existsSync(dist);
  const result = spawnSync(command[0]!, command.slice(1), { cwd: root, encoding: "utf8" });
  if (!hadDist) rmSync(dist, { recursive: true, force: true });
  if (result.status !== 0) {
    process.stderr.write(result.stdout + result.stderr);
    throw new Error(`${mode} compile of ${name} failed (${result.status})`);
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

// --- an app that declares app.fonts --------------------------------------------
console.log(`declared: ${Object.entries(declared).map(([role, path]) => `${role}=${path.slice(root.length + 1)}`).join(" ")}`);
for (const mode of ["js", "aot"] as const) {
  const compiled = compile(fontsApp, "sticks3-demos", mode);
  check(compiled.atlases.size > 0, `${mode}: sticks3-demos baked slots ${slotsOf(compiled)}`);
  for (const [slot, atlas] of compiled.atlases) {
    check(equal(atlas, await rebake(atlas, declared)), `${mode}: slot ${slot} atlas equals the bake from the declared faces`);
    // The app declares no mono face: a mono slot keeps the default.
    const info = fontSlotInfo(slot);
    const family = info.mono ? "mono" : info.bold ? "bold" : "regular";
    if (family !== "mono") {
      check(!equal(atlas, await rebake(atlas, {})), `${mode}: slot ${slot} (${family}) atlas differs from the bake from the default face`);
    }
  }
  const listed = readFileSync(compiled.depfile, "utf8");
  for (const path of Object.values(declared)) {
    check(listed.includes(path), `${mode}: depfile lists ${path.slice(root.length + 1)}`);
  }
}

// --- an app without app.fonts ----------------------------------------------------
for (const mode of ["js", "aot"] as const) {
  const compiled = compile(smokeApp, "aot-smoke", mode);
  let same = compiled.atlases.size > 0;
  for (const [, atlas] of compiled.atlases) same &&= equal(atlas, await rebake(atlas, {}));
  check(same, `${mode}: aot-smoke (no app.fonts) atlases equal the bake from the default faces (slots ${slotsOf(compiled)})`);
}

console.log(failures ? `${failures} check(s) failed` : "all checks passed");
process.exit(failures ? 1 : 0);
