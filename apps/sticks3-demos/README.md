# sticks3-demos

Seven demos of the M5Stack StickS3 hardware (IMU, keys, microphone, speaker,
display) behind a home carousel. Built by `programs/sticks3` in JS mode (the
default) and in AOT mode. This application is not a recreation of a factory
firmware; the UIFlow2 startup menu that `apps/sticks3-factory` reproduced
was removed (git history keeps it).

## Navigation

| Key | Home screen | In a demo |
| --- | --- | --- |
| Side key (KEY2, GPIO 12) | Next card (press; held: one card every 220 ms) | Demo control (below) |
| Front key (KEY1, GPIO 11) | Open the card's demo (press) | Demo control (below) |
| Front key held 700 ms | — | Back to the home screen |
| Power key click (PMIC) | — | Back to the home screen |

- The home screen shows one 135 px card per demo on a strip that eases to the
  selection (exponential approach, rate 18/s); after the seventh card it
  continues to the first in the same direction. Page dots in the demo's
  accent colour and the key legend sit below the cards; the title bar shows
  the battery level, a battery gauge (green, red at 15 % and below, blue
  while charging) and a lightning mark while charging.
- In a demo, holding the front key for more than 180 ms draws a white bar
  across the top that is full at 700 ms, when the home screen opens. For
  2.2 s after a demo opens, a "Hold FRONT for menu" pill shows at the bottom.
- A click is a release within 450 ms of the press. A key still held after a
  page change acts only after it is released.
- After 30 s without a key on the home screen the backlight dims from 200 to
  24; any key restores it.
- Tones: 2093 Hz on start, 2093 Hz per card, 2637 Hz on opening a demo,
  1319 Hz on returning home.

## Demos

| Demo | Accent | Controls | Hardware |
| --- | --- | --- | --- |
| Marble Maze | `#FFB020` | Tilt rolls the ball; side click: new maze; front click after a level: next level | BMI270 acceleration, speaker |
| Dice | `#FF5470` | Shake or front click: throw; side click: one or two dice | BMI270 acceleration, speaker |
| Reaction | `#4ADE80` | Front press: start, then press on green | Front key interrupt timestamp, frame presentation time |
| Snake | `#2DD4BF` | Front press: turn left; side press: turn right | Keys, speaker |
| Spirit Level | `#38BDF8` | Front click: set zero; side click: clear zero | BMI270 acceleration, speaker |
| Sound Meter | `#E879F9` | Front click: reset maximum; side click: fast/slow | ES8311 microphone |
| Metronome | `#A78BFA` | Front click: start/stop; side click: +5 BPM; side held: −5 BPM every 220 ms | Speaker with board-timed tone starts |

### Marble Maze

`maze.ts` generates a perfect maze with a depth-first search on a cell grid
over the 131 × 212 px area below the title bar: level 1 is 3 × 5 cells,
level 2 4 × 6, level 3 5 × 7, level 4 and later 5 × 8. Walls are 3 px,
merged into runs. The ball (radius 5 px) starts in the top-left cell; the
goal hole is in the bottom-right cell. From level 4 on, `level − 2` trap
holes (at most 6) sit off-centre in random cells away from start and goal;
the ball drops into a trap within 4 px of its centre and restarts at the
start, the timer running on.

Physics per frame: acceleration = tilt × 700 px/s² per g, velocity damping
`1 − 1.4 dt`, speed limit 260 px/s, up to 8 sub-steps of at most 2 px, circle
against rectangle collision with restitution 0.35. Impacts above 70 px/s play
a 8 ms tick (700 Hz + 2 × impact speed), at most every 70 ms. The timer
starts when the ball first moves 2 px. Within 5 px of the goal centre the
ball sinks in 380 ms, the level is cleared (two-note tone) and the time is
compared with the stored best of that level (levels 5 and later share the
level-5 slot).

The ball rolls towards the lower edge with screen acceleration
`(−accelX, +accelY)`, assuming M5Unified's board frame puts +X at the right
edge and +Y at the top edge of the portrait screen. This has not been
checked on a device; if the ball rolls the wrong way, the two signs in
`physics()` change.

### Dice

