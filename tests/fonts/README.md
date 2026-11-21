# fonts

Host-side checks of per-app font faces (`docs/FONTS.md`). No firmware and no
device are involved.

| Path | Content |
| --- | --- |
| `app/` | Application with a `pocket.fonts.json`: 16 px regular, 24 px bold and 14 px mono text, ASCII only |
| `app/pocket.fonts.json` | Declares faces the submodule ships: InterDisplay Regular, InterDisplay Bold, W95FA (mono) |
| `pocket.host.json` | Host contract: esp-idf, 240×135, raster density 1 |
| `check.ts` | The check |
| `atlas-cost.ts` | Atlas bytes per slot for given faces at density 1 and 2 |

## Run

```sh
# once per submodule checkout: the MicroTS layout oracle for AOT compiles
(cd third_party/pocketjs && bun tools/wasm.ts)

bun tests/fonts/check.ts
bun tests/fonts/atlas-cost.ts --regular=F.ttf --bold=F.ttf --mono=F.ttf
```

`check.ts` writes to `.pocket-build/fonts-test/` and:

1. compiles `app/` with `tools/pocket` and with the submodule CLI, each as a
   JS bundle and with `--aot`;
2. reads each atlas (the bundle's pak section, or `font-<slot>.bin`), takes
   the codepoints of its cmap and bakes the slot again with
   `framework/compiler/bake-font.ts`. `tools/pocket`'s atlases must equal the
   bake from the declared face and differ from the bake from the default
   face; the submodule CLI's atlases must equal the default bake;
3. checks that both depfiles list `pocket.fonts.json` and the three faces;
4. compiles `tests/aot-smoke/app` (no declaration) both ways and requires
   identical atlases from `tools/pocket` and the submodule CLI;
5. checks that invalid declarations are rejected.

## Results

Bun 1.4.2, PocketJS v0.17.0, macOS arm64: `all checks passed` (39 checks,
about 12 s).

`tests/aot-smoke` firmware builds through `tools/pocket` (ESP-IDF v6.1-dev,
esp32s3), see `tests/aot-smoke/README.md` for the commands:

| Build | Result |
| --- | --- |
| JS, PSRAM | links, app image 1,638,864 B |
| AOT, `sdkconfig.defaults;sdkconfig.aot;sdkconfig.nopsram` | links, app image 694,512 B |
| JS and AOT with `tests/fonts/app/pocket.fonts.json` copied into `tests/aot-smoke/app/` | the incremental build reran the compile (the app directory is in the depfile), atlases changed; removing the file restored the previous atlases byte for byte |
| AOT, touching a declared face | `ninja -n` reruns `MicroTS: compiling smoke to Rust` |

No M5Stack hardware was connected; nothing was flashed. These are host-side
checks and firmware builds, not device validation.
