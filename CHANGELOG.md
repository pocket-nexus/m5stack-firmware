# Changelog

Each version is a git tag `v<version>`. The Firmware workflow publishes the
section of that version as the release notes, with one merged flash image per
board and mode.

## 0.1.3 - 2026-11-24

StopWatch: the AOT image runs at a 60 Hz cadence, 56 to 60 frames per second
on still screens, the watch faces and the running stopwatch. PocketJS
`484a3100`; ESP-IDF 6.2, as in 0.1.2.

| Image | Board | Application | Mode |
| --- | --- | --- | --- |
| `m5stack-firmware-tab5-js-v0.1.3.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | JS |
| `m5stack-firmware-tab5-aot-v0.1.3.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | AOT |
| `m5stack-firmware-stopwatch-js-v0.1.3.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | JS |
| `m5stack-firmware-stopwatch-aot-v0.1.3.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | AOT |
| `m5stack-firmware-sticks3-js-v0.1.3.bin` | StickS3 (ESP32-S3) | Seven board demos | JS |
| `m5stack-firmware-sticks3-aot-v0.1.3.bin` | StickS3 (ESP32-S3) | Seven board demos | AOT |
| `m5stack-firmware-atoms3-aot-v0.1.3.bin` | AtomS3 and AtomS3R (ESP32-S3) | AtomS3 factory demo | AOT |

Each image holds the bootloader, the partition table and the application and
is written at offset 0x0:

```sh
esptool --chip esp32s3 write-flash 0x0 m5stack-firmware-stopwatch-aot-v0.1.3.bin
esptool --chip esp32p4 write-flash 0x0 m5stack-firmware-tab5-js-v0.1.3.bin
```

- **StopWatch, AOT image**:
  - Frame rates on the board at the 60 Hz cadence: 60 frames per second on
    a still screen, 59 with the stopwatch running (15 in 0.1.2), 58 on the
    classic watch face, 31 during launcher swipes, 21 in FluidBox.
  - A frame is repainted and sent as the windows it changed, on even
    columns, instead of full-width rows.
  - The code runs from PSRAM, which takes 2,432 KiB of the 8 MiB; 3.1 MB
    stay free on the launcher.
  - Tasks of core 0 read the touch panel and the IMU every 16 ms and the
    battery once per second; a frame of the application does not wait for
    their I2C transactions.
  - The throw of the launcher and of the rollers takes the pointer velocity
    of the same time window at any frame rate.
- **All boards**: PocketJS `484a3100`. The rasterizer starts a damage
  rectangle at the last opaque rectangle that covers it. An AOT view moves
  no row of a keyed list whose keys keep their sequence. A flat box draws a
  corner radius of 33 to 64 px from a baked disc once eight frames in a row
  asked for it (65,536 B of heap per radius, eight at most).
  `m5_pocket_app_get_stats` reads the app task's stack high-water mark when
  it is called; the app task does not scan its stack between frames.
- **Tab5, StickS3, AtomS3**: the applications and the board support are
  those of 0.1.2. The README screenshots, which the simulator renders with
  this PocketJS, are byte for byte those of 0.1.2.

**Known issues**: the StopWatch JS image does not start on the board; it
restarts in a loop, as in 0.1.2 (`programs/stopwatch/README.md`). Flash the
AOT image. Battery life of the StopWatch at the 60 Hz cadence was not
measured. The Tab5 JS frame rate (3 to 9 frames per second on the launcher
in 0.1.1) was not measured again.

**Validation**: firmware builds in CI. Device runs of local builds of the
same sources (ESP-IDF v6.1-dev-8142): a StopWatch (ESP32-S3 v0.2) in AOT
mode ran a scripted 100 s tour of the launcher, FluidBox, the four watch
faces, the stopwatch and Audio.FFT without a reset; all 1,130 frames were
sent behind a TE-timed scan and none late. The owner of the device watched
the panel at 60 Hz and reported no problem. The StopWatch JS image, Tab5,
StickS3, AtomS3 and AtomS3R were built and not run for this release.

## 0.1.2 - 2026-11-23

StopWatch: the AOT image starts on the board, draws 15 to 30 frames per
second and shows whole frames. PocketJS `98ead939`; ESP-IDF 6.2, as in 0.1.1.

| Image | Board | Application | Mode |
| --- | --- | --- | --- |
| `m5stack-firmware-tab5-js-v0.1.2.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | JS |
| `m5stack-firmware-tab5-aot-v0.1.2.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | AOT |
| `m5stack-firmware-stopwatch-js-v0.1.2.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | JS |
| `m5stack-firmware-stopwatch-aot-v0.1.2.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | AOT |
| `m5stack-firmware-sticks3-js-v0.1.2.bin` | StickS3 (ESP32-S3) | Seven board demos | JS |
| `m5stack-firmware-sticks3-aot-v0.1.2.bin` | StickS3 (ESP32-S3) | Seven board demos | AOT |
| `m5stack-firmware-atoms3-aot-v0.1.2.bin` | AtomS3 and AtomS3R (ESP32-S3) | AtomS3 factory demo | AOT |

