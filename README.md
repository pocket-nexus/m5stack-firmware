# M5Stack-sample

PocketJS firmware that recreates the M5Stack factory demos of four boards.
Each board has one ESP-IDF program that embeds one TypeScript/JSX
application. The application runs in the QuickJS guest (JS mode, the
default) or is compiled by MicroTS into a Rust static library (AOT mode).

| Board | SoC | Display | Program | Application |
| --- | --- | --- | --- | --- |
| Tab5 | ESP32-P4 + ESP32-C6 | 5" 1280×720 MIPI-DSI | `programs/tab5` | `apps/tab5-factory` |
| StopWatch (C152) | ESP32-S3 | 1.75" 466×466 round AMOLED | `programs/stopwatch` | `apps/stopwatch-factory` |
| StickS3 | ESP32-S3 | 1.14" 135×240 IPS | `programs/sticks3` | `apps/sticks3-factory` |
| AtomS3 | ESP32-S3 | 0.85" 128×128 IPS | `programs/atoms3` | `apps/atoms3-factory` |

## Requirements

- ESP-IDF `>=6.0,<6.3`
- Bun, version in `.bun-version`
- Rust through rustup: the toolchain pinned in
  `third_party/pocketjs/rust-toolchain.toml` with
  `riscv32imafc-unknown-none-elf` for ESP32-P4, and Espressif's `esp`
  toolchain for ESP32-S3

## Quick start

```sh
git clone --recurse-submodules <repository-url> M5Stack-sample
cd M5Stack-sample
bun install                 # JavaScript dependencies and git hooks
. $IDF_PATH/export.sh
cd programs/<board>
idf.py build                # JS mode
idf.py -p <port> flash monitor
```

Each program README gives its AOT build command and the board facts.

## Documentation

- [Repository structure](docs/STRUCTURE.md)
- [Engineering conventions](AGENTS.md): commits, formatting, dependencies

## PocketJS version

`third_party/pocketjs` is pinned to the `v0.17.0` release
(`83c6cac9`).

## License

MIT, see [LICENSE](LICENSE). Files derived from M5Stack or Espressif sources
keep their original license headers.
