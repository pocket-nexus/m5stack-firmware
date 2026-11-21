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

## Composition

The program brings the board up and runs the application through the shared
host [`m5_pocket_app`](../../docs/APP-HOST.md). `CMakeLists.txt` includes
`platforms/esp-idf/m5_pocket_app/components.cmake` (mode from
`CONFIG_M5_POCKET_APP_MODE_*`, JS by default in `sdkconfig.defaults`; Pocket
CLI `tools/pocket`; runtime archive from source with the `esp` toolchain's
cargo) and adds `m5_bsp_sticks3`. `main/CMakeLists.txt` embeds the
application with `m5_pocket_app()` from
`apps/sticks3-factory/pocket.host.config.ts`.

`main/main.c` powers the board, clears the panel, sets the backlight to
230/255 (UIFlow's 90 %), starts the speaker at volume 100, initialises Wi-Fi
and calls `m5_pocket_app_start` with:

| Setting | Value |
| --- | --- |
| Host contract | `pocket.host.json` (`m5-sticks3`: 135×240, native, density 1, `repaint`, cadence 30 Hz, `input.buttons`, `text.glyphs.baked`) |
| Display | `M5_POCKET_DISPLAY_ESP_LCD`: two 40-row RGB565 strips (2 × 10 800 bytes), `byte_swap` for the big-endian panel |
| Input | `buttons` callback: KEY1 → `POCKET_BTN_CIRCLE`, KEY2 → `POCKET_BTN_DOWN`, sampled once per frame |
| Memory | runtime heap 2 MiB, QuickJS heap 4 MiB and stack limit 96 KiB in PSRAM, app task stack 128 KiB in PSRAM |
| `before_frame` | publishes `batteryLevel`, `charging` (PMIC, every 500 ms), `wifiConfigured`, `wifiStatus`, `apStations`, `portalClients`, `apSsid` |
| `on_request` | `tone`, `volume`, `brightness`; `setup 0/1` is passed to a task that stops/starts the access point |

`firmwareVersion` (`v` + `PROJECT_VER`, 0.1.0) and `cloudStatus` (-1, no
UIFlow cloud client) are published once before the start. The field and
request list is in the [application README](../../apps/sticks3-factory/README.md#device-bridge).

## Wi-Fi

`components/m5_sticks3_net` ports the Wi-Fi side of the UIFlow2 StickS3
startup to ESP-IDF: at boot the station connects to the network saved in NVS
namespace `uiflow` (`ssid0`/`pswd0`) and retries every 5 s after a
disconnect; on `setup 1` the station disconnects and the access point
`S3_XXXX` (open, two clients, last four MAC hex digits) starts with captive
DNS answering `192.168.4.1` and the UIFlow portal page
(`files/system/sticks3/index.html`, served from flash) with `/config.json`
(scan) and `/wifisave` (save and connect); `setup 0` stops them and
reconnects an idle station. `/config.json` starts its `aps` list with an
empty object because the page drops the first entry; UIFlow's handler omits
it and so hides the first network.

## Build

Requirements: ESP-IDF `>=6.0,<6.3`, Bun, `bun install` at the repository
root, and Espressif's Rust toolchain `esp` with `xtensa-esp32s3-none-elf`
(the runtime archive builds with `-Zbuild-std=core,alloc`).

JS mode (default):

```sh
. $IDF_PATH/export.sh
cd programs/sticks3
idf.py set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
```

`components.cmake` defaults `POCKETJS_CARGO` to
`~/.rustup/toolchains/esp/bin/cargo` for ESP32-S3; pass
`-DPOCKETJS_CARGO=<path>` for another location.

AOT mode: not available yet; `m5_pocket_app` does not build AOT
applications so far. The application passes AOT admission
(`bun run pocket compile --config apps/sticks3-factory/pocket.host.config.ts --aot --out <dir>`).

## Size

ESP-IDF 6.2 (`v6.1-dev-8142-g188e3e55bb2`), `idf.py size`:

| Item | Bytes |
| --- | --- |
| Application image | 3 994 469 (`0x3cf3e0`) |
| Embedded bundle | 1 891 096 (artwork as RGB565 1.66 MB) |
| `factory` partition | 7 MiB (`0x700000`), 46 % free |
| DIRAM static use | 132 137 of 341 760 (39 %) |
| IRAM | 16 384 of 16 384 |

Partition table (`partitions.csv`): `nvs` 24 KiB, `phy_init` 4 KiB,
`factory` 7 MiB.

## Validation

`idf.py build` with `-Wall -Wextra -Werror` on every component of this
repository. No device run: the program has not been flashed to a StickS3.
