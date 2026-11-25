# corep4x-factory

The M5Stack CoreP4X factory demo (`m5stack/M5CoreP4X-UserDemo`, commit
`cd1e1bd0bfd4b6e51a4898999b33a9392ea11f65`,
`components/m5_corep4_component/`) as a PocketJS Solid application for
`programs/corep4x`. The same sources build in JS mode (QuickJS bundle) and in
AOT mode (MicroTS): the host config sets `framework: "solid"`, `aot: true`,
`model: "compiled"`.

The factory UI is a 480x480 LVGL 9 screen; the application is laid out at
480x480 logical pixels with raster density 1, so every factory coordinate is
used as it is.

## Files

| File | Content |
| --- | --- |
| `pocket.host.config.ts` | Host config: the `app` section; host contract `../../programs/corep4x/pocket.host.json` |
| `main.tsx` | JS-mode entry: `mount()` and the device bridge (`connectDevice`) |
| `app.tsx` | The views: the three screens, the 14 hotspots, the window frame with every window's content, dialogs, the touch indicator |
| `app.ts` | Root model: screens, window motion, dialogs, each window's published state |
| `device.ts` | Device bridge state module (`docs/APP-HOST.md`): board values, requests |
| `nav.ts`, `windows.ts`, `frame.ts`, `dialog.ts`, `ease.ts` | Swipes between the screens, window open/close motion and hotspots, the dashed window frame, `corep4_ui_dialog`, LVGL's ease-in/ease-out curves |
| `power.ts`, `sleep.ts`, `off.ts`, `touch.ts`, `sd.ts`, `i2c.ts`, `imu.ts`, `rec.ts`, `music.ts`, `net.ts`, `timer.ts`, `ir.ts` | One model per window (`net.ts`: PING, WiFi, USB) |
| `*View.tsx`, `WindowFrame.tsx`, `Dashes.tsx`, `Dialog.tsx`, `Button.tsx`, `GpioPopup.tsx`, `RecSetDialog.tsx`, `TouchCanvas.tsx`, `TimeTile.tsx` | Views |
| `wrap.ts` | Montserrat Medium advance widths: line wrapping and `LV_LABEL_LONG_DOT` truncation as LVGL measures them |
| `assets/`, `images.json` | Pictures (below); the screens and canvases are stored as RGB565 (PSP PSM 5650) |
| `tools/convert-assets.ts` | LVGL C image arrays to PNG; the canvas pictures, two symbols and the dash strips |
| `tools/preview.ts` | Simulator screenshots of a scripted tour |

## Assets

| File | Source |
| --- | --- |
| `startup.png`, `info.png`, `function.png` | `corep4_lvgl_ui_out/images/ui_img_corep4x_ui_{startup,info,function}_png.c` of M5CoreP4X-UserDemo (MIT, Copyright (c) 2026 M5Stack Technology CO LTD), decoded by `tools/convert-assets.ts`; info and function are RGB565A8, blended over the theme's 0xF5F5F5 |
| `touch-canvas.png`, `imu-canvas.png` | Drawn by `tools/convert-assets.ts`: the empty 5x5 grid and the IMU view's background, shown until the board uploads its first texture |
| `rec-canvas.png` | The idle Waveform card as `programs/corep4x/main/audio_draw.c` draws it |
| `dash-h.png`, `dash-v.png` | Drawn by `tools/convert-assets.ts`: 512x2 and 2x512 strips of 7 px 0xCBD5E1 dashes and 5 px gaps, from which `frame.ts` cuts every dashed edge of the window frame |
| `refresh.png`, `check.png` | `LV_SYMBOL_REFRESH` and `LV_SYMBOL_OK` of LVGL 9's `lv_font_montserrat_14` (MIT; Font Awesome 5 Free, SIL OFL 1.1) |

PocketJS textures are power-of-two sized and at most 512 px on a side: each
picture is padded to 512x512 and the views clip it. The three canvases are
replaced at run time by program textures under the same names
(`programs/corep4x/README.md`).

## Fonts

