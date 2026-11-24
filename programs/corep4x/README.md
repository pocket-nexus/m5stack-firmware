# programs/corep4x

ESP-IDF program for the M5Stack CoreP4X (C157). It brings the board up with
`platforms/esp-idf/m5_bsp_corep4x`, runs `apps/corep4x-factory` through the
shared app host `platforms/esp-idf/m5_pocket_app` (docs/APP-HOST.md) and
serves the application's device bridge (`main/services.c` and the
`main/svc_*.c` service modules).

## Build

ESP-IDF `>=6.0,<6.3` (built with 6.2, `v6.1-dev-8142-g188e3e55bb2`), Bun, and
the rustup toolchain of `third_party/pocketjs/rust-toolchain.toml` (1.98.1)
with `riscv32imafc-unknown-none-elf`: the runtime archive is built from
source (`POCKETJS_RUST_FROM_SOURCE=ON`, set by `components.cmake`).

```sh
. $IDF_PATH/export.sh
cd programs/corep4x
# JS mode (CONFIG_M5_POCKET_APP_MODE_JS, the default), build directory build/
idf.py set-target esp32p4 build
# AOT mode (CONFIG_M5_POCKET_APP_MODE_AOT), build directory build-aot/
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot" set-target esp32p4 build
idf.py -p <port> flash monitor          # or -B build-aot ... flash monitor
```

The CoreP4X carries an ESP32-P4 of chip revision 3.x; `sdkconfig.defaults`
sets `CONFIG_ESP32P4_REV_MIN_300`, so the image does not start on revision
1.x or 2.x silicon. The JS build compiles
`apps/corep4x-factory/pocket.host.config.ts` against this program's host
contract `pocket.host.json` and embeds the bundle. The AOT build runs
`pocket compile --aot` on the same config, builds the crate `aot/`
(`libm5_pocket_app_aot.a`) and links it in place of the runtime archive and
QuickJS. AOT mode keeps its own `dependencies.aot.lock`.

`aot/src/lib.rs` maps every board value to its `device.ts` field
(`device_fields!`, the eight `rail<Name>` values into `device.rails`,
`sleepError` into `device.sleepErrors`) and the six events, in the same order
as the `hostNumber`, `hostText`, `hostArray` and `hostEvent` switches of
`apps/corep4x-factory/device.ts`; the two lists change together.

### Build results

| Item | JS mode | AOT mode |
| --- | --- | --- |
| Application image | 11,189,680 bytes (0xaabdb0), 89 % of the 12 MiB `factory` partition | 10,264,272 bytes (0x9c9ed0), 82 % |
| Flash contents | bundle 4,255,912 bytes (pack 3,575,232 bytes, of which 414,750 are font atlases); audio 4,225,088 bytes; IMU model textures 491,520 bytes | 8 pictures 3,147,840 bytes, font atlases 406,288 bytes; audio and IMU textures as in JS mode |
| Internal RAM, static (DIRAM) | 129,534 of 655,360 bytes (.data 13,392, .bss 27,632) | 128,946 bytes (.data 13,376, .bss 27,608) |
| Staging strips | 2 x 76,800 bytes internal DMA RAM (80 raster rows of 480 px) | same |
| Heaps | QuickJS and runtime heap limits 16 MiB each, PSRAM preferred | runtime heap limit 16 MiB; the pictures are copied into the heap at mount (3,147,840 bytes) |
| App task stack | 192 KiB in PSRAM, QuickJS stack limit 144 KiB | 192 KiB in PSRAM |

The audio is the factory's embedded material: the 45 s "CandyWind" track
(mono 44.1 kHz, 3,969,044 bytes) and the startup sound (stereo 16 kHz,
256,044 bytes), as WAV files in `assets/audio/`. These numbers are build
outputs; no device run happened.

### What differs in AOT mode

The application uses no framework gestures: swipes, drags, sliders and the
IR byte tiles are model code reading `pointerX`, `pointerY` and
`pointerDown`, identical in both modes. Buttons are `onPress` and act on
release in both modes. JS mode delivers board values through the `device.ts`
host functions; AOT mode writes the fields directly. Program textures
(`m5_pocket_app_set_texture`) work in both modes.

## Board facts

