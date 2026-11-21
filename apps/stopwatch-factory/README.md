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
| `*View.tsx`, other `*.tsx` | Presentational child views; they receive values and callbacks as props. A view that shares its name with a logic module carries a `View` suffix: on a case-insensitive file system `Stopwatch.tsx` would otherwise take `stopwatch.ts` as its basename model |
| `images/`, `images.json` | Factory artwork converted to power-of-two PNG textures |
| `scripts/import-factory-assets.ts` | Regenerates `images/` and `images.json` from a factory checkout |
| `scripts/gen-big-number.ts` | Regenerates `BigNumber.tsx` |
| `DigitStrip.tsx` | One rolling number-flow digit |

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
| Badge: active JPEG picture, hint when empty, A/B slot selection, long-press dialog, edit screen; the board runs the factory access point, captive DNS, phone editor and six JPEG slots | done |
| FluidBox | partial: 48-particle 2D fluid (factory: 450-particle 3D SPH); see the FluidBox limit below |

## Differences from the factory firmware

- Fonts: PocketJS bakes Inter at fixed slot sizes (10–54 px). Montserrat,
  Maple Mono and Commissioner are drawn with the nearest Inter slot
  (Maple Mono 28 → 24 px, Maple Mono 48 → 36 px, Commissioner 64 → 54 px,
  Commissioner 108 → 54 px; PocketJS scales the positions of a scaled text run but not its baked glyphs, so no scaling is applied). The factory's 0 → O substitution for
  Maple Mono digits is kept.
- Number flow digits roll like smooth_ui_toolkit 2.12.1 `DigitFlow`
  (strip 9,0..9,0, spring with visual duration 0.6 s and bounce 0.05, up
  when the value grew, down when it shrank, velocity kept across the wrap).
  With a turned face the 66 px digit windows stay upright and only the
  glyphs turn, so a digit rolls along the screen's vertical axis.
- Images: PocketJS textures are power-of-two and at most 512 px, so the
  artwork sits on padded canvases (icons and the guide are stored as
  RGB565, the big-number digits as one RGBA4444 tile per palette colour and
  digit, 3.7 MB of the bundle).
- The watch-face panel is not turned as a whole: a turned subtree loses
  its rounded corners in the PocketJS rasterizer, so ticks, hands, texts and
  digit images turn one by one about their anchors while the dial, the date
  pill and the number-flow tiles only move.
- Lucky wheel sectors are drawn as rotated half-planes clipped to the two
  screen halves; the factory draws LVGL arcs. Bar spectrum wedges are
  rectangles 12 px wide instead of triangles.
- The rollers are 164 px tall everywhere (the factory day roller is 220 px)
  and glide for a fixed 300 ms of the release velocity.
- Settings sub-pages are left with A+B (as in the factory) and the menu
  scroll has no momentum.
- Alarm list order, limits and storage follow the factory (NVS namespace
  `alarm`, 68-byte blob), so alarms set by the factory firmware load.
- The badge editor screen can also be left with A+B (the factory waits only
  for the web page's Close).
- The badge picture is a run-time texture: the board decodes the active
  JPEG (scaled by 1/2, 1/4 or 1/8 when larger than 466 px) onto the smallest
  power-of-two RGB565 canvas that holds it and registers it under
  `images/badge_slot.png` with `m5_pocket_app_set_texture` in both modes
  (`main.tsx` also binds the published handle in JS mode). The view hides the image
  for one frame when the handle changes so the node binds the new texture.

## FluidBox limit

The factory simulates 450 particles with a 3D double-density SPH at 80 Hz
in C on its own task (neighbour grid, up to 14,400 pairs) and paints them
directly into the framebuffer at 25 Hz from the other core. Here the
simulation is model code (QuickJS in JS mode) on the app task, which also
lays out and rasterises the frame at 30 Hz, and every particle is a UI node.
Estimate, not measured on a device: 48 particles cost about 4,500 pair
evaluations per frame (two passes, two sub-steps); 450 particles with the
factory's grid cost about 13 times more, and each frame would also update
about 1,350 node properties and repaint the full 466 × 466 frame. At the
interpreter speeds of QuickJS on a 240 MHz S3 that is several times the
33 ms frame budget, so the JS-mode app keeps 48 particles. AOT mode runs the
same model as compiled Rust; a larger count there still pays the per-node
drawing cost.
- The 1-column offset between the factory 468-column frame and the
  466-column frame used here moves the content by at most 1 px.
- Wi-Fi icon in the status bar: not shown (Wi-Fi is not implemented).
