# programs/stopwatch

ESP-IDF firmware for the M5Stack StopWatch (C152). It embeds
`apps/stopwatch-factory`, the recreation of the M5StopWatch-UserDemo factory
firmware.

## Board

| Item | Value |
| --- | --- |
| SoC | ESP32-S3R8, 240 MHz, 8 MB octal PSRAM, 16 MB flash |
| Display | 1.75" round AMOLED, 466 × 466 visible, CO5300 on QSPI (SPI2, 80 MHz); CS 39, SCLK 40, D0–D3 41/42/46/45, TE 38 |
| Touch | CST820 at 0x15 on the internal I2C bus, INT GPIO13, reset on IOE1 pin 4 |
| Keys | KEYA (yellow) GPIO2, KEYB (blue) GPIO1, active low; A+B held 500 ms = home |
| I2C | SDA 47, SCL 48: PM1 0x6E, IOE1 0x4F/0x6F, BMI270 0x68, RX8130CE, ES8311 |
| Power | M5PM1 PMIC (battery, charging, power hold); M5IOE1 expander (display/touch power and reset, speaker amplifier, vibration motor PWM on pin 9) |
| Audio | ES8311, I2S MCLK 18, BCLK 17, LRCK 15, DOUT 21, DIN 16; amplifier GPIO14 + IOE1 pin 10 |

`platforms/esp-idf/m5_bsp_stopwatch` brings the board up and returns the
panel, panel IO and touch handles.

## Composition

The program uses the shared application host
(`platforms/esp-idf/m5_pocket_app`, [docs/APP-HOST.md](../../docs/APP-HOST.md)):
`CMakeLists.txt` includes its `components.cmake`, and `main/CMakeLists.txt`
embeds `apps/stopwatch-factory` with `m5_pocket_app()`.

- `main/main.c` initialises NVS and the board (`m5_stopwatch_init`), starts
  the services and calls `m5_pocket_app_start` with the CO5300 panel
  (`M5_POCKET_DISPLAY_ESP_LCD`, two RGB565 strips of 466 × 32 px, 29,824
  bytes each, under the ESP32-S3 SPI DMA limit, byte-swapped), the CST820
  contact as `touch`, KEYA/KEYB as `BTN.LTRIGGER`/`BTN.RTRIGGER`, a 6 MiB
  QuickJS heap in PSRAM and a 64 KiB internal-RAM app task on core 1
  (internal because the request hook writes NVS).
- `main/services.c` is the board side of the device bridge. Before every
  frame it publishes `uptimeMs`, the wall clock (system time, set from the
  RX8130CE at boot), `keyA`/`keyB`, the IMU and, once per second, the
  battery. At boot it publishes the settings, the alarms and the badge
  state. It handles the application's requests: tones (factory sine with a
  200-sample fade) and the factory boot sound on an audio task, vibration,
  brightness, volume, button settings, RTC writes and alarm storage, with
  the factory NVS keys (`system`: `bl_lev`, `spk_vol`, `btn_sfx`,
  `btn_vibrate`, `wifi_enabled`, `launch_count`; `alarm`: `storage`).
- `main/spectrum.c` computes the factory microphone spectrum (512-point
  FFT every 256 samples, 20 bands, peak frequency) on a microphone task
  while the Audio.FFT page requests it.
- `main/assets/boot_sfx.bin` is the factory boot sound (M5StopWatch-UserDemo
  `main/assets/sfx/boot_sfx.bin`, MIT): 16-bit mono PCM at 44.1 kHz.

`pocket.host.json` is the host contract (`m5stack-stopwatch`: 466 × 466,
native presentation, cadence 30, `input.buttons`, `input.touch`,
`text.glyphs.baked`). The application's host config is
`apps/stopwatch-factory/pocket.host.config.ts`.

## Badge service

`main/badge.c` (`main/badge.h`) is the board side of the Badge app. It is a
C port of the factory badge code; the factory behaviour is kept unless
listed under "Differences".

- Storage: the `storage` partition is mounted with wear levelling
  (`esp_vfs_fat_spiflash_mount_rw_wl`, 4 KiB sectors, at most 4 open files,
  formatted when the mount fails) at `/spiflash`. Six slots, 0 to 5:
  `/spiflash/badge/slot_N.jpg` (image), `/spiflash/badge/slot_N.meta`
  (extension text, `jpg`) and `/spiflash/badge/active_slot.txt` (active
  slot number as text). An upload is written to `slot_N.jpg.tmp` and
  renamed.