| Part | Detail |
| --- | --- |
| SoC | Stamp-P4X: ESP32-P4NRW32X, chip revision 3.x, 32 MiB PSRAM at 200 MHz, 16 MiB flash |
| Panel | 2.64" 480x480 ST7102, MIPI-DSI 2 lanes at 700 Mbps, DPI 24 MHz, RGB565 framebuffer in PSRAM; mounted upside down |
| Touch | CST3530 at 0x58, interrupt GPIO1, up to 5 contacts |
| Backlight | M5IOE1 IO9, PWM channel 0 at 50 kHz |
| Power | M5PM1 at 0x6E (battery, charger, M5-Bus 5 V boost, power button, wake sources, IRQ to GPIO0); M5IOE1 expander (rails, resets, microSD detect, charger status) |
| Audio | ES8311 DAC (0x18) + AW8737A amplifier, ES7210 4-channel ADC (0x40); I2S0 at 16 kHz |
| Sensors | LSM6DS3TR-C IMU (0x6A or 0x6B), RX8130CE RTC (0x32) |
| Storage | microSD on SPI3 (CS 50, MOSI 7, MISO 8, CLK 10) at 20 MHz; SPIFFS `storage` partition |
| Network | IP101 Ethernet PHY over RMII; optional ESP32-C6 module (A172 Stamp-AddOn C6 For P4) on SDIO, not driven |
| Interfaces | IR TX GPIO49 / RX GPIO13; Port A I2C (SDA 18, SCL 16); M5-Bus GPIOs; USB-A host port (not driven) |