`app.fonts` declares the faces LVGL's `lv_font_montserrat_*` are built from:
`regular` `fonts/Montserrat-Medium.ttf`, `bold` `fonts/Montserrat-SemiBold.ttf`
(no text uses `font-bold`, so no bold atlas is baked). `tools/pocket compile`
bakes them in JS and AOT mode (`docs/FONTS.md`).

| Face | Version | Source | Licence |
| --- | --- | --- | --- |
| Montserrat Medium, Montserrat SemiBold | 9.000 | `github.com/JulietaUla/Montserrat` commit `555facfb2a18c72c3c0380f0d9c0f060453a9058`, `fonts/ttf/` | SIL OFL 1.1, `fonts/Montserrat-OFL.txt` |

| Factory font (px) | Used for | Slot | Match |
| --- | --- | --- | --- |
| 10 | M5BUS tab: row headers, L/R marks, GPIO card pin names | `text-2xs` 10 | exact |
| 12 | M5BUS tab: GPIO numbers | `text-xs` 12 | exact |
| 14 | the LVGL default: SD rows and buttons, OFF status, status lines | `text-sm` 14 | exact |
| 16 | most texts, buttons and dialogs | `text-base` 16 | exact |
| 20 | I2C bus buttons, IMU overlay, TIMER panel titles and +/- | `text-xl` 20 | exact |
| 24 | IR byte values | `text-2xl` 24 | exact |
| 28 | POWER rail toggles, MUSIC title | `text-2xl` 24 | nearest |
| 44 | TIMER values | `text-4xl` 36 | nearest |

## Device bridge

`device.ts` holds every board value and every request; `programs/corep4x`
publishes the values under these names and serves the requests
(`programs/corep4x/README.md` has the table per service). In AOT mode
`programs/corep4x/aot/src/lib.rs` maps the same list. Without a board
(simulator, browser) `hostAttached` stays false and the windows show the
defaults in `device.ts`; there is no simulation.

