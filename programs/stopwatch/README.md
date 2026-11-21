# programs/stopwatch

ESP-IDF firmware for the M5Stack StopWatch (C152). It embeds
`apps/stopwatch-factory`, the recreation of the M5StopWatch-UserDemo factory
firmware.

## Board

| Item | Value |
| --- | --- |
| SoC | ESP32-S3R8, 240 MHz, 8 MB octal PSRAM, 16 MB flash |
| Display | 1.75" round AMOLED, 466 × 466 visible, CO5300 on QSPI (SPI2, 80 MHz); CS 39, SCLK 40, D0–D3 41/42/46/45, TE 38 |
| Touch | CST820 at 0x15 on the internal I2C bus, INT GPIO13, reset on IOE1 pin 4 |
| Keys | KEYA (yellow) GPIO2, KEYB (blue) GPIO1, active low; A+B held 500 ms = home |
| I2C | SDA 47, SCL 48: PM1 0x6E, IOE1 0x4F/0x6F, BMI270 0x68, RX8130CE, ES8311 |
| Power | M5PM1 PMIC (battery, charging, power hold); M5IOE1 expander (display/touch power and reset, speaker amplifier, vibration motor PWM on pin 9) |
| Audio | ES8311, I2S MCLK 18, BCLK 17, LRCK 15, DOUT 21, DIN 16; amplifier GPIO14 + IOE1 pin 10 |

`platforms/esp-idf/m5_bsp_stopwatch` brings the board up and returns the
panel, panel IO and touch handles.

## Composition (JS mode)

`main/main.c` initialises NVS and the board, then registers with the
PocketJS runtime:

- `pocket_display_esp_lcd`: two RGB565 strips of 466 × 32 px (29,824 bytes
  each, under the ESP32-S3 SPI DMA limit), byte-swapped for the panel;
- `pocket_input_touch`: the CST820 contact in logical pixels;
- `pocket_input_gpio`: KEYA → `BTN.LTRIGGER`, KEYB → `BTN.RTRIGGER`, 10 ms
  debounce;
- `pocket_guest_quickjs` with a 6 MiB PSRAM heap;
- `pocket_runner` on core 1 with a 64 KiB PSRAM stack, ticking at the host
  contract's 30 Hz cadence.

`pocket.host.json` is the host contract (`m5stack-stopwatch`: 466 × 466,
native presentation, cadence 30, `input.buttons`, `input.touch`,
`text.glyphs.baked`). The application's host config is
`apps/stopwatch-factory/pocket.host.config.ts`.

## Build

Requirements: ESP-IDF `>=6.0,<6.3`, Bun, the Espressif Rust toolchain
(`rustup` toolchain `esp` with `xtensa-esp32s3-none-elf`) and the PocketJS
layout oracle for AOT checks (`bun tools/wasm.ts` inside
`third_party/pocketjs`; the output is ignored).

```sh
. $IDF_PATH/export.sh
cd programs/stopwatch
idf.py build                       # JS mode
idf.py -p <port> flash monitor
```

`CMakeLists.txt` builds the runtime archive from the submodule
(`POCKETJS_RUST_FROM_SOURCE=ON`) with `~/.rustup/toolchains/esp/bin/cargo`
when present; pass `-DPOCKETJS_CARGO=<cargo>` otherwise. The Pocket CLI runs
through `tools/pocket` (Bun and the submodule's `tools/pocket.ts`), selectable
with `-DPOCKETJS_CLI=<path>`.

Check the application against the MicroTS subset without building firmware:

```sh
bun run pocket compile --config apps/stopwatch-factory/pocket.host.config.ts --aot --out /tmp/stopwatch-aot
```

AOT mode firmware is not available yet; it follows the shared application
host (`platforms/esp-idf/m5_pocket_app`).

## Partitions

| Name | Size |
| --- | --- |
| nvs | 24 KiB |
| phy_init | 4 KiB |
| factory | 12 MiB |
| storage (FAT, reserved for badge images) | 3,968 KiB |

## Validation

Firmware builds only; no device run. JS mode: `m5stack_stopwatch.bin`
7,834,832 bytes, 38 % of the 12 MiB app partition free.
