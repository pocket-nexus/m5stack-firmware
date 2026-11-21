// The Pocket CLI of the pinned third_party/pocketjs checkout, with per-app
// font faces (docs/FONTS.md). tools/pocket runs this file.
//
// Every command runs the submodule's tools/pocket.ts in this process with the
// same arguments. For `compile` whose host config has a pocket.fonts.json
// beside it, two injection points are installed first:
//
// - JS bundle: compile-app.ts spawns `bun tools/build.ts --plan=...`; the
//   spawn gets build.ts's own `--font-regular=`/`--font-bold=`/`--font-mono=`
//   flags for the declared faces.
// - AOT (`--aot`): aot-compile.ts imports bakeAtlases from
//   framework/compiler/bake-font.ts; a Bun runtime plugin resolves that one
//   import to tools/fonts/bake-font.ts, which passes the declared faces as
//   `regularTtf`/`boldTtf`/`monoTtf`.
//
// After the compile, the wrapper fails when an injection point did not run
// (the submodule changed how it compiles) and adds the declaration, the faces
// and the wrapper's own sources to the --depfile.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { active, readFontDeclaration, type FontDeclaration } from "./fonts/declaration.ts";

const root = resolve(import.meta.dir, "..");
const pocketjs = resolve(root, "third_party/pocketjs");
const upstreamCli = resolve(pocketjs, "tools/pocket.ts");
const upstreamBuild = resolve(pocketjs, "tools/build.ts");
const upstreamAotCompile = resolve(pocketjs, "tools/aot-compile.ts");
const shim = resolve(import.meta.dir, "fonts/bake-font.ts");
const wrapperSources = [import.meta.path, resolve(import.meta.dir, "fonts/declaration.ts"), shim, resolve(root, "tools/pocket")];

const args = Bun.argv.slice(2);

/** An option the way the submodule CLI reads it: `--name value` or `--name=value`. */
function option(name: string): string | undefined {
  for (let i = 0; i < args.length; i++) {
    const arg = args[i]!;
    if (arg.startsWith(`--${name}=`)) return arg.slice(name.length + 3);
    if (arg === `--${name}`) return args[i + 1];
  }
  return undefined;
}

interface Compile {
  readonly aot: boolean;
  readonly configPath: string;
  readonly depfile?: string;
  readonly declaration?: FontDeclaration;
}

async function compileRequest(): Promise<Compile | undefined> {
  if (args[0] !== "compile") return undefined;
  const named = option("config") ?? option("host-config");
  let configPath: string | undefined;
  if (named) configPath = resolve(named);
  else {
    const { findHostConfig } = await import("../third_party/pocketjs/tools/profile-resolver.ts");
    configPath = findHostConfig(process.cwd()) ?? undefined;
  }
  // Without a config the submodule CLI reports the usage error.
  if (!configPath || !existsSync(configPath)) return undefined;
  const projectRoot = resolve(option("project-root") ?? dirname(configPath));
  const depfile = option("depfile");
  return {
    aot: args.includes("--aot"),
    configPath,
    depfile: depfile === undefined ? undefined : resolve(projectRoot, depfile),
    declaration: readFontDeclaration(configPath),
  };
}

function installFaces(declaration: FontDeclaration): void {
  active.faces = { ...declaration.faces };
  Bun.plugin({
    name: "m5stack-sample-app-fonts",
    setup(build) {
      build.onResolve({ filter: /\/framework\/compiler\/bake-font(?:\.ts)?$/ }, request => {
        if (request.importer !== upstreamAotCompile) return undefined;
        return { path: shim };
      });
    },
  });
  const spawn = Bun.spawn;
  const flags = Object.entries(declaration.faces).map(([role, path]) => `--font-${role}=${path}`);
  const withFonts = (command: unknown): unknown => {
    if (!Array.isArray(command) || command[1] !== upstreamBuild) return command;
    active.buildSpawns++;
    return [...command, ...flags];
  };
  (Bun as { spawn: unknown }).spawn = (first: unknown, ...rest: unknown[]) => {
    if (Array.isArray(first)) return (spawn as (...a: unknown[]) => unknown)(withFonts(first), ...rest);
    if (first && typeof first === "object" && "cmd" in first) {
      const options = first as { cmd: unknown };
      return (spawn as (...a: unknown[]) => unknown)({ ...options, cmd: withFonts(options.cmd) }, ...rest);
    }
    return (spawn as (...a: unknown[]) => unknown)(first, ...rest);
  };
}

/** Make's escaping, as framework/compiler/build-inputs.ts `depfile` writes it. */
function escapeDependency(path: string): string {
  if (/[\r\n]/.test(path)) throw new Error("newline in build dependency path");
  return path.replace(/\\/g, "\\\\").replace(/\$/g, () => "$$").replace(/[ #:\t]/g, c => "\\" + c);
}

/** Add prerequisites to the one rule the submodule wrote. */
function extendDepfile(depfile: string, paths: readonly string[]): void {
  const text = readFileSync(depfile, "utf8");
  if (!text.endsWith("\n") || !text.includes(": ")) throw new Error(`${depfile}: unexpected depfile format`);
  const listed = new Set(text.split(/ \\\n {2}|: | \\\n|\n/).filter(Boolean));
  const extra = [...new Set(paths.map(path => resolve(path)))]
    .filter(path => existsSync(path) && !listed.has(escapeDependency(path)));
  if (extra.length === 0) return;
  writeFileSync(depfile, text.slice(0, -1) + extra.map(path => ` \\\n  ${escapeDependency(path)}`).join("") + "\n");
}

const compile = await compileRequest();
if (compile?.declaration) installFaces(compile.declaration);
await import(upstreamCli);

if (compile && process.exitCode === undefined) {
  const { declaration } = compile;
  if (declaration) {
    const ran = compile.aot ? active.bakeCalls : active.buildSpawns;
    if (ran === 0) {
      const point = compile.aot
        ? `tools/aot-compile.ts no longer imports bakeAtlases from ../framework/compiler/bake-font.ts`
        : `tools/compile-app.ts no longer spawns tools/build.ts through Bun.spawn`;
      console.error(`tools/pocket: ${declaration.file} declares fonts, but ${point} in third_party/pocketjs; ` +
        "update tools/pocket.ts (docs/FONTS.md, \"Submodule bumps\")");
      process.exit(1);
    }
  }
  if (compile.depfile && existsSync(compile.depfile)) {
    // Without a declaration the config's directory stands in for it:
    // creating pocket.fonts.json there changes the directory.
    const declared = declaration
      ? [declaration.file, ...Object.values(declaration.faces)]
      : [dirname(compile.configPath)];
    extendDepfile(compile.depfile, [...wrapperSources, ...declared]);
  }
}
