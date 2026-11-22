# M5Stack-sample

PocketJS firmware for four M5Stack boards. Each board has one ESP-IDF
program that embeds one TypeScript/JSX application: a recreation of the
board's factory demo on Tab5, StopWatch and AtomS3, and a set of demos of
the board's hardware on StickS3. The application runs in the QuickJS guest
(JS mode, the default) or is compiled by MicroTS into a Rust static library
(AOT mode).

| Board | SoC | Display | Logical viewport | Modes | Program | Application |
| --- | --- | --- | --- | --- | --- | --- |
| Tab5 | ESP32-P4 + ESP32-C6 | 5" 720×1280 MIPI-DSI, landscape UI | 640×360 at raster density 2 | JS (default), AOT | `programs/tab5` | `apps/tab5-factory` |
| StopWatch (C152) | ESP32-S3, 8 MB PSRAM | 1.75" 466×466 round AMOLED | 466×466 | JS (default), AOT | `programs/stopwatch` | `apps/stopwatch-factory` |
| StickS3 | ESP32-S3, 8 MB PSRAM | 1.14" 135×240 IPS | 135×240 | JS (default), AOT | `programs/sticks3` | `apps/sticks3-demos` |
| AtomS3 | ESP32-S3, no PSRAM | 0.85" 128×128 IPS | 128×128 | AOT only | `programs/atoms3` | `apps/atoms3-factory` |

The AtomS3 has 512 KB of SRAM and no PSRAM. Under QEMU its JS-mode boot ran
out of internal RAM (220 bytes free at the low point) while the AOT image ran
with 168 KB free at its low point, so the AtomS3 program is AOT-only;
`programs/atoms3/README.md` has the measurements.

## Requirements

- ESP-IDF `>=6.0,<6.3`
- Bun, version in `.bun-version`
- Rust through rustup: the toolchain pinned in
  `third_party/pocketjs/rust-toolchain.toml` with
  `riscv32imafc-unknown-none-elf` for ESP32-P4, and Espressif's `esp`
  toolchain for ESP32-S3
- For AOT builds, `wasm32-unknown-unknown`: the first AOT build runs
  `bun tools/wasm.ts` inside the submodule to build the MicroTS layout
  oracle (ignored output)

## Quick start

```sh
git clone --recurse-submodules https://github.com/pocket-nexus/m5stack-sample.git
cd m5stack-sample
bun install                 # JavaScript dependencies and git hooks
. $IDF_PATH/export.sh
cd programs/<board>
idf.py build                # JS mode
idf.py -p <port> flash monitor
```

Each program README gives its AOT build command and the board facts.

## Documentation

- [Repository structure](docs/STRUCTURE.md)
- [Application host](docs/APP-HOST.md): JS/AOT modes, device bridge, C API
- [Fonts](docs/FONTS.md): per-app faces, slot sizes, atlas budgets
- [Engineering conventions](AGENTS.md): commits, formatting, dependencies

## PocketJS version

`third_party/pocketjs` is pinned to `472b4a2a` on `pocket-nexus/pocketjs`
main, four commits after the `v0.17.0` release (`83c6cac9`):

| Commit | Change |
| --- | --- |
| `21ac1ca3` | A rounded box with a flat fill keeps its corners under a turned transform (#322) |
| `8759ed00` | Arcs (`arcWidth`) are drawn under a turned transform (#731) |
| `660a35bd` | The guest frame carries the hovering pointer, wheel scroll and exact touch positions; capability `input.scroll` (#730) |
| `472b4a2a` | Zephyr display pixels in Zephyr's byte order (#703) |

The programs here declare neither `input.pointer` nor `input.scroll`; the
input records grew (`pocket_input_frame.scroll`, three `pocket_guest_frame`
fields) and the host's C input source zero-fills the new field.

## License

MIT, see [LICENSE](LICENSE). Files derived from M5Stack or Espressif sources
keep their original license headers.
