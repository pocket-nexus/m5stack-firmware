// The per-app font declaration, `pocket.fonts.json` beside the app's
// pocket.host.config.ts (docs/FONTS.md):
//
//   { "regular": "fonts/Montserrat-Medium.ttf",
//     "bold": "fonts/Montserrat-SemiBold.ttf",
//     "mono": "fonts/MapleMono-Medium.ttf" }
//
// Each face is optional; an absent face, or an absent file, keeps the
// upstream default (Inter Regular, Inter Bold, JetBrains Mono Regular).
// Paths are relative to the declaration file.
import { existsSync, readFileSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

export const FONT_DECLARATION_FILE = "pocket.fonts.json";
export const FONT_ROLES = ["regular", "bold", "mono"] as const;
export type FontRole = (typeof FONT_ROLES)[number];

export interface FontDeclaration {
  /** Absolute path of the pocket.fonts.json the faces come from. */
  readonly file: string;
  /** Absolute TrueType/OpenType paths, by role. */
  readonly faces: Readonly<Partial<Record<FontRole, string>>>;
}

/** The declaration beside a host config, or undefined when there is none. */
export function readFontDeclaration(configPath: string): FontDeclaration | undefined {
  const file = join(dirname(resolve(configPath)), FONT_DECLARATION_FILE);
  if (!existsSync(file)) return undefined;
  const fail = (message: string): never => {
    throw new Error(`${file}: ${message}`);
  };
  let value: unknown;
  try {
    value = JSON.parse(readFileSync(file, "utf8"));
  } catch (error) {
    fail(`invalid JSON: ${error instanceof Error ? error.message : String(error)}`);
  }
  if (!value || typeof value !== "object" || Array.isArray(value)) fail("expected an object");
  const record = value as Record<string, unknown>;
  for (const key of Object.keys(record)) {
    if (!(FONT_ROLES as readonly string[]).includes(key)) fail(`unknown field ${key}; expected ${FONT_ROLES.join(", ")}`);
  }
  const faces: Partial<Record<FontRole, string>> = {};
  for (const role of FONT_ROLES) {
    const name = record[role];
    if (name === undefined) continue;
    if (typeof name !== "string" || !name) fail(`${role} must be a nonempty path`);
    if (!/\.(?:ttf|otf)$/i.test(name as string)) fail(`${role} must name a .ttf or .otf file`);
    const path = resolve(dirname(file), name as string);
    if (!existsSync(path) || !statSync(path).isFile()) fail(`${role}: file not found: ${path}`);
    faces[role] = path;
  }
  return { file, faces };
}

/** The faces of the declaration the current compile uses, and whether each
 *  injection point ran (tools/pocket.ts checks both after the compile). */
export const active: {
  faces: Partial<Record<FontRole, string>>;
  bakeCalls: number;
  buildSpawns: number;
} = { faces: {}, bakeCalls: 0, buildSpawns: 0 };
