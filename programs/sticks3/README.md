# programs/sticks3

ESP-IDF firmware for the M5Stack StickS3 (K150). It embeds one application,
[`apps/sticks3-demos`](../../apps/sticks3-demos/): seven demos of the board
(marble maze, dice, reaction timer, snake, spirit level, sound meter,
metronome) behind a home carousel. Unlike the other programs of this
repository it does not recreate a factory firmware.

## Board

| Item | Value |
| --- | --- |
| SoC | ESP32-S3-PICO-1-N8R8: 8 MB quad flash, 8 MB octal PSRAM, 240 MHz |
| Display | 1.14" IPS, ST7789, 135×240 portrait, SPI3 (MOSI 39, SCLK 40, CS 41, DC 45, RST 21) at 40 MHz, backlight GPIO 38 |
| Keys | KEY1 GPIO 11 (front), KEY2 GPIO 12 (side), active low, press edges timestamped by a GPIO interrupt; power key on the M5PM1 |
| PMIC | M5PM1 at 0x6E on I2C1 (SDA 47, SCL 48): LCD rail G2, amplifier G3, charger status G0, battery voltage, power-key clicks |
| IMU | BMI270 at 0x68 on I2C1 (400 kHz), ±4 g and ±2000 dps at 100 Hz |
| Audio | ES8311 (0x18) + AW8737 amplifier; one full-duplex I2S0 at 22050 Hz: MCLK 18, BCLK 17, WS 15, DOUT 14 (speaker), DIN 16 (microphone) |
| Other | IR TX 46 / RX 42 (unused), 250 mAh battery |

Board bring-up is [`m5_bsp_sticks3`](../../platforms/esp-idf/m5_bsp_sticks3/).

## Composition

The program brings the board up and runs the application through the shared
host [`m5_pocket_app`](../../docs/APP-HOST.md). `CMakeLists.txt` includes
`platforms/esp-idf/m5_pocket_app/components.cmake` (mode from
`CONFIG_M5_POCKET_APP_MODE_*`, JS by default in `sdkconfig.defaults`; Pocket
CLI `tools/pocket`; runtime archive from source with the `esp` toolchain's
cargo) and adds `m5_bsp_sticks3`. `main/CMakeLists.txt` embeds the
application with `m5_pocket_app()` from
`apps/sticks3-demos/pocket.host.config.ts`.

`main/main.c` powers the board, clears the panel, sets the backlight to 200,
starts the speaker (and with it the microphone path) at volume 110 and the
IMU, opens NVS and loads the best scores, publishes the values below and
calls `m5_pocket_app_start` with:

| Setting | Value |
| --- | --- |
| Host contract | `pocket.host.json` (`m5-sticks3`: 135×240, native, density 1, `repaint`, cadence 30 Hz, `input.buttons`, `text.glyphs.baked`) |
| Display | `M5_POCKET_DISPLAY_ESP_LCD`: two 40-row RGB565 strips (2 × 10 800 bytes), `byte_swap` for the big-endian panel |
| Input | No button callback: the keys reach the application as bridge values |
| Memory | runtime heap limit 2 MiB, QuickJS heap 4 MiB and stack limit 96 KiB in PSRAM (JS mode), app task stack 128 KiB in PSRAM |
| `before_frame` | publishes `uptimeMs`, `presentedMs`, `keyFront`, `keySide`, `frontPressMs`, `sidePressMs`, the six IMU values and, while capturing, `micDb` and `micPeakDb`; `batteryLevel` and `charging` every 500 ms; posts `powerKey` for a PMIC power-key click (polled every 100 ms) |
| `after_frame` | records the time the tick that rendered the frame returned (`presentedMs` of the next frame) |
| `on_request` | `tone` (now, or from an `esp_timer` one-shot at a given uptime), `brightness`, `volume`, `mic 0/1`, `best <slot> <value>` |

Before the start it publishes `bridged`, `imuOk`, `micOn`, `seed`
(`esp_random()`) and the `best` array. The field and request lists are in
the [application README](../../apps/sticks3-demos/README.md#device-bridge).

Best scores are an `int32[7]` blob, NVS namespace `demos`, key `best`. The app
task's stack is in PSRAM, which must not run flash writes, so `on_request`
queues each `best` request to a task with an internal-RAM stack that writes
and commits the blob.

## Build

Requirements: ESP-IDF `>=6.0,<6.3`, Bun, `bun install` at the repository
root, and Espressif's Rust toolchain `esp` with `xtensa-esp32s3-none-elf`
(the runtime archive builds with `-Zbuild-std=core,alloc`).

JS mode (default), build directory `build/`:

```sh
. $IDF_PATH/export.sh
cd programs/sticks3
idf.py set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
```

AOT mode, build directory `build-aot/` (`sdkconfig.aot` selects
`CONFIG_M5_POCKET_APP_MODE_AOT`):

```sh
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot" set-target esp32s3 build
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig -p <port> flash monitor
```

`components.cmake` defaults `POCKETJS_CARGO` (JS-mode runtime archive) and
the AOT toolchain to Espressif's `esp` Rust toolchain for ESP32-S3. The AOT
step needs the MicroTS layout oracle `third_party/pocketjs/platforms/web/pocketjs.wasm`
and builds it with `bun tools/wasm.ts` when it is missing.

The per-app crate is `aot/` (`Cargo.lock` committed). Its `device_fields!`
list maps every board → app field to its `device_<name>_mut` accessor and
matches the `hostNumber`/`hostArray` switches of `device.ts`; its `event`
counts `powerKey` in `powerClicks`. The application has no images, so the
crate loads fonts and styles only. The two modes resolve different managed
components (AOT has no QuickJS), so AOT builds use `dependencies.aot.lock`
and JS builds `dependencies.lock`.

### Differences in AOT mode

- **Rendering and input.** Both modes run the same compiled model at 30 Hz,
  read the keys from the bridge and draw the same Views, including the 3D
  dice; there are no images and no touch input.
- **Tasks and timing.** `frames(1)` drives the frame loop in both modes; the
  demos take their time from `uptimeMs`.

## Size

ESP-IDF 6.2 (`v6.1-dev-8142-g188e3e55bb2`), `esp_idf_size` on the map file:

| Item | JS mode | AOT mode |
| --- | --- | --- |
| Application image (`.bin`) | 2 886 656 | 1 853 776 |
| `factory` partition (7 MiB) free | 4 453 376 (61 %) | 5 486 256 (75 %) |
| Application content | bundle 1 405 336 (font atlases 952 384) | generated code, styles, font atlases |
| DIRAM static use (of 341 760) | 81 585 (24 %) | 78 491 (23 %) |
| `.data` / `.bss` | 14 686 / 10 208 | 14 416 / 9 936 |
| IRAM | 16 384 of 16 384 | 16 384 of 16 384 |

The UIFlow startup menu this program embedded before (its artwork, the Wi-Fi
station and the setup portal) took about 4.02 MB (JS) and 3.11 MB (AOT) of
application image and about 130 KB of static DIRAM in either mode.

Runtime internal-RAM use was not measured: no device run, and the board
bring-up (PMIC, panel) does not run under QEMU.

Partition table (`partitions.csv`): `nvs` 24 KiB, `phy_init` 4 KiB (unused:
the firmware starts no radio), `factory` 7 MiB.

## Validation

`idf.py build` in JS and AOT mode, with `m5_strict_warnings` on `main` and
`m5_bsp_sticks3`. The application was run headless in the PocketJS wasm core
with a stand-in bridge ([application README](../../apps/sticks3-demos/README.md#validation)).
No device run: the program has not been flashed to a StickS3.
