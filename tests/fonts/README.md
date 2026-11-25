# fonts

Host-side checks of per-app font faces (`docs/FONTS.md`). No firmware and no
device are involved.

| Path | Content |
| --- | --- |
| `check.ts` | The check |
| `atlas-cost.ts` | Atlas bytes per slot for given faces at density 1 and 2 |

The check compiles two applications of this repository:
`apps/sticks3-demos`, whose host config declares `app.fonts` (Montserrat
Medium as `regular`, Montserrat SemiBold as `bold`, no `mono`), and
`tests/aot-smoke/app`, which declares none.

## Run

```sh
# once per submodule checkout: the MicroTS layout oracle for AOT compiles
(cd third_party/pocketjs && bun tools/wasm.ts)

bun tests/fonts/check.ts
bun tests/fonts/atlas-cost.ts --regular=F.ttf --bold=F.ttf --mono=F.ttf
```

`check.ts` writes to `.pocket-build/fonts-test/` and:

1. compiles `apps/sticks3-demos` with `tools/pocket` as a JS bundle and with
   `--aot`;
2. reads each atlas (the bundle's pak section, or `font-<slot>.bin`), takes
   the codepoints of its cmap and bakes the slot again with
   `framework/compiler/bake-font.ts`. Every atlas must equal the bake from
   the declared faces, and an atlas of a declared family (regular or bold)
   must differ from the bake from the default face;
3. checks that both depfiles list the two declared faces;
4. compiles `tests/aot-smoke/app` both ways and requires atlases equal to
   the bake from the default faces.

PocketJS validates `app.fonts` itself (unknown fields, missing files, other
extensions, `..` segments); its own tests cover those errors.

## Results

Bun 1.4.2, PocketJS `4f0dabf5`, macOS arm64: `all checks passed` (52 checks,
about 13 s).

These are host-side checks, not device validation.
