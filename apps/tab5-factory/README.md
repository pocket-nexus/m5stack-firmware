# tab5-factory

The M5Stack Tab5 factory demo (`m5stack/M5Tab5-UserDemo`, commit
`fe8aa051beb9385468f7e46b909e764169df3f7a`, `app/`) as a PocketJS Solid
application for `programs/tab5`. The same sources build in JS mode (QuickJS
bundle) and in AOT mode (MicroTS): the host config sets `framework: "solid"`,
`aot: true`, `model: "compiled"`.

The factory UI is a 1280x720 LVGL 9 screen. PocketJS limits a touch
viewport to 1024 logical pixels per axis, so the application is laid out at
**640x360 logical pixels with raster density 2**: every factory coordinate is
halved, and the 1280x720 raster carries the factory artwork at full
resolution (`assets/<name>@2x.png`).

## Files

| File | Content |
| --- | --- |
| `pocket.host.config.ts` | Host config: the `app` section; host contract `../../programs/tab5/pocket.host.json` |
| `main.tsx` | JS-mode entry: `mount()` and the device bridge (`connectDevice`) |
| `app.tsx` | The views: startup animation, launcher, the 15 windows, toasts |
| `app.ts` | Root model: animation, launcher polling, window and toast state, sounds, drags |
| `device.ts` | Device bridge state module (`docs/APP-HOST.md`): board values, requests, simulation |
| `motion.ts` | Spring (Motion visualDuration/bounce) and linear tween, as smooth_ui_toolkit's AnimateValue |
| `calendar.ts` | Month grid arithmetic for the RTC window |
| `WindowFrame.tsx`, `Chart.tsx`, `Spinner.tsx` | ui::Window frame, the audio charts, lv_spinner |
| `assets/` | Factory images converted to PNG (`tools/convert-assets.ts`) |
| `images.json` | The opaque pictures are stored as RGB565 (PSP PSM 5650) |
| `tools/convert-assets.ts` | LVGL C image arrays to `<name>.png` / `<name>@2x.png` |
| `tools/preview.ts` | Simulator screenshots of a scripted tour |

## Assets

`assets/` holds `app/assets/images/*.c` of M5Tab5-UserDemo (MIT, Copyright
(c) 2025 M5Stack Technology CO LTD), decoded from RGB565 by
`tools/convert-assets.ts`. PocketJS textures are power-of-two sized and at
most 512 px on a side: each picture is padded at its right and bottom, and
the 1280x720 `launcher_bg` is cut into six tiles (`launcher-bg-<row>-<column>`).
`mouse_cursor` (the USB mouse pointer) is not used.

## Fonts

