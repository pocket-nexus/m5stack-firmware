// Stand-in for third_party/pocketjs/framework/compiler/bake-font.ts as seen
// by tools/aot-compile.ts of the submodule. tools/pocket.ts redirects that one
// import here (a Bun runtime plugin) when the app declares its faces. The
// module re-exports the upstream module unchanged and fills the faces
// `bakeAtlases` leaves at their defaults (`regularTtf`, `boldTtf`, `monoTtf`)
// from the app's pocket.fonts.json. Font reads still go through the caller's
// `onRead`, so the faces land in the AOT depfile.
import {
  bakeAtlases as upstreamBakeAtlases,
  type BakedAtlas,
  type BakeOptions,
} from "../../third_party/pocketjs/framework/compiler/bake-font.ts";
import { active } from "./declaration.ts";

export * from "../../third_party/pocketjs/framework/compiler/bake-font.ts";

export async function bakeAtlases(options: BakeOptions): Promise<BakedAtlas[]> {
  active.bakeCalls++;
  return upstreamBakeAtlases({
    ...options,
    regularTtf: options.regularTtf ?? active.faces.regular,
    boldTtf: options.boldTtf ?? active.faces.bold,
    monoTtf: options.monoTtf ?? active.faces.mono,
  });
}
