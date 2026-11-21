# programs/atoms3

ESP-IDF program for the M5Stack AtomS3: it brings up the board with
`platforms/esp-idf/m5_bsp_atoms3` and runs `apps/atoms3-factory` through
`platforms/esp-idf/m5_pocket_app`. **The program is AOT-only**: the
application is compiled by MicroTS into the Rust library of `aot/`; the
QuickJS guest is not built (see [Memory budget](#memory-budget) for why).

## Board facts

| Item | Value |
| --- | --- |
| SoC | ESP32-S3FN8, dual-core 240 MHz, 512 KiB SRAM, **no PSRAM**, 8 MB flash |
| Display | 0.85" 128×128 IPS, GC9107 on SPI3 (MOSI 21, SCLK 17, DC 33, CS 15, RST 34), 40 MHz, GRAM row offset 32 |
| Backlight | GPIO16, LEDC 256 Hz, brightness 127 at boot |
| Button | GPIO41, active low |
| IMU | MPU6886 at 0x68 on I2C SDA 38 / SCL 39 |
| IR | GPIO4, NEC with 38 kHz carrier |
| Port A | G1 = GPIO1, G2 = GPIO2 |
| USB | USB Serial/JTAG: console and the factory's USB serial bridge |

The AtomS3R (ESP32-S3-PICO with 8 MB PSRAM, BMI270/BMM150, other panel
pins, I2C backlight) is a different board and is not supported. Details and
sources: `platforms/esp-idf/m5_bsp_atoms3/README.md`.

## Files

| File | Content |
| --- | --- |
| `CMakeLists.txt` | Project: `m5_pocket_app/components.cmake` and the BSP component |
| `main/main.c` | Board bring-up, `m5_pocket_app_start` with the GC9107 (`M5_POCKET_DISPLAY_ESP_LCD`), memory limits, heap logs |
| `main/atoms3_device.c` | Button click/hold → `BTN.CIRCLE`/`BTN.CROSS`; the factory functions' hardware behind the device bridge (Wi-Fi scan, I2C scan, UART bridge, PWM, ADC, IR, IMU) |
| `aot/` | Per-app crate: links the MicroTS output with `platforms/rust/m5_pocket_aot`; the artwork is stored in flash as RGB565 (`images.json`, `emit_images`) and decoded on first draw (`load_images_at_density`, `ImageStorage::Flash`) under a 32 KiB texture budget |
| `pocket.host.json` | Host contract: 128×128 native, density 1, repaint, 30 Hz, buttons, baked glyphs |
| `sdkconfig.defaults`, `partitions.csv`, `dependencies.lock` | Configuration (AOT mode, no PSRAM, 7 MB factory partition) |

The host config is `apps/atoms3-factory/pocket.host.config.ts`.

## Build

```sh
bun install                          # once, at the repository root
. $IDF_PATH/export.sh                # ESP-IDF >=6.0,<6.3
cd programs/atoms3
idf.py set-target esp32s3 build
idf.py -p <port> flash monitor
```

Requirements: Bun, the rustup toolchain `esp` with
`xtensa-esp32s3-none-elf` (the crate builds with `-Zbuild-std=core,alloc`),
and the MicroTS layout oracle `third_party/pocketjs/platforms/web/pocketjs.wasm`
(built on demand by `m5_pocket_app`, `bun tools/wasm.ts` in the submodule).

JS mode is not offered: `sdkconfig.defaults` selects
`CONFIG_M5_POCKET_APP_MODE_AOT=y` and `dependencies.lock` is the AOT
resolution (no QuickJS component). A JS-mode image of this application does
not boot in the available RAM; see below.

AOT admission of the application alone:

```sh
bun run pocket compile --config apps/atoms3-factory/pocket.host.config.ts --aot --out /tmp/atoms3-aot
```

## Build results

ESP-IDF v6.1-dev-8142-g188e3e55bb2, esp32s3, AOT mode:

| Item | Bytes |
| --- | --- |
| `m5_atoms3.bin` | 2,087,872 (factory partition 7,340,032: 5,252,160 free) |
| Flash data | 838,176 (includes 18 × 32 KiB RGB565 artwork, two text images and three efont atlases, 38,064 B) |
| Static internal RAM (DIRAM) | 105,797 used of 341,760 (`.data` 21,694, `.bss` 21,840, IRAM-in-DIRAM text 62,263) |

## Memory budget

All numbers are internal RAM; there is no PSRAM.

| Consumer | Setting | Bytes |
| --- | --- | --- |
| Display strips | 2 × 16 rows × 128 × 2 (`main.c`) | 8,192 |
| App task stack | `CONFIG_M5_POCKET_APP_TASK_STACK` | 40,960 |
| Runtime heap limit | `RUNTIME_HEAP_LIMIT` (`main.c`) | 163,840 |
| Decoded artwork kept past a frame | `TEXTURE_BUDGET` (`aot/src/lib.rs`) | 32,768 |
| Fonts | loaded in place from flash | 0 |

Measured under QEMU (Espressif qemu-xtensa 9.2.2, `-machine esp32s3`), with
the real application and limits but a transfer-only display instead of the
GC9107 and no BSP or Wi-Fi in the image; scripted button pulses walk boot →
menu → Wi-Fi → menu → IMU. Harness: `.pocket-build/validation/atoms3-memory/`
(not committed).

| Mode | Free before start | While running | Minimum | Result |
| --- | --- | --- | --- | --- |
| AOT (32 KiB task stack) | 389,136 | 183,436–188,184 | 168,108 | 402 frames, 0 errors, longest frame 27,052 µs, 5,320 B of the stack never used |
| AOT with the efont faces | 389,136 | 185,616 | 170,256 | 401 frames, 0 errors, longest frame 34,361 µs |
| AOT, efont, shared flash image loader | 389,136 | 185,520 | 170,180 | 402 frames, 0 errors, longest frame 21,073 µs |
| JS (bundle 985,544 B: 786,912 B assets in flash, about 195 KB QuickJS bytecode) | 387,248 | — | 220 | boot fails: QuickJS `InternalError` (out of memory) before the first frame |

So the AOT application, runtime, layout and one decoded artwork take at most
about 221 KB including the task stack. The JS image exhausts the whole
internal heap during boot: QuickJS copies the bytecode into its heap and
the framework's objects come on top, which needs more than the 387 KB the
QEMU image had free; the firmware has less (its Wi-Fi/BSP static RAM is
56,454 B more than the harness). The task stack was raised to 40 KiB after
the measurement left 5,320 B unused of 32 KiB.

Estimate for the firmware, not measured on a device: free heap before start
about 389,136 − 56,454 ≈ 332 KB; minimum while running about 332 − 229 ≈
100 KB before Wi-Fi. Entering the Wi-Fi function starts the radio (buffers
reduced in `sdkconfig.defaults`); leaving it de-initialises the radio. Device
logs print `heap <stage> free … largest … minimum …` at boot, after board
bring-up, after start and every 300 frames.

## Validation

`idf.py build` in AOT mode, AOT admission, and the QEMU runs above. **No
device run happened** (no AtomS3 was connected); panel init, IR, Port A and
Wi-Fi are unverified on hardware.
