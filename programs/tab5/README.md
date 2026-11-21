# programs/tab5

ESP-IDF program for the M5Stack Tab5. It brings the board up with
`platforms/esp-idf/m5_bsp_tab5`, runs `apps/tab5-factory` through the shared
app host `platforms/esp-idf/m5_pocket_app` (docs/APP-HOST.md) and serves the
application's device bridge (`main/services.c`, `main/sound.c`).

## Build

ESP-IDF `>=6.0,<6.3` (built with 6.2, `v6.1-dev-8142-g188e3e55bb2`), Bun, and
the rustup toolchain of `third_party/pocketjs/rust-toolchain.toml` (1.98.1)
with `riscv32imafc-unknown-none-elf`: the runtime archive is built from
source (`POCKETJS_RUST_FROM_SOURCE=ON`, set by `components.cmake`).

```sh
. $IDF_PATH/export.sh
cd programs/tab5
idf.py set-target esp32p4      # once
idf.py build                   # JS mode (CONFIG_M5_POCKET_APP_MODE_JS, the default)
idf.py -p <port> flash monitor
```

AOT mode (`CONFIG_M5_POCKET_APP_MODE_AOT`) is not wired yet: the shared app
host's AOT build is still in progress. The application already passes
`pocket compile --aot` (see `apps/tab5-factory/README.md`).

The build compiles `apps/tab5-factory/pocket.host.config.ts` against this
program's host contract `pocket.host.json` and embeds the bundle. It leaves
an ignored `__pycache__` in `third_party/pocketjs/platforms/esp-idf/pocket_runtime/tools/`;
delete it before committing.

### Build results (JS mode)

| Item | Size |
| --- | --- |
| Application image | 7,351,598 bytes (0x702df0), 58 % of the 12 MiB `factory` partition |
| Bundle (`tab5.pkb`) | 4,479,704 bytes, of which 3,983,440 are the pack (images: RGB565 background tiles and charts, RGBA8888 small pictures; five baked Inter sizes) |
| Internal RAM, static | 102,420 of 441,296 bytes (DIRAM: .text 72,284, .data 15,456, .bss 14,680) |
| Staging strips | 2 x 122,880 bytes internal DMA RAM (48 raster rows of 1280 px) |
| QuickJS heap limit, runtime heap limit | 24 MiB each, PSRAM preferred |
| App task stack | 192 KiB in PSRAM, QuickJS stack limit 144 KiB |

No device run happened for these numbers: they are build outputs.

## Board facts

| Part | Detail |
| --- | --- |
| SoC | ESP32-P4NRW32: 32 MiB PSRAM, 16 MiB flash; ESP32-C6 radio on SDIO (esp_hosted, not used yet) |
| Panel | 5" 720x1280 MIPI-DSI, 2 lanes, RGB565 framebuffer in PSRAM; ILI9881C + GT911 (older boards) or ST7123 / ST7121 with integrated touch, detected at boot (`m5_bsp_tab5/README.md`) |
| Backlight | GPIO22, LEDC 12-bit at 5 kHz |
| IO expanders | PI4IOE5V6408 at 0x43 and 0x44: RF switch, speaker amplifier, EXT 5V, camera, LCD/touch reset, headphone detect, ESP32-C6 power, USB-A 5V, power-off request, quick charge, USB-C detect, charge enable |
| Audio | ES8388 DAC (0x10) + NS4150B amplifier, ES7210 4-channel ADC (0x40): MIC-L, AEC loopback, MIC-R, headphone microphone; I2S1 at 48 kHz |
| Sensors | BMI270 IMU (0x68), RX8130CE RTC (0x32), INA226 power monitor (0x41, 5 mOhm), on-chip temperature |
| Power | IP2326 charger (charge enable, quick charge) |
| Storage | microSD on SDMMC slot 0, 4-bit (CLK 43, CMD 44, D0-D3 39-42), IO power from LDO4 |
| Interfaces | RS485 on UART1 (TX 20, RX 21, DE 34); Port A I2C (SDA 53, SCL 54); Ext.Port1 and MBUS test GPIOs |

## Presentation

The host contract (`pocket.host.json`, id `m5stack-tab5-factory`) has one
logical viewport of 640x360 at raster density 2, `native` presentation on
the 720x1280 panel, one 30 Hz cadence, the `repaint` animation class and one
touch contact. PocketJS limits a touch viewport to 1024 logical pixels per
axis, which rules out the factory's 1280x720 as a logical size.

