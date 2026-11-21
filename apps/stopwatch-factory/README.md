# stopwatch-factory

PocketJS recreation of the M5Stack StopWatch (C152) factory firmware,
[m5stack/M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo)
at `81099c784991dc77f522de1a5bf62ec5de9ae0b5` (main, 2026-10-29). The same
TSX/TS sources build in JS mode (QuickJS bundle) and in AOT mode (MicroTS);
`pocket.host.config.ts` sets `aot: true` and `model: "compiled"`.

The host config lives here because the Pocket CLI resolves the entry
relative to the config and rejects `..`; it names the program's host
contract, `../../programs/stopwatch/pocket.host.json` (466 × 466, 30 Hz,
`input.buttons`, `input.touch`, `text.glyphs.baked`).

## Files

| File | Content |
| --- | --- |
| `main.tsx` | JS-mode `mount()` entry |
| `app.tsx`, `app.ts` | Root view and root model; `app.ts` owns every rendered signal and runs the frame task |
| `device.ts` | Device data bridge state: every host value (clock, battery, raw keys and pointer, IMU, spectrum, settings, alarms) and every host request (tones, vibration, RTC, settings, …) |
| `input.ts` | Key manager (click, hold, A+B home) and pointer tracking on the raw device fields |
| `motion.ts` | Springs (smooth_ui_toolkit visual duration / bounce) and the LVGL ease-out curve |
| `launcher.ts`, `statusbar.ts`, `watch.ts`, `stopwatch.ts`, `alarms.ts`, `setup.ts`, `wheel.ts`, `imu.ts`, `fft.ts`, `badge.ts`, `fluid.ts` | Per-app logic in state modules |
| `roller.ts` | lv_roller behaviour (drag, glide, snap, tap, wrap) for up to three pickers |
| `*.tsx` (others) | Presentational child views; they receive values and callbacks as props |
| `images/`, `images.json` | Factory artwork converted to power-of-two PNG textures |
| `scripts/import-factory-assets.ts` | Regenerates `images/` and `images.json` from a factory checkout |
| `scripts/gen-big-number.ts` | Regenerates `BigNumber.tsx` |

State modules cannot declare signals in MicroTS, so the per-app modules
keep plain fields and `app.ts` copies their results into signals once per
frame. Gestures (carousel drag, swipes, list scrolling, long presses) are
model code driven by `device.pointerDown/X/Y`, so they behave the same in
both modes.

`main.tsx` connects the JS-mode bridge (`connectDevice`, docs/APP-HOST.md);
`app.ts` re-exports the host entry points of `device.ts`. The board
(`programs/stopwatch/main/services.c`) publishes the clock, keys, battery,
IMU, microphone spectrum, settings and alarms, and handles the requests
`tone`, `vibrate`, `brightness`, `volume`, `button_sfx`, `button_vibrate`,
`wifi`, `rtc`, `guide_count`, `boot_sfx`, `mic`, `alarm_clear`/`alarm`/
`alarm_save`, `badge_slot`, `badge_edit` and `alarm_ring`.

Without a board (`device.bridged` false until `uptimeMs` arrives) the app
advances its own clock from the frame count and takes key clicks from
`ActionHandler` presses (LTRIGGER = KEYA, RTRIGGER = KEYB).

## Factory screens

| Factory screen | State |
| --- | --- |
| Home gesture guide (first five boots, exits on A+B hold) | done |
| Launcher carousel: nine apps in factory order, endless drag with one-page snap, side arrows, page dots, fading title, curved top clock | done |
| Pull-down status bar: battery gauge, level, charging colour and bolt, spring motion, auto-hide 1.8 s / 6 s | done |
| WatchFace: classic (three modes), number flow (ten themes), big number (four palettes), simple (ten themes, second dot, tips), swipe and A/B face switching, accelerometer rotation | done |
| Boot logo (StopWatch, Starting up ..., V0.5) for the first frame | done |
| Stopwatch: start, stop, resume, reset, cumulative laps newest first, scrolling lap list, key press spring | done |
| AlarmClock: list with switches, scrolling, long-press delete dialog, add screen with hour/minute rollers (default 07:00), daily trigger once per day, trigger screen with the factory beep/vibration pattern | done |
| Settings: scrolling menu, brightness (10–100) and volume (0–100, step 5) sliders, button sound/vibration switches, time and date rollers, version, ten-tap blue screen | done |
| Settings: Wi-Fi page | partial: the switch and its saved state work; the firmware has no Wi-Fi station or setup access point |
| LuckyWheel: 2–18 option roller, factory colour order, rotated labels, pointer spins (A anticlockwise, B clockwise, tap random) with cubic ease-out | done |
| IMU: ball (acceleration, size from rotation rate), orbit dot (integrated yaw), labels, tap toggle | done |
| Audio.FFT: board-side factory spectrum (512-point FFT, 20 bands, noise floor, level, smoothing, peak frequency); 40 mirrored bars, pulsing disc, Hz label, tap toggle | done |
| Badge: hint, A/B slot selection, long-press dialog, edit screen with the access point name | partial: no Wi-Fi access point, HTTP editor or JPEG display, so no badge picture is ever shown |
| FluidBox | partial: 48-particle 2D fluid (factory: 450-particle 3D SPH) |

## Differences from the factory firmware

- Fonts: PocketJS bakes Inter at fixed slot sizes (10–54 px). Montserrat,
  Maple Mono and Commissioner are drawn with the nearest Inter slot
  (Maple Mono 28 → 24 px, Maple Mono 48 → 36 px, Commissioner 64 → 54 px,
  Commissioner 108 → 54 px scaled 2×). The factory's 0 → O substitution for
  Maple Mono digits is kept.
- Number flow digits change without the smooth_ui_toolkit rolling
  animation.
- Images: PocketJS textures are power-of-two and at most 512 px, so the
  artwork sits on padded canvases (icons and the guide are stored as
  RGB565, the big-number digits as one RGBA4444 tile per palette colour and
  digit, 3.7 MB of the bundle).
- Lucky wheel sectors are drawn as rotated half-planes clipped to the two
  screen halves; the factory draws LVGL arcs. Bar spectrum wedges are
  rectangles 12 px wide instead of triangles.
- The rollers are 164 px tall everywhere (the factory day roller is 220 px)
  and glide for a fixed 300 ms of the release velocity.
- Settings sub-pages are left with A+B (as in the factory) and the menu
  scroll has no momentum.
- Alarm list order, limits and storage follow the factory (NVS namespace
  `alarm`, 68-byte blob), so alarms set by the factory firmware load.
- The badge editor screen can be left with A+B (the factory waits for the
  web page's Close).
- The 1-column offset between the factory 468-column frame and the
  466-column frame used here moves the content by at most 1 px.
- Wi-Fi icon in the status bar: not shown (Wi-Fi is not implemented).