Each image holds the bootloader, the partition table and the application and
is written at offset 0x0:

```sh
esptool --chip esp32s3 write-flash 0x0 m5stack-firmware-stopwatch-aot-v0.1.2.bin
esptool --chip esp32p4 write-flash 0x0 m5stack-firmware-tab5-js-v0.1.2.bin
```

- **StopWatch, AOT image**:
  - It starts on the board. The 0.1.0 and 0.1.1 AOT images restart in a
    loop.
  - Frame rates on the board: 30 frames per second on a still screen, 19
    during launcher swipes, 15 on screens that repaint the whole panel
    (FluidBox, the running stopwatch).
  - The panel shows whole frames. The firmware renders a frame into PSRAM
    and sends its rows behind the panel's scan, timed by the TE signal.
  - The speaker is silent between sounds, and the vibration motor stops
    after its duration.
  - Text under a turned transform turns: the lucky wheel's sector labels,
    and the date and weekday of a turned watch face.
  - The watch face follows gravity in turns: a turn starts when the face is
    3° off and ends within 1°.
- **All boards**: PocketJS `98ead939`. The AOT images build the PocketJS
  crates that run in every frame (rasterizer, retained core, damage tracker,
  presenter, driver adapters) for speed. Applications declare their font
  faces with `app.fonts` in `pocket.host.config.ts`; the atlases are the
  same.
- **Tab5, StickS3, AtomS3**: the applications and the board support are
  those of 0.1.1. With the PocketJS rasterizer of this version the edge
  pixels of turned shapes differ (40 pixels of the StickS3 dice screen and 4
  of the Tab5 RTC window in the README screenshots).
- **Development**: a frame profiler for the application host
  (`CONFIG_M5_POCKET_APP_PROFILE`, `tools/profile.py`): stage times of every
  tick, scripted touch and keys over USB Serial/JTAG, and a program counter
  sampler (`docs/APP-HOST.md`).

**Known issues**: the StopWatch JS image does not start on the board; it
restarts in a loop, as the 0.1.0 and 0.1.1 JS images do
(`programs/stopwatch/README.md`). Flash the AOT image. The Tab5 JS frame rate
(3 to 9 frames per second on the launcher in 0.1.1) was not measured again.

**Validation**: firmware builds in CI. Device runs of local builds of the
same sources (ESP-IDF v6.1-dev-8142): a StopWatch (ESP32-S3 v0.2) in AOT
mode ran a scripted 100 s tour of the launcher, FluidBox, the four watch
faces, the stopwatch and Audio.FFT without a reset; all 582 frames were sent
behind a TE-timed scan, and the panel was watched and showed no tear. The
StopWatch JS build was run and does not start. Tab5, StickS3, AtomS3 and
AtomS3R were built and not run for this release.

## 0.1.1 - 2026-11-23

Tab5 fixes and AtomS3R support. PocketJS `2c9fcf30` and ESP-IDF 6.2, as in
0.1.0. The repository is now `pocket-nexus/m5stack-firmware`, and the images
are named `m5stack-firmware-<board>-<mode>-v<version>.bin`.

| Image | Board | Application | Mode |
| --- | --- | --- | --- |
| `m5stack-firmware-tab5-js-v0.1.1.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | JS |
| `m5stack-firmware-tab5-aot-v0.1.1.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | AOT |
| `m5stack-firmware-stopwatch-js-v0.1.1.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | JS |
| `m5stack-firmware-stopwatch-aot-v0.1.1.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | AOT |
| `m5stack-firmware-sticks3-js-v0.1.1.bin` | StickS3 (ESP32-S3) | Seven board demos | JS |
| `m5stack-firmware-sticks3-aot-v0.1.1.bin` | StickS3 (ESP32-S3) | Seven board demos | AOT |
| `m5stack-firmware-atoms3-aot-v0.1.1.bin` | AtomS3 and AtomS3R (ESP32-S3) | AtomS3 factory demo | AOT |

Each image holds the bootloader, the partition table and the application and
is written at offset 0x0:

```sh
esptool --chip esp32p4 write-flash 0x0 m5stack-firmware-tab5-js-v0.1.1.bin
esptool --chip esp32s3 write-flash 0x0 m5stack-firmware-atoms3-aot-v0.1.1.bin
```