`m5_pocket_app` creates `pocket_display_esp_dpi` with `turned = true`: the
runtime renders 1280x720 raster strips into two internal DMA buffers and the
PPA turns each damaged rectangle 90 degrees counter-clockwise into the panel
framebuffer, the orientation LVGL's `LV_DISPLAY_ROTATION_90` gives the
factory firmware. One AXI GDMA channel pair carries whole-row copies and
retained moves. A turned strip must be a multiple of 8 raster rows; the
strip is 48 rows (122,880 bytes), so a full 1280x720 frame is 15 strips.

The touch controller reports panel coordinates; the program sets
`mirror_y` and `swap_xy` on the `esp_lcd_touch` handle, so picture
coordinates are (1279 - y, x), and scales them by 1/2 to logical pixels. The
app host publishes them as `pointerX`, `pointerY` and `pointerDown`.

Frame budget (estimates, no device measurement): at 30 Hz a frame has
33.3 ms. PocketJS's platform contract documents, for the Tab5 at 360 MHz, a
full-surface repaint of 25 to 60 ms per core, a translated opaque page moved
by DMA in under 20 ms and an icon-sized region in under 10 ms. The launcher
repaints only small label rectangles once per poll period; the window springs
repaint the union of the window's old and new rectangles each frame (up to
800x480 raster for the RTC and camera windows), which can exceed one frame
on a single PPA renderer.

**Single PPA renderer.** `m5_pocket_app` registers one `pocket_render_esp_ppa`
renderer for an ESP_DPI display. The two-lane renderer
(`pocketjs_esp32p4_ppa_lanes`, one lane per core) needs a host option the
app host does not have yet; the program's first version, before the app host
landed, used it.

## Device bridge

`main/services.c` publishes the board values `apps/tab5-factory/device.ts`
declares, at the factory panels' periods (clock every frame from the system
clock, which follows the RTC; INA226 and BMI270 every 100 ms; expander inputs
every 200 ms; CPU temperature every second), and carries out the requests.
`main/sound.c` synthesizes the factory tones, decodes the factory MP3 clips
(`audio/`, from `platforms/tab5/audio/` of M5Tab5-UserDemo, MIT) with libhelix
and resamples them from 44.1 kHz to 48 kHz, runs the 3 s record-and-play test
and turns 20 ms capture chunks into the chart columns.

| Request | Action |
| --- | --- |
| `backlight`, `volume` | LEDC duty; ES8388 volume of the next sound (0 mutes) |
| `switch <name> <0/1>` | Expander output |
| `tone`, `sound startup` | Factory tones; startup clip |
| `rtc` | RX8130CE, then the system clock |
| `power 1..4` | Shutdown clip and power-off pulse; sleep until touched (backlight off); BMI270 any-motion wakeup and power off; RTC alarm wakeup and power off (the factory sets the clock to 00:01:49 for it) |
| `i2cScan <0/1/2>` | Scan every 300 ms; Port A only with EXT 5V on |
| `gpioTest`, `gpio` | The 24 factory test pins as outputs |
| `capture`, `recordTest`, `music` | Chart columns, record test, music clip |
| `rs485`, `rs485Send` | Received chunks as `rs485Text`; send a line |
| `sdScan` | Mount, list the root, unmount |
| `camera` | Reports the request back as `cameraCapturing` (no camera pipeline) |

## Not implemented

- **Camera preview** (SC2356 / SC202CS on MIPI-CSI through esp_video): the
  window opens and the canvas stays black.
- **Wi-Fi access point** (`M5Tab5-UserDemo-WiFi` and its web page on the
  ESP32-C6 through esp_hosted): the window shows the factory text only.
- **USB-A host** (HID mouse and the USB-A detect arrow): `usbAConnected` is
  always false.
- Two-lane PPA rendering (above).

## Validation

`idf.py build` in JS mode succeeds with `-Wall -Wextra -Werror` on every
component of this repository. No M5Stack hardware was connected: no device
run, flash or measurement happened. The application was checked with
`pocket compile` (JS bundle and `--aot` admission) and simulator screenshots
(`apps/tab5-factory/tools/preview.ts`).