Gestures are model code on `pointerX`, `pointerY` and `pointerDown`: the
screen swipes (50 px with a minimum speed, as LVGL's gesture detection), the
POWER slider, the MUSIC volume slider, the SD list drag, the IR byte tiles
(one step per 12 px).

## Screens reproduced

| Factory | Here |
| --- | --- |
| `ui_startup`, `ui_info`, `ui_function` | The three pictures; a swipe moves between them in 500 ms (linear), startup → info on a right swipe, → function on a left swipe |
| Function screen hotspots | 14 areas open the windows; the sleep and power icons open SLEEP and OFF |
| `ui_function_window` | 170x48 header (action button, Exit), 460x418 content panel, dashed outline and tabs; opens from the touched area over 220 ms (ease-out) and closes into Exit over 180 ms (ease-in) |
| `corep4_ui_dialog` | Small, medium and large dialogs with their footers |
| POWER | Peripheral, System and M5BUS tabs: eight rail toggles with the LCD/L3B/AudioIC warning dialogs (10 s restart, Restart button), backlight slider, charge 180 mA / 650 mA / OFF, filtered VBAT, 20 M5-Bus GPIO cards with live levels and the configuration popup |
| SLEEP | BTN, RTC (10/60/300 s), Touch and IMU wake toggles, Deep Sleep with its status |
| OFF | Power Off and ESP Restart with the 3.1 s countdown, Cancel, Boot Sound, firmware version |
| TOUCH | The full-screen 5x5 canvas with strokes, visited cells, the progress pill, Info/Clean/Exit at 25/25 and the 10 s idle close; the info page with the controller status and the touch indicator switch |
| SD CARD | Capacity, path, the first 48 entries (directories first), select, double tap to open, Back, the speed test dialog |
| I2C SCAN | Internal and PORT.A scans (three passes), the eight known internal devices as OK/MISS, unknown addresses, Clean |
| IMU | The textured device model following the board, one-finger drag, two-finger twist, finger markers, recentre, the calibration and failure overlays |
| REC | MIC1/MIC2 (and REF) waveforms with dB levels, REC / END REC / PLAY LAST / END PLAY, the Set dialog (Waveform/Spectrum, REF), the spectrograms |
| MUSIC | CandyWind with 24 spectrum bars, progress, time, status, PLAY/PAUSE, volume slider |
| PING | Ethernet status, four targets with checkboxes, Ping with per-target results and the summary |
| TIMER | DATE and TIME tiles with +/-, Save per panel, NTP |
| WiFi | The factory layout under the module overlay (see differences) |
| USB | The factory layout with its counters (see differences) |
| IR TxRx | NEC address/command tiles, Auto+1, Repeat x2, Send NEC, Reinit, the decoded RX frame, Clear |
| Touch indicator | The 8x8 ring on the contact on every screen when enabled |

## Differences from the factory firmware

- **Font sizes**: the factory's Montserrat 28 (POWER toggles, MUSIC title)
  uses the 24 px slot and Montserrat 44 (TIMER values) the 36 px slot.
- **WiFi**: the A172 Stamp-AddOn C6 For P4 module is not driven. The window
  keeps the factory layout under its overlay, which reads "WiFi needs the
  module A172 Stamp-AddOn C6 For P4. This firmware does not drive it."; Scan
  AP and Retry are disabled and there is no Info button.
- **USB**: the USB host is not run. The counters stay at 0 and the result
  line reads "USB host is not supported by this firmware." one second after
  the window opens and on Check.
- **TOUCH**: the CST3530 firmware upgrade card is not shown (the updater is
  not implemented). The status line reads "Panel: CST3XX OK" once the
  controller answers; the factory always prints "--" because its
  initialised flag is never set. The canvas follows the contacts once per
  frame (30 Hz) instead of every 16 ms, and the visited cells' rounded
  corners are not anti-aliased.
- **IMU**: the view is rendered at up to 30 Hz (the factory: 40 Hz).
- **SD CARD**: card insertion and removal are noticed within about 1.3 s
  (the detect pin is read every second, 300 ms debounce); the factory polls
  the expander every 5 s.
- **PM1 button**: a click is handled when its interrupt arrives; the factory
  checks the interrupt flag every 2 s.
- **Deep sleep**: the application stops first, then the factory's order
  follows; if entering deep sleep fails, the board restarts (the factory
  leaves the screen dark).
- **Time**: the system clock is set from the RTC at boot (the factory never
  does). A missing RTC is probed again every second.
- **Ethernet** follows the L3B rail: turning L3B off removes the driver and
  releases the RMII pins, turning it on reinstalls it. Losing the DHCP lease
  shows "Link Up".
- **IR**: Repeat x2 sends the frame twice; the factory passes a loop count
  of 1, which ESP-IDF 6 transmits once (m5stack/M5CoreP4X-UserDemo#1). A
  send reports its result after the frame went out (about 0.12 s later than
  the factory, which reports when it is queued).
- **REC and MUSIC**:
  - Stopping playback or a recording runs on a task, so the UI does not
    freeze for about 1 s; meanwhile MUSIC still shows Playing and REC shows
    ENDING....
  - Opening REC reads `REC.PCM` on a task: REC is disabled until it is read.
  - Playback fades change the codec volume only, so the volume label does
    not flash 10 % or 0 %.
  - The factory's "Embedded track unavailable" state is not modelled.
  - Chart lines are anti-aliased approximations of LVGL's line drawing.
  - Switching AudioIC off stops audio and releases the codecs cleanly; the
    factory keeps writing to the unpowered codecs (the warning dialog still
    asks for a restart, as in the factory).
- Without a board the windows show their defaults; nothing is simulated.
- LVGL theme effects (pressed-button shading, focus outlines) are not drawn.

## Checks

```sh
bun run pocket compile --config apps/corep4x-factory/pocket.host.config.ts \
  --outdir .pocket-build/js --output .pocket-build/js/app.pkb
bun run pocket compile --config apps/corep4x-factory/pocket.host.config.ts \
  --aot --out ../../.pocket-build/aot
bun apps/corep4x-factory/tools/preview.ts apps/corep4x-factory/.pocket-build/js \
  third_party/pocketjs/platforms/web/pocketjs.wasm .pocket-build/validation/preview
```

`--outdir` is relative to the config. `--aot` needs the wasm core
`third_party/pocketjs/platforms/web/pocketjs.wasm` (`bun tools/wasm.ts`
inside the submodule). The preview tool runs the JS bundle in PocketJS's
simulator and writes one picture per step of a tour through every window.