- **Tab5**: the 0.1.0 images abort in `app_main` on a Tab5 and restart in a
  loop (`pocket_display_esp_dpi_create failed`, `ESP_ERR_NO_MEM`). Three
  fixes:
  - The raster density comes from the host contract. 0.1.0 divided the
    bundle's physical height by the logical width; the bundle records the
    landscape 1280x720, so the density came out as 1, the picture was
    configured as 640x360 and touch coordinates were not scaled.
  - The two staging strips are 40 raster rows (102,400 bytes each). Each
    strip is one internal DMA block, and the second 48-row strip (122,880
    bytes) did not fit the largest free block left (118,784 bytes).
  - AOT mode boots the application at its logical viewport (640x360). 0.1.0
    passed the panel size (720x1280), which the runtime rejected with
    `ProfileMismatch`. The other boards' logical viewport equals their panel
    size, so they are not affected.
- **AtomS3R**: the AtomS3 image runs on the AtomS3R. The board support reads
  the chip package from eFuse at boot and drives the AtomS3R's panel pins,
  its I2C backlight, the BMI270 IMU (in M5Unified's AtomS3R axis order) and
  the IR LED on GPIO47. AtomS3R units with an ST7735S panel are not
  supported (`ESP_ERR_NOT_SUPPORTED`); the AtomS3R's PSRAM is not enabled.
- **StopWatch, StickS3**: unchanged apart from the image names.
- **Documentation**: the README describes the boards, the JS and AOT modes,
  release images and building from source, with simulator screenshots of
  every application (`tools/screenshots.ts`); `README.zh-CN.md` is the
  Simplified Chinese version.

**Known issue**: in JS mode the Tab5 launcher draws 3 to 9 frames per second;
AOT mode draws 30.

**Validation**: firmware builds in CI. Device runs of local builds of the
same sources (ESP-IDF v6.1-dev-8142): a Tab5 (ESP32-P4 v1.3, ST7121 panel)
boots and runs in JS and AOT mode with no frame errors (serial log); an
AtomS3R (ESP32-S3-PICO-1 v0.2, GC9107 panel) boots in AOT mode, and the
panel, backlight, button and IMU were checked on the board.

## 0.1.0 - 2026-11-23

First release. PocketJS `2c9fcf30` (`v0.17.0` and later fixes on
`pocket-nexus/pocketjs` main), ESP-IDF 6.2.

| Image | Board | Application | Mode |
| --- | --- | --- | --- |
| `m5stack-firmware-tab5-js-v0.1.0.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | JS |
| `m5stack-firmware-tab5-aot-v0.1.0.bin` | Tab5 (ESP32-P4) | Tab5 factory demo | AOT |
| `m5stack-firmware-stopwatch-js-v0.1.0.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | JS |
| `m5stack-firmware-stopwatch-aot-v0.1.0.bin` | StopWatch C152 (ESP32-S3) | StopWatch factory demo | AOT |
| `m5stack-firmware-sticks3-js-v0.1.0.bin` | StickS3 (ESP32-S3) | Seven board demos | JS |
| `m5stack-firmware-sticks3-aot-v0.1.0.bin` | StickS3 (ESP32-S3) | Seven board demos | AOT |
| `m5stack-firmware-atoms3-aot-v0.1.0.bin` | AtomS3 (ESP32-S3, no PSRAM) | AtomS3 factory demo | AOT |

Each image holds the bootloader, the partition table and the application and
is written at offset 0x0:

```sh
esptool --chip esp32s3 write-flash 0x0 m5stack-firmware-stopwatch-js-v0.1.0.bin
esptool --chip esp32p4 write-flash 0x0 m5stack-firmware-tab5-js-v0.1.0.bin
```

- **Tab5**: startup animation, launcher and the 15 panel windows of
  `M5Tab5-UserDemo`; board support for both panel variants, touch, audio,
  IMU, RTC, power monitor, IO expanders, microSD and RS485. No camera
  preview, Wi-Fi access point or USB-A host.
- **StopWatch**: the ten applications of `M5StopWatch-UserDemo`, including
  the badge editor (access point, captive portal, the factory phone page,
  six JPEG slots); watch faces turn as one panel with the accelerometer.
- **StickS3**: marble maze, dice, reaction timer, snake, spirit level, sound
  meter and metronome over the IMU, microphone, speaker and keys.
- **AtomS3**: the seven functions of `M5AtomS3-UserDemo` with the factory's
  efont glyphs; AOT only, because the QuickJS guest does not fit the 512 KB
  of SRAM.
- **Shared host** `m5_pocket_app`: JS and AOT modes selected by Kconfig, the
  device bridge (`device.ts`), in-place AOT fonts and images, per-app fonts
  through `tools/pocket`.

Each application README lists the factory screens it reproduces and every
known difference from the factory firmware.

**Validation**: firmware builds in CI and host-side checks (format, fonts,
simulator renders); the AOT AtomS3 and aot-smoke images ran under QEMU.