- Active slot: the stored slot (0 when the file is missing or out of range)
  if its image is readable, else the first occupied slot, which is then
  stored; none when every slot is empty. This runs at boot and after the
  editor closes. Deleting the active slot stores the first remaining
  occupied slot, or removes `active_slot.txt`.
- Bridge values: `badgeSlots` (6 numbers, 1 = occupied), `badgeActive`
  (slot, -1 for none), `apSsid`, `apMessage` (the factory status text:
  `Connect to Wi-Fi: <ssid>\nAnd open page:\nhttp://192.168.4.1`, the
  captive-probe notice, error texts, empty when idle) and the event
  `badgeClosed` when the phone presses Close.
- Access point: open network `M5StopWatch-XXXX` (XXXX = last two SoftAP MAC
  bytes, upper-case hex), channel 1, at most 4 stations, 192.168.4.1/24,
  power save off. The DHCP server offers 192.168.4.1 as DNS server. A UDP
  DNS server on port 53 answers every query with the A record 192.168.4.1
  (TTL 28 s).
- HTTP (`esp_http_server`, wildcard URI matching, 15 s socket timeouts):

  | Route | Result |
  | --- | --- |
  | `GET /` | editor page (`text/html; charset=utf-8`) |
  | `GET /badge/state` | `{"apSsid","apUrl","slotCount":6,"activeSlot","slots":[{"slot","hasImage","isActive","imageUrl"}]}` |
  | `GET /badge/image?slot=N` | stored image (`image/jpeg`); 404 `badge image not found` / `invalid badge slot` |
  | `POST /badge/active?slot=N` | `{"status":"ok","message":"active slot updated"}`; 400 on an empty or invalid slot |
  | `DELETE /badge/image?slot=N` | `{"status":"ok","message":"badge image deleted"}`; 400 `badge image not found` |
  | `POST /upload?slot=N` | body = image, at most 2 MiB (400 `invalid upload size`); only JPEG, decided by the `X-File-Name` extension, else the `Content-Type` (400 `only jpg images are supported`); stores the image and makes the slot active |
  | `POST /close` | `closing`; then stops the HTTP and DNS servers, Wi-Fi and the AP interface |
  | `GET` `/hotspot-detect.html`, `/generate_204*`, `/mobile/status.php`, `/check_network_status.txt`, `/ncsi.txt`, `/fwlink/`, `/connectivity-check.html`, `/success.txt`, `/portal.html`, `/library/test/success.html` | 302 to `http://192.168.4.1/?_=<µs since boot>` |

  A missing `slot` query parameter gives 400 `missing slot`.
- Decode: `stopwatch_badge_take_image` decodes the active slot on the app
  task with `espressif/esp_jpeg` 1.3.2 (TJpgDec, baseline JPEG only). An
  image wider or taller than 466 px is decoded at 1/2, 1/4 or 1/8. The
  pixels are centred on the smallest square canvas of 64, 128, 256 or 512
  px, black elsewhere, as little-endian `(b5 << 11) | (g6 << 5) | r5` in
  PSRAM.

Differences from the factory firmware:

- The factory blocks the Badge app while the editor runs; here the
  service runs beside the app and `stopwatch_badge_edit(false)` also stops
  it.
- The factory keeps Wi-Fi initialised after the editor closes and resumes
  its station mode; this firmware has no station mode and calls
  `esp_wifi_deinit` to return the Wi-Fi memory.
- The factory leaves the AP channel at 0 in `wifi_config_t`; this port sets
  channel 1.
- The factory DNS server appends its answer after the whole query, after
  any additional (EDNS) record; this port answers after the question and
  clears the authority and additional counts.
- The factory serves the page with the terminating NUL byte that
  `EMBED_TXTFILES` adds; this port omits it.
- PNG slots (never written by the factory upload, but recognised by its
  file rules) are listed as occupied but not decoded.

Sources taken from
[M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo)
at commit `81099c784991dc77f522de1a5bf62ec5de9ae0b5` (MIT, M5Stack):
`main/hal/hal_badge.cpp`, `main/hal/utils/config_ap/config_ap.cpp` and
`config_ap.h`, `main/hal/utils/wear_levelling/wear_levelling.c` (ported to
`main/badge.c`), and `main/hal/utils/config_ap/assets/badge_config_ap.html`
(copied unchanged to `main/assets/badge_config_ap.html`). The DNS answer
format follows `dns_server.cc` of `78/esp-wifi-connect` 3.1.3, the
component the factory uses.

