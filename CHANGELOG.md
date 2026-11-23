# Changelog

Each version is a git tag `v<version>`. The Firmware workflow publishes the
section of that version as the release notes, with one merged flash image per
board and mode.

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