Each die is a 46 px cube of six Views in its own `perspective-[320]` root,
under a fixed tray rotation (`rotate-x-[24] rotate-y-[30]`, seen from above
and the left). A throw picks each value with the generator in `rng.ts`
(Park–Miller, seeded with `esp_random` at boot and stirred with key press
times), spins the cube at 520–900 °/s about X and Y, and throws it up at
230–300 px/s under 1500 px/s² gravity. Each landing keeps 42 % of the
vertical speed and 60 % of the spin and plays a 10 ms click at 950–1450 Hz;
below 70 px/s the die rests and eases in 260 ms onto the rotation that turns
the chosen face to the viewer. The face colours come from the rotated face
normals under a light from the upper left (13 grey levels). A shadow shrinks
with the height.

A shake is an acceleration magnitude more than 0.9 g away from 1 g, at most
one throw per 700 ms. The ±4 g sensor range and the 30 Hz sampling are
enough for a hand shake; short taps can fall between two samples.

### Reaction

Front press starts a random wait of 1.5–4.5 s on a dark red screen; then the
screen turns green and the press is timed. The green time is the board's
`presentedMs` of the frame that turned green (the time `after_frame` ran,
after the tick that rendered the frame and handed it to the panel), the
press time the front key's GPIO interrupt timestamp (`frontPressMs`), so the
result does not depend on the 33 ms frame period. A press during the wait,
or within 90 ms of the green frame, is a foul. The result screen shows the
time, a rating, the stored best and the average of the last five results.

The panel scanout after the transfer is not included: the result contains
the time from the end of the frame transfer to the light reaching the eye.

### Snake

15 × 24 cells of 9 px below the title bar. The snake starts with four
segments heading up; front and side presses turn it left and right relative
to its heading, with up to two turns queued per step. Each food (1320 Hz
tone) adds a segment and shortens the step from 220 ms by 7 ms, down to
75 ms. Running into a wall or into itself ends the game (two falling tones);
the body blinks for 0.9 s, then the score card shows the length and the
stored best.

### Spirit Level

Readings are low-pass filtered with a 120 ms time constant. With the stick
lying flat (|accelZ| above 0.55 g) the bull's-eye shows the bubble at 3 px
per degree (at most 44 px), the angles `atan2(x, √(y² + z²))` and
`atan2(y, √(x² + z²))`, and a horizontal vial for X. Standing on an edge,
the screen shows the roll `atan2(x, y)` and a horizon line rotated by it.
Within 0.5° of level (flat), or of 0°, 90° or 180° (edge), the bubble and
line turn green and a 2637 Hz tick plays once. Front click stores the
current attitude as zero, side click clears it.

### Sound Meter

Opening the demo requests `mic 1`, leaving it `mic 0`; the board publishes
the RMS and peak level (dBFS) of the samples captured since the previous
frame. The meter averages the RMS in the power domain with a 125 ms (fast)
or 1 s (slow) time constant, shows it as a number and on 20 segments of 3 dB
from −84 to −24 dBFS (green, yellow from 60 %, red from 85 %), holds the
peak for 1.5 s before it falls at 20 dB/s, keeps the maximum, and adds a bar
to a 44-bar history every 100 ms.

The level is relative to the ADC's full scale after the codec's gain (PGA
0 dB, ADC volume maximum, as M5Unified sets it); it is not a calibrated
sound pressure level, and the −84 to −24 dBFS range has not been checked
against the microphone on a device.

### Metronome

40 to 240 BPM in 4/4, first beat accented (1760 Hz, 45 ms; other beats
1175 Hz, 30 ms). Each click is requested up to 70 ms ahead as
`tone <hz> <ms> <atMs>`; the board starts it from an `esp_timer` one-shot
at that uptime, so the click timing does not depend on the 30 Hz frame
loop. The visuals switch beat when the clock passes the click time: the beat
dot lights, the tempo glows for 200 ms, and the pendulum reaches an end of
its 28° swing on every beat. The sliding weight sits lower on the rod for a
faster tempo, as on a mechanical metronome. Changing the tempo while running
times the next beat from the last one.

## Visual style