## Build

Requirements: ESP-IDF `>=6.0,<6.3`, Bun, the Espressif Rust toolchain
(`rustup` toolchain `esp` with `xtensa-esp32s3-none-elf`) and, for AOT
mode and AOT checks, the PocketJS layout oracle (`bun tools/wasm.ts` inside
`third_party/pocketjs`; the output is ignored).

```sh
. $IDF_PATH/export.sh
cd programs/stopwatch

# JS mode (default): bundle in the QuickJS guest
idf.py build
idf.py -p <port> flash monitor

# AOT mode: MicroTS-compiled Rust library (programs/stopwatch/aot)
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot" set-target esp32s3 build
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig -p <port> flash monitor
```

The two modes need different managed components (AOT links no QuickJS), so
JS mode keeps `dependencies.lock` and AOT mode `dependencies.aot.lock`
(selected in `CMakeLists.txt`). Both share `managed_components/`: run the
two builds one after the other, not at the same time.

`components.cmake` builds the JS-mode runtime archive from the submodule
with `~/.rustup/toolchains/esp/bin/cargo` and runs the Pocket CLI through
`tools/pocket`. In AOT mode `m5_pocket_app()` compiles the application with
`pocket compile --aot` and Cargo builds `aot/` with the `esp` toolchain.

`aot/` is the program's AOT crate (from `tests/aot-smoke/aot`):
`src/lib.rs` writes every board value into the `device.ts` fields
(`device_fields!`, the counterpart of the `hostNumber`/`hostText`/
`hostArray` switches; `uptimeMs` also sets `bridged`, the `badgeClosed`
event counts, `badgeTexture` and `badgeCanvas` arrive like any field).
`build.rs` uses the shared image loader (`m5_pocket_aot_build::emit_images`,
docs/APP-HOST.md "Images") instead of the generated `load_images`, which
would embed every picture as RGBA8888 and copy it into the heap at mount
(about 10 MB of flash and heap with the 116 big-number tiles): each picture
is stored in the format `images.json` selects (RGB565, RGBA4444 or
RGBA8888, as in the JS bundle) and registered with `ImageStorage::Flash`, so
a texture is read from flash when first drawn; `mount` bounds the decoded
copies with a 2 MiB texture budget. (The program's own in-place loader that
preceded it produced the same formats and storage, so it was dropped.)

The badge picture is handed to PocketJS in one function,
`badge_texture_update` in `main/services.c`, the same in both modes:
`m5_pocket_app_set_texture` uploads it and binds `images/badge_slot.png`
(docs/APP-HOST.md, "Program textures"), and the published `badgeTexture`
handle makes the view rebind it.

`main` and `m5_bsp_stopwatch` call `m5_strict_warnings()` after their
`-Wall -Wextra -Werror` (IDF relaxes `-Wextra`); both build without
warnings.

Check the application against the MicroTS subset without building firmware:

```sh
bun run pocket compile --config apps/stopwatch-factory/pocket.host.config.ts --aot --out /tmp/stopwatch-aot
```

## Partitions

| Name | Size |
| --- | --- |
| nvs | 24 KiB |
| phy_init | 4 KiB |
| factory | 12 MiB |
| storage (FAT with wear levelling, badge images, `/spiflash`) | 3,968 KiB |

## Validation

Firmware builds only; no device run (ESP-IDF 6.x, esp32s3).

| Mode | `m5stack_stopwatch.bin` | Free in the 12 MiB app partition | Static DIRAM |
| --- | --- | --- | --- |
| JS | 11,347,424 bytes (bundle about 8.9 MB: 3.7 MB big-number artwork, 1.8 MB factory glyph images) | 10 % | 154,905 of 341,760 bytes |
| AOT | 10,627,280 bytes | 16 % | 152,721 of 341,760 bytes |

AOT pictures stay in flash and are decoded under the runtime's texture
budget when drawn, so the 8 MiB PSRAM holds only the textures on screen (estimate:
(at most about 1 MB for the launcher's three icon textures, the guide or
four big-number tiles) plus the runtime heap).
