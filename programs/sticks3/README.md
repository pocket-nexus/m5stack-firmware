# programs/sticks3

ESP-IDF firmware for the M5Stack StickS3 (K150). It embeds one application,
[`apps/sticks3-factory`](../../apps/sticks3-factory/), the recreation of the
UIFlow2 StickS3 startup menu.

## Board

| Item | Value |
| --- | --- |
| SoC | ESP32-S3-PICO-1-N8R8: 8 MB quad flash, 8 MB octal PSRAM, 240 MHz |
| Display | 1.14" IPS, ST7789, 135×240 portrait, SPI3 (MOSI 39, SCLK 40, CS 41, DC 45, RST 21) at 40 MHz, backlight GPIO 38 |
| Keys | KEY1 GPIO 11 (front), KEY2 GPIO 12 (side), active low; power key on the M5PM1 |
| PMIC | M5PM1 at 0x6E on I2C1 (SDA 47, SCL 48): LCD rail G2, amplifier G3, charger status G0, battery voltage |
| Audio | ES8311 (0x18) + AW8737 amplifier; I2S MCLK 18, BCLK 17, WS 15, DOUT 14, DIN 16 |
| Other | BMI270 IMU (0x68), IR TX 46 / RX 42, 250 mAh battery |

Board bring-up is [`m5_bsp_sticks3`](../../platforms/esp-idf/m5_bsp_sticks3/).

## Composition (JS mode)

`main/main.c` powers the board, clears the panel, sets the backlight to
230/255 (UIFlow's 90 %), starts the speaker at volume 100 and registers:

| PocketJS component | Configuration |
| --- | --- |
| `pocket_runtime` | Host contract `pocket.host.json` (`m5-sticks3`: 135×240, native, density 1, `repaint`, cadence 30 Hz, `input.buttons`, `text.glyphs.baked`) |
| `pocket_guest_quickjs` | 4 MiB heap, 96 KiB stack limit, PSRAM |
| `pocket_display_esp_lcd` | Two 40-row RGB565 strips (2 × 10 800 bytes), `byte_swap` for the big-endian panel |
| `pocket_input_gpio` | KEY1 → `CIRCLE`, KEY2 → `DOWN`, 10 ms debounce |
| `pocket_runner` | Owner task `sticks3_ui`, 128 KiB stack in PSRAM |

`components/m5_sticks3_net` ports the Wi-Fi side of the UIFlow2 StickS3
startup to ESP-IDF: at boot the station connects to the network saved in NVS
namespace `uiflow` (`ssid0`/`pswd0`) and retries every 5 s after a
disconnect; the Setup access point `S3_XXXX` (open, two clients, last four
MAC hex digits) runs captive DNS answering `192.168.4.1` and the UIFlow
portal page (`files/system/sticks3/index.html`, served from flash) with
`/config.json` (scan) and `/wifisave` (save and connect). `/config.json`
starts its `aps` list with an empty object because the page drops the first
entry; UIFlow's handler omits it and so hides the first network. The
application starts and stops the access point and reads the status through
the device data bridge; until the bridge exists only the boot-time station
connection runs.

`main/CMakeLists.txt` compiles the application with `pocketjs_compile_app`
from `apps/sticks3-factory/pocket.host.config.ts`. The top-level
`CMakeLists.txt` writes `build/pocket-cli`, a wrapper that runs
`third_party/pocketjs/tools/pocket.ts` with Bun, and passes it as
`POCKETJS_CLI`. The PocketJS runtime archive is built from source
(`POCKETJS_RUST_FROM_SOURCE=ON`).

## Build

Requirements: ESP-IDF `>=6.0,<6.3`, Bun, `bun install` at the repository
root, and Espressif's Rust toolchain `esp` with `xtensa-esp32s3-none-elf`
(the runtime archive builds with `-Zbuild-std=core,alloc`).

JS mode (default):

```sh
. $IDF_PATH/export.sh
cd programs/sticks3
idf.py -DPOCKETJS_CARGO=$HOME/.rustup/toolchains/esp/bin/cargo set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
```

`POCKETJS_CARGO` selects the `esp` toolchain's cargo; the `rustc` beside it
is used. Without it CMake takes `cargo` from `PATH`, whose rustup toolchain
(pinned by the submodule's `rust-toolchain.toml`) has no Xtensa target.

AOT mode: not available yet. It needs the shared application host
(`platforms/esp-idf/m5_pocket_app`); the application already passes AOT
admission (`bun run pocket compile --config apps/sticks3-factory/pocket.host.config.ts --aot --out <dir>`).

## Size

ESP-IDF 6.2 (`v6.1-dev-8142-g188e3e55bb2`), `idf.py size`:

| Item | Bytes |
| --- | --- |
| Application image | 3 929 805 (`0x3bf740`) |
| Embedded bundle | 1 884 040 (artwork as RGB565 1.66 MB) |
| `factory` partition | 7 MiB (`0x700000`), 46 % free |
| DIRAM static use | 125 657 of 341 760 (37 %) |
| IRAM | 16 384 of 16 384 |

Partition table (`partitions.csv`): `nvs` 24 KiB, `phy_init` 4 KiB,
`factory` 7 MiB.

## Validation

`idf.py build` with `-Wall -Wextra -Werror` on every component of this
repository. No device run: the program has not been flashed to a StickS3.