Background `#0B0E13`, cards and panels `#151A21`, text `#F8FAFC` and
`#94A3B8`, one accent colour per demo (table above). Everything is drawn
with Views: rectangles, `rounded-full` circles, `arc-*` rings, 2D
transforms and the 3D pipeline for the dice. There are no images. Every
animation is model state applied through `style` props at the 30 Hz frame
rate.

## Files

| File | Role |
| --- | --- |
| `pocket.host.config.ts` | Host config: `app` section (Solid, `aot: true`, `model: "compiled"`, 30 Hz) and the program's host contract `../../programs/sticks3/pocket.host.json` |
| `main.tsx` | JS-mode `mount()` entry; `connectDevice` from `m5_pocket_app/js/m5-bridge.ts` |
| `app.tsx`, `app.ts` | Root view and model: pages, frame loop, signals the views render; re-exports the device host entry points |
| `device.ts` | Device bridge state module ([APP-HOST](../../docs/APP-HOST.md)), below |
| `input.ts` | Key edges, clicks, holds, auto-repeat, home request |
| `rng.ts` | Park–Miller generator |
| `home.ts`, `maze.ts`, `dice.ts`, `reaction.ts`, `snake.ts`, `level.ts`, `sound.ts`, `metronome.ts` | State modules of the home carousel and the demos |
| `types.ts` | Records passed to the views |
| `HomeView.tsx`, `MazeView.tsx`, `DiceView.tsx`, `ReactionView.tsx`, `SnakeView.tsx`, `LevelView.tsx`, `SoundView.tsx`, `MetronomeView.tsx`, `HeaderView.tsx`, `BatteryView.tsx` | Views; props only |
| `pocket.fonts.json`, `fonts/` | Montserrat Medium and SemiBold ([Fonts](#fonts)) |

Views are named `*View.tsx` so that no view shares a case-insensitive
basename with a model module (`maze.ts`).

## Device bridge

`device.ts` holds every value the board publishes and every request the app
makes. The board publishes before every frame.

| Field (bridge name) | Type | Board source |
| --- | --- | --- |
| `bridged` | bool | 1, published once before the start; without it the app advances its own clock (desktop previews) |
| `uptimeMs` | number | `esp_timer_get_time()` in `before_frame`, ms |
| `presentedMs` | number | `esp_timer_get_time()` in the previous frame's `after_frame`, ms |
| `keyFront`, `keySide` | bool | KEY1, KEY2 levels |
| `frontPressMs`, `sidePressMs` | number | Last press edge from the GPIO interrupt, ms (`m5_sticks3_key_press_time_us`) |
| `batteryLevel`, `charging` | number, bool | PMIC battery voltage → percent, charger status, every 500 ms |
| `imuOk` | bool | The BMI270 started |
| `accelX`, `accelY`, `accelZ`, `gyroX`, `gyroY`, `gyroZ` | number | BMI270 in M5Unified's StickS3 frame, g and °/s |
| `micOn`, `micDb`, `micPeakDb` | bool, number | Capture running; RMS and peak dBFS since the previous frame |
| `seed` | number | `esp_random()` at boot |
| `best` | array | Best scores from NVS (7 slots, below), at boot |
| event `powerKey` | event | PMIC power-key single click (polled every 100 ms); counted in `powerClicks` |

| Request | Board action |
| --- | --- |
| `tone <hz> <ms>` | Sine tone now |
| `tone <hz> <ms> <atMs>` | Sine tone starting at uptime `atMs` (`esp_timer` one-shot; one pending start) |
| `brightness <0..255>` | Backlight |
| `volume <0..255>` | Speaker master volume |
| `mic <0\|1>` | Stop / start microphone capture |
| `best <slot> <value>` | Store a best score in NVS (`demos`/`best`, an `int32[7]` blob) |

| `best` slot | Value |
| --- | --- |
| 0 | Fastest reaction, ms |
| 1 | Longest snake, segments |
| 2–6 | Fastest maze, levels 1–5 (5: level 5 and later), ms |

At startup the app requests `brightness 200` and `volume 110`.

In AOT mode `programs/sticks3/aot/src/lib.rs` writes the same fields through
the generated `device_<name>_mut` accessors (`device_fields!`, the same names
as the `hostNumber`/`hostArray` switches) and counts `powerKey` in
`powerClicks`; it drains `requests` after each frame. Both modes run the same
model at 30 Hz; the keys arrive as bridge values in both, and the views bind
no `ActionHandler`.

## Fonts

`pocket.fonts.json` declares Montserrat Medium as `regular` and Montserrat
SemiBold as `bold` ([docs/FONTS.md](../../docs/FONTS.md)); `mono` keeps the
default and is not baked.

| Face | File | Version | Source | Licence |
| --- | --- | --- | --- | --- |
| Montserrat Medium | `fonts/Montserrat-Medium.ttf`, unmodified | 9.000 | [JulietaUla/Montserrat](https://github.com/JulietaUla/Montserrat) `fonts/ttf/` at `555facfb2a18c72c3c0380f0d9c0f060453a9058` | SIL OFL 1.1, `fonts/Montserrat-OFL.txt` |
| Montserrat SemiBold | `fonts/Montserrat-SemiBold.ttf`, unmodified | as above | as above | as above |

| Use | Utility | Slot | Atlas bytes |
| --- | --- | --- | --- |
| Hints, labels | `text-2xs` | 19 | 15,267 |
| Subtitles, status text | `text-xs` | 0 | 23,448 |
| Title bar titles | `text-xs font-bold` | 7 | 23,448 |
| "milliseconds" | `text-sm` | 1 | 29,912 |
| Home title, card headings | `text-sm font-bold` | 8 | 29,912 |
| Default style (baked always) | `text-base` | 2 | 37,184 |
| Overlay titles, level angles | `text-lg font-bold` | 10 | 47,486 |
| Card titles, ratings | `text-xl font-bold` | 11 | 58,899 |
| Times, scores, messages | `text-2xl font-bold` | 12 | 82,634 |
| Dice total, sound level, roll | `text-4xl font-bold` | 13 | 187,169 |
| Reaction time, "GO!", BPM | `text-5xl font-bold` | 15 | 407,450 |

The atlases hold 101 glyphs each (ASCII, the non-ASCII characters of the
sources such as `·`, `°` and `…`, and the tofu glyph): 952,384 bytes in the
JS bundle's pak; AOT mode embeds the same atlases with `include_bytes!` and
reads them in place from flash.

## Limits

- **IMU axes.** The screen mapping of the accelerometer axes is M5Unified's
  StickS3 frame plus the assumption above; not checked on a device.
- **3D drawing.** The PocketJS 3D pipeline draws no corner radius, so dice
  faces and pips are square. Pips sit 2 px out from their face: coplanar
  children are painter-sorted against the face per triangle and partly hide.
- **Sampling.** The IMU and the microphone level are read once per frame
  (30 Hz); the BMI270 runs at 100 Hz.
- **Microphone and speaker.** Both run at once on one I2S controller; tones
  reach the microphone and show on the sound meter.
- **Power key.** M5Unified reports a PMIC power-key click to applications
  (`BtnPWR`); whether a click while running is free of other PMIC actions
  has not been checked on a device. A double click is the PMIC's power-off.

## Validation

- `tools/pocket compile --config apps/sticks3-demos/pocket.host.config.ts --aot --out <dir>`:
  AOT admission and source generation pass. `programs/sticks3` builds in JS
  and in AOT mode.
- JS bundle 1,405,336 bytes (pak 952,384, script about 453 KB).
- Headless runs of the JS bundle in the PocketJS wasm core at 135 × 240 with
  a stand-in `globalThis.m5` (keys with press timestamps, IMU, microphone
  levels, battery, power key): screenshots of the home carousel (all cards,
  wrap, charging, low battery) and of every demo, kept in
  `.pocket-build/validation/sticks3-demos/`. A pixel-reading autopilot tilts
  the ball through maze levels 1 to 3 (cleared in 9.3, 10.8 and 9.0 s of
  simulated time) and level 4 shows its traps; a reaction pressed 230 ms
  after the green frame's `presentedMs` reads 230 ms; eight single-die
  throws come to rest on the face of the number shown; the requests arrive
  as `tone` (with and without start time), `brightness`, `volume`,
  `mic 1`/`mic 0` and `best`.
- No device run.
