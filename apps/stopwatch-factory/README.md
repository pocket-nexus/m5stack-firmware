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
| `launcher.ts`, `statusbar.ts`, `watch.ts`, `stopwatch.ts` | Per-app logic in state modules |
| `*.tsx` (others) | Presentational child views; they receive values and callbacks as props |
| `images/`, `images.json` | Factory artwork converted to power-of-two PNG textures |
| `scripts/import-factory-assets.ts` | Regenerates `images/` and `images.json` from a factory checkout |
| `scripts/gen-big-number.ts` | Regenerates `BigNumber.tsx` |

State modules cannot declare signals in MicroTS, so the per-app modules
keep plain fields and `app.ts` copies their results into signals once per
frame. Gestures (carousel drag, swipes, list scrolling, long presses) are
model code driven by `device.pointerDown/X/Y`, so they behave the same in
both modes.

Until a host bridge writes `device.ts`, `device.bridged` stays false: the
app advances its own clock from the frame count, keys come from
`ActionHandler` presses (LTRIGGER = KEYA, RTRIGGER = KEYB, reported as
clicks), and drags are unavailable; taps work through the guest's touch
input.

## Factory screens

| Factory screen | State |
| --- | --- |
| Home gesture guide (first five boots, exits on A+B hold) | done |
| Launcher carousel: nine apps in factory order, endless drag with one-page snap, side arrows, page dots, fading title, curved top clock | done |
| Pull-down status bar: battery gauge, level, charging colour and bolt, spring motion, auto-hide 1.8 s / 6 s | done |
| WatchFace: classic (three modes), number flow (ten themes), big number (four palettes), simple (ten themes, second dot, tips), swipe and A/B face switching, accelerometer rotation | done |
| Stopwatch: start, stop, resume, reset, cumulative laps newest first, scrolling lap list, key press spring | done |

## Differences from the factory firmware

- Fonts: PocketJS bakes Inter at fixed slot sizes (10–54 px). Montserrat,
  Maple Mono and Commissioner are drawn with the nearest Inter slot
  (Maple Mono 28 → 24 px, Maple Mono 48 → 36 px, Commissioner 64 → 54 px,
  Commissioner 108 → 54 px scaled 2×). The factory's 0 → O substitution for
  Maple Mono digits is kept.
- Number flow digits change without the smooth_ui_toolkit rolling
  animation.
- The 1-column offset between the factory 468-column frame and the
  466-column frame used here moves the content by at most 1 px.
- Wi-Fi icon in the status bar: not shown (Wi-Fi is not implemented).