`app.fonts` in `pocket.host.config.ts` declares the faces the factory's LVGL fonts are built
from (LVGL's `lv_font_montserrat_*` are Montserrat Medium): `regular`
`fonts/Montserrat-Medium.ttf`, `bold` `fonts/Montserrat-SemiBold.ttf` (no
text uses `font-bold`, so no bold atlas is baked). `tools/pocket compile`
bakes them in JS and AOT mode (`docs/FONTS.md`).

| Face | Version | Source | Licence |
| --- | --- | --- | --- |
| Montserrat Medium, Montserrat SemiBold | 9.000 | `github.com/JulietaUla/Montserrat` commit `555facfb2a18c72c3c0380f0d9c0f060453a9058`, `fonts/ttf/` | SIL OFL 1.1, `fonts/Montserrat-OFL.txt` |

The panel runs at raster density 2: a factory font of N physical px is the
N/2 logical px slot, baked at 2x.

| Factory font (px) | Used for | Logical size | Slot | Match |
| --- | --- | --- | --- | --- |
| 14 (LVGL default) | calendar | 7 | `text-2xs` 10 | nearest (none smaller) |
| 16 | IMU labels, LCD IC, record buttons, I2C addresses, capture label | 8 | `text-2xs` 10 | nearest |
| 18 | date, CPU temperature, window titles, headphone state, RS485 text | 9 | `text-2xs` 10 | nearest |
| 22 | time, voltage, current, GPIO toggles, antenna message, send button | 11 | `text-xs` 12 | nearest |
| 24 | toasts, volume, window texts and buttons, rollers, "V0.5" | 12 | `text-xs` 12 | exact |
| 28 | GPIO pin names | 14 | `text-sm` 14 | exact |
| 36 | brightness, power window headers | 18 | `text-lg` 18 | exact |

Every factory size has a slot within 2 logical px; the large numbers (36 px)
are exact, so none is drawn as an image. Baked atlases (98 glyphs each,
@2x): 10 px 56,856 B, 12 px 88,608 B, 14 px 113,696 B, 16 px 141,920 B (the
default slot, always baked), 18 px 173,280 B.

## Device bridge

`device.ts` holds every board value and every request. Without a board
(simulator, browser) `hostAttached` stays false and the model calls
`simulateFrame` and `simulateSecond`: a ticking clock from 2025/6/18
9:41:00, fixed power readings (7.72 V, 0.24 A), the Tab5's internal I2C
addresses, quiet microphone columns and a 6 s record test.

Board values (published by `programs/tab5` under these names; in AOT mode
`programs/tab5/aot/src/lib.rs` maps the same list): `hostAttached`, `rtcYear`,
`rtcMonth` (1..12), `rtcDay`, `rtcHour`, `rtcMinute`, `rtcSecond`,
`busVoltage`, `shuntCurrent`, `cpuTemperature`, `accelX/Y/Z`, `gyroX/Y/Z`,
`usbCConnected`, `usbAConnected`, `headphoneConnected`, `panelIc`, `sdList`
(names joined by `/`), `sdDirectories`, event `sdScan`, `i2cAddresses`, event
`i2cScan`, `captureLeft`, `captureRight` (60 peak columns, 0..100),
`recordTestState`, `musicPlaying`, `cameraCapturing`, `rs485Text`, event
`rs485`, `pointerX`, `pointerY`, `pointerDown`.

Requests: `backlight <0..100>`, `volume <0..100>`,
`switch <charge|quickCharge|ext5v|usbA5v|extAntenna> <0|1>`,
`tone <ms> <midi>...`, `sound startup`, `rtc <y> <m> <d> <h> <min> <s>`,
`power <1 off|2 sleep-touch|3 off-shake|4 off-10s>`, `i2cScan <0|1|2>`,
`gpioTest <0|1> <pin>...`, `gpio <pin> <0|1>`, `capture <0..3>`,
`recordTest <1|2>`, `music <0|1>`, `rs485 <0|1>`, `rs485Send <text>`,
`camera <0|1>`, `sdScan`.

## Screens reproduced

| Factory | Here |
| --- | --- |
| `AppStartupAnim` | White screen, "TAB" rises 785 -> 309 with a 0.6 s / 0.2 spring and a 0.6 s fade, then "5" slides 700 -> 82 (0.4 s / 0.2), the startup sound at volume 80 when it passes 90, one second on the logo, "V0.5" |
| `LauncherView` background | `launcher_bg` at full resolution |
| `PanelRtc` | Time (1 s) and date labels; tap opens `RtcSettingWindow` |
| `PanelLcdBacklight` | Brightness label with the 8 px bump, +/- 20 % steps clamped 20..100 with the factory tones, `LCD IC: <panel>` |
| `PanelSpeakerVolume` | Volume label, +/- 20 % steps 0..100 |
| `PanelPowerMonitor` | Voltage and current (100 ms), CPU temperature (1 s), charge arrows while the current is >= 0 |
| `PanelImu` | X/Y/Z labels (100 ms), the pink dot following acceleration and growing with motion |
| `PanelSwitches` | Charge, quick charge, EXT 5V, USB-A 5V and RF switch images and toggles with their toasts; USB-C/USB-A/headphone arrows; USB-A and headphone plug toasts and sounds; `WifiApMsgWindow` |
| `PanelPower` | Power off (15 s countdown), sleep with touch wakeup, power off with shake wakeup, power off with 10 s RTC wakeup |
| `PanelCamera` | `CameraWindow` with "Opening Camera ...", the 760x440 canvas toggling full screen, "Closing Camera ..." |
| `PanelDualMic` | `MicTestWindow`: two waveform charts, REC 3S / REC / PLAY button with spinner |
| `PanelHeadphone` | `HeadphoneTestWindow`: chart, record button, connected label |
| `PanelSdCard` | `SdCardScanWindow`: directory and file rows or "No files found on SD Card." |
| `PanelI2cScan` | `I2cScanWindow`: Port A / internal chart pictures, address labels every 300 ms, the EXT 5V button |
| `PanelGpioTest` | `ExtPortWindow` (G49 G50 G0 G1 G54 G53) and `MbusWindow` (18 pins), LOW/HIGH toggles, drag scrolling |
| `PanelMusic` | `MusicTestWindow`: speaker loopback chart, PLAY MUSIC / STOP with spinner |
| `PanelComMonitor` | `ComMonitorWindow`: received text, `Send "Hello M5Stack!"` |
| `ui::Window` | Spring from the closed to the opened key frame (0.4 s / 0.2), 0.8 s fade on close, background tap closes (except Wi-Fi and MBUS), close dot and button on the Wi-Fi window |
| `ui::Toast` | Dark toasts sliding in from the top (0.4 s / 0.3 spring), stacking up to three, 1.6 s on screen, tap to close |
| `audio::play_*` | Progression, random and plug tones as `tone` requests |

## Differences from the factory firmware

- **Font sizes**: PocketJS bakes a fixed slot table, so factory sizes
  without an `N/2` slot use the nearest one (see Fonts below): small labels
  at 16, 18 and 22 px are 1 to 2 logical px larger.
- **Charts** are drawn as 60 peak columns (the board computes them from the
  48 kHz capture) instead of LVGL's 512-point line series.
- **Spinners** are a grey track and a 60 degree arc turning once a second;
  LVGL's spinner arc also changes length.
- **Calendar**: the header dropdowns list 26 years (the current year +10 to
  -15) and the 12 months in a fixed panel; LVGL's dropdown lists scroll. The
  selected day is filled blue. Rollers show one value and change with a
  vertical drag (22 logical px per step, wrapping); they do not animate the
  neighbouring values.
- **Scrolling**: only the Ext.Port1 and MBUS windows scroll (by drag, no
  momentum). The SD window shows the first eight entries; the RS485 window
  keeps the last 15 lines without a cursor.
- **SD entries** use small coloured blocks instead of the FontAwesome folder
  and file symbols.
- **Camera**: the canvas stays black; the program has no camera preview yet
  (see `programs/tab5/README.md`).
- **Window concurrency**: each window kind exists once; reopening a window
  that is still closing restarts it from its closed key frame.
- **Spring integration**: eight Euler steps per 30 Hz frame; the motion
  settles within 0.5 px instead of smooth_ui_toolkit's thresholds.
- Without a board the values are simulated (above); the factory has no such
  mode.
- LVGL theme effects (pressed-button grow, focus outlines) are not drawn.

## Checks

```sh
bun run pocket compile --config apps/tab5-factory/pocket.host.config.ts \
  --outdir .pocket-build/js --output .pocket-build/js/app.pkb
bun run pocket compile --config apps/tab5-factory/pocket.host.config.ts \
  --aot --out ../../.pocket-build/aot
bun apps/tab5-factory/tools/preview.ts apps/tab5-factory/.pocket-build/js \
  third_party/pocketjs/platforms/web/pocketjs.wasm .pocket-build/validation/preview
```

`programs/tab5` builds the AOT output into firmware (`programs/tab5/README.md`).
`--aot` needs the wasm core at `third_party/pocketjs/platforms/web/pocketjs.wasm`
(`bun tools/wasm.ts` inside the submodule; ignored output). The preview tool
runs the JS bundle in PocketJS's deterministic simulator and writes one PNG
per step of a tour through every window; it checks layout and artwork, not
device behaviour.