Partitions (`partitions.csv`, 16 MiB flash): `nvs` 24 KiB, `phy_init` 4 KiB,
`factory` 12 MiB, `storage` 3 MiB SPIFFS mounted at `/webspiffs` (the REC
window's `REC.PCM`; formatted on first mount).

## Presentation

The host contract (`pocket.host.json`, id `m5stack-corep4x-factory`) has one
logical viewport of 480x480 at raster density 1, `native` presentation, one
30 Hz cadence and one touch contact.

`m5_pocket_app` creates `pocket_display_esp_dpi` with `half_turn = true`: the
runtime renders 480x480 raster strips of 80 rows into two internal DMA
buffers and the PPA turns each damaged rectangle 180 degrees into the panel
framebuffer, the orientation LVGL's `LV_DISPLAY_ROTATION_180` gives the
factory firmware. Two PPA renderers split each strip at a multiple of 8
raster rows: the app task (core 0) renders the upper rows, a helper task on
core 1 the lower ones. The touch input reads the controller once per frame;
the contacts are the upright picture's coordinates minus one pixel on each
axis, as in the factory (`m5_bsp_corep4x/README.md`).

## Device bridge

`main/services.c` registers the hooks: `before_frame` calls every module's
frame function (publishing values, uploading textures), `on_request`
dispatches each request to the module that owns it. Requests run on the
host's internal-stack helper task while the app task waits, so NVS, FAT and
SPIFFS work is allowed there; long work runs on the modules' tasks, which
publish through `m5_pocket_bridge_set_*`. Values that must arrive together
with an event (the I2C scan's addresses and its result) are published from
the frame hook.

| Module | Requests | Values and events |
| --- | --- | --- |
| `svc_power.c` | `rail <L3B\|LCD\|AudioIC\|AudioPA\|USBA\|P4USBC\|Grove\|M5BUS> <0\|1>`, `lcdOff`, `backlight <1..100>`, `charge <0\|1\|2>`, `powerPage <0\|1>`, `gpioConfig <gpio> <out> <level> <opendrain> <pull>`, `powerOff`, `restart [1]`, `sleepButton <0\|1>`, `sleepWake <s> <touch> <imu>`, `deepSleep <s> <touch> <imu>` | `pm1Ready`, `pybReady`, `firmwareVersion`, `rail<Name>` (read back every second), `backlight`, `charge`, `vbatFiltered`, `gpioLevels`, `gpioConfigError`, `sleepError`; events `gpioConfig`, `powerOff` |
| `svc_rtc.c` | `timerPage <0\|1>`, `rtcSet Y M D h m s`, `rtcNtp` | `rtcReady`, `rtcYear`..`rtcSecond` (every second), `ntpReady` |
| `svc_storage.c` | `sdList <path>`, `sdTest` | `sdReady`, `sdStatus`, `sdListing`, `sdPathError`, `sdFolderSize`, `sdTestText`; events `sdListed`, `sdTest` |
| `svc_i2c.c` | `i2cScan <0\|1>` | `i2cAddresses`; event `i2cScan` |
| `svc_canvas.c` | `touchCanvas <0\|1>`, `touchCanvasPause <0\|1>`, `touchCanvasClean`, `touchIndicator <0\|1>` | `touchVisited`, `touchPillDark`, `touchTexture`, `touchStatus`, `touchIndicator` |
| `svc_imu.c` | `imu <0\|1>`, `imuRecenter` | `imuState`, `imuError`, `imuTexture`, `imuMarkers`, `imuMarker{0,1}{X,Y}` |
| `svc_audio.c` | `rec <0\|1>`, `recView <spectrum> <ref>`, `recStart`, `recEnd`, `recPlay`, `recPlayStop`, `music <0\|1>`, `musicPlay`, `musicStop`, `volume <0..100>`, `bootSound <0\|1>` | `recState`, `recStopping`, `recPlayStopping`, `recBusy`, `recPlayBusy`, `recSpectrum`, `recDb0..2`, `recTexture`, `musicState`, `musicElapsedMs`, `musicDurationMs`, `musicBars`, `volume`, `bootSound` |
| `svc_net.c` | `pingPage <0\|1>`, `ping <mask>` | `ethState`, `ethIp`, `ethGateway`, `pingRunning`, `pingRows`, `pingSummary`, `pingSummaryTone` |
| `svc_ir.c` | `ir <0\|1>`, `irInit`, `irSend <address> <command> <repeat> <auto>`, `irClear` | `irReady`, `irInitializing`, `irSending`, `irResult`, `irResultTone`, `irFrames`, `irAddress`, `irCommand`, `irRepeat`; event `irNext` |

The host publishes `pointerX`, `pointerY` and `pointerDown`; `services.c`
publishes `hostAttached`. Preferences are the factory's NVS namespace
`corep4_prefs` (`boot_sound`, `touch_ind`).

Three windows draw pictures the board computes into program textures
(512x512 RGB565 in PocketJS's PSM_5650 layout, uploaded with
`m5_pocket_app_set_texture` at most every 50 ms when they changed):

| Texture | Content | Drawn by |
| --- | --- | --- |
| `assets/touch-canvas.png` | the TOUCH canvas, 480x480: grid, strokes, visited cells | `svc_canvas.c` on the app task |
| `assets/imu-canvas.png` | the IMU view, 460x418: gradient, shadow, the textured device model (`imu_view.c`, a C port of the factory's Mahony filter and renderer) | a render task |
| `assets/rec-canvas.png` | the REC display card, 448x298 or 448x406: waveform charts or spectrograms (`audio_draw.c`) | `audio_ui` (core 1), double buffered |

Tasks besides the app task and the PPA helper: `p4_pm1_task` (PM1 button
IRQ, VBAT and GPIO sampling, rail read-back), `sd_hotplug` (card detect every
second, listings, speed test), `rtc`, `net` (Ethernet, follows the L3B rail),
`ir_rx_task`, `imu_fusion` (10 ms) and `imu_render` while the IMU window is
open, `audio_ctl` and `audio_ui` (core 1), and short-lived workers for scans,
pings, sends, recording and playback.

## Deep sleep

`deepSleep` (SLEEP window) and a single click of the PM1 power button with
BTN sleep on (after 1 s) run the factory's sequence on a task: the
application stops (`m5_pocket_app_stop`), the RTC reads stop, the IMU is
set up for motion wake or turned off, Ethernet stops and releases its pins,
audio stops and releases I2S, the microSD card is unmounted, the backlight
goes off, the touch controller sleeps with its reset held (unless touch
wakes the board), the panel is deleted and its rail cut, the RX8130 timer is
armed for RTC wake, the rails go off and `m5_corep4x_deep_sleep` enters deep
sleep with the PM1 IRQ (button, RTC, IMU) and optionally the touch interrupt
as wake sources (`m5_bsp_corep4x/README.md`). A wake boots the firmware from
the start. If the deep-sleep entry fails the board restarts.

## Not implemented

- Wi-Fi: the A172 Stamp-AddOn C6 For P4 module (ESP-Hosted over SDIO) is not
  driven; the WiFi window says so.
- USB-A host: the hub, device and storage counters stay at 0; the USB window
  says so.
- The CST3530 firmware updater of the TOUCH window.
- The factory's AEC and speech-recognition audio paths (not reachable from
  its windows).

## Validation

No CoreP4X board was available: this firmware has not run on a device.
What was checked:

- Firmware builds with ESP-IDF 6.2 in JS and AOT mode, without warnings
  (sizes above).
- `apps/corep4x-factory/tools/preview.ts`: the JS bundle in the PocketJS
  simulator through every screen and window (layout and artwork only; the
  simulator has no board values).
- Host-side tests of the pure C parts against the factory sources: the IMU
  filter and renderer (`imu_view.c`, pixel-identical to the factory C++ over
  300 poses), the audio resampler (bit-identical over the whole track), the
  REC spectrum DFT, the RX8130 register encoding, the NEC timing tables and
  the PING texts.
