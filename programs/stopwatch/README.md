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

## Composition

The program uses the shared application host
(`platforms/esp-idf/m5_pocket_app`, [docs/APP-HOST.md](../../docs/APP-HOST.md)):
`CMakeLists.txt` includes its `components.cmake`, and `main/CMakeLists.txt`
embeds `apps/stopwatch-factory` with `m5_pocket_app()`.

- `main/main.c` initialises NVS and the board (`m5_stopwatch_init`), starts
  the services and calls `m5_pocket_app_start` with the CO5300 panel
  (`M5_POCKET_DISPLAY_ESP_LCD`, two RGB565 strips of 466 × 32 px, 29,824
  bytes each, under the ESP32-S3 SPI DMA limit, byte-swapped), the CST820
  contact as `touch`, KEYA/KEYB as `BTN.LTRIGGER`/`BTN.RTRIGGER`, a 6 MiB
  QuickJS heap in PSRAM and a 64 KiB internal-RAM app task on core 1
  (internal because the request hook writes NVS).
- `main/services.c` is the board side of the device bridge. Before every
  frame it publishes `uptimeMs`, the wall clock (system time, set from the
  RX8130CE at boot), `keyA`/`keyB`, the IMU and, once per second, the
  battery. At boot it publishes the settings, the alarms and the badge
  state. It handles the application's requests: tones (factory sine with a
  200-sample fade) and the factory boot sound on an audio task, vibration,
  brightness, volume, button settings, RTC writes and alarm storage, with
  the factory NVS keys (`system`: `bl_lev`, `spk_vol`, `btn_sfx`,
  `btn_vibrate`, `wifi_enabled`, `launch_count`; `alarm`: `storage`).
- `main/spectrum.c` computes the factory microphone spectrum (512-point
  FFT every 256 samples, 20 bands, peak frequency) on a microphone task
  while the Audio.FFT page requests it.
- `main/assets/boot_sfx.bin` is the factory boot sound (M5StopWatch-UserDemo
  `main/assets/sfx/boot_sfx.bin`, MIT): 16-bit mono PCM at 44.1 kHz.

`pocket.host.json` is the host contract (`m5stack-stopwatch`: 466 × 466,
native presentation, cadence 30, `input.buttons`, `input.touch`,
`text.glyphs.baked`). The application's host config is
`apps/stopwatch-factory/pocket.host.config.ts`.

## Build

Requirements: ESP-IDF `>=6.0,<6.3`, Bun, the Espressif Rust toolchain
(`rustup` toolchain `esp` with `xtensa-esp32s3-none-elf`) and, for AOT
checks, the PocketJS layout oracle (`bun tools/wasm.ts` inside
`third_party/pocketjs`; the output is ignored).

```sh
. $IDF_PATH/export.sh
cd programs/stopwatch
idf.py build                       # JS mode (CONFIG_M5_POCKET_APP_MODE_JS)
idf.py -p <port> flash monitor
```

`components.cmake` builds the runtime archive from the submodule with
`~/.rustup/toolchains/esp/bin/cargo` and runs the Pocket CLI through
`tools/pocket`.

Check the application against the MicroTS subset without building firmware:

```sh
bun run pocket compile --config apps/stopwatch-factory/pocket.host.config.ts --aot --out /tmp/stopwatch-aot
```

AOT-mode firmware follows when the shared host's AOT mode lands.

## Partitions

| Name | Size |
| --- | --- |
| nvs | 24 KiB |
| phy_init | 4 KiB |
| factory | 12 MiB |
| storage (FAT, reserved for badge images) | 3,968 KiB |

## Validation

Firmware builds only; no device run. JS mode (`idf.py build`, ESP-IDF 6.x):
`m5stack_stopwatch.bin` 8,481,024 bytes, 33 % of the 12 MiB app partition
free (the application bundle is about 6.4 MB, 3.7 MB of it the big-number
artwork); static DIRAM use 104,945 of 341,760 bytes.
