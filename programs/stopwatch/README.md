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
  (`M5_POCKET_DISPLAY_ESP_LCD` with staged presentation: two RGB565 frames
  of 466 × 466 px in PSRAM, 434,312 bytes each; the display driver's
  presenter task on core 0 sends the rows a frame changed, 32 at a time
  (29,824 bytes, under the ESP32-S3 SPI DMA limit, byte-swapped), behind the
  panel's scan as timed by the TE signal, so that one refresh of the panel
  shows one frame), the CST820
  contact as `touch` (a task of core 0 reads it over I2C every 16 ms, and
  the app task's input poll takes the latest sample),
  KEYA/KEYB as `BTN.LTRIGGER`/`BTN.RTRIGGER`, a 6 MiB
  QuickJS heap in PSRAM and the app task on core 1. In JS mode its stack is
  64 KiB of internal RAM (the request hook writes NVS). In AOT mode it is
  256 KiB of PSRAM: the compiled application uses 69,360 bytes of stack, and
  internal RAM has no free block of that size when the task is created. The
  host then runs the request hook on its
  helper task (12 KiB of internal RAM,
  `CONFIG_M5_POCKET_APP_INTERNAL_STACK`), and `badge_texture_update` passes
  the badge decode, which reads FAT, to it with `m5_pocket_app_run_internal`
  (docs/APP-HOST.md, "Flash operations from the hooks").
- `main/services.c` is the board side of the device bridge. Before every
  frame it publishes `uptimeMs`, the wall clock (system time, set from the
  RX8130CE at boot) and `keyA`/`keyB`. A task of core 0 reads the IMU every
  16 ms and the battery once per second and publishes them: their I2C
  transactions do not run on the app task. At boot it publishes the
  settings, the alarms and the badge state. It handles the application's
  requests: tones (factory sine with a 200-sample fade) and the factory boot
  sound on an audio task, vibration,
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

Add `sdkconfig.profile` for the frame profiler (docs/APP-HOST.md, "Frame
profiler"), in either mode:

```sh
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot;sdkconfig.profile" set-target esp32s3 build
python ../../tools/profile.py run <port> run.log --reset @idle 5s
```

**In AOT mode the code runs from PSRAM; read-only data stays in flash.**
`sdkconfig.aot` sets `CONFIG_SPIRAM_FETCH_INSTRUCTIONS`: the boot copies the
2,419,988 bytes of code into PSRAM and maps them there, which takes
2,368 KiB of the 8 MiB (37 pages of 64 KiB). A frame of the compiled
application runs through more code than the 32 KiB instruction cache holds,
and a cache miss then reads the octal PSRAM (eight data lines at 80 MHz)
instead of the QIO flash (four). The 8,132,420 bytes of read-only data (the
pictures) stay in flash: `CONFIG_SPIRAM_RODATA` is off. JS mode keeps its
code in flash and the PSRAM for the QuickJS heap.

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
`mount` calls the generated `load_images_in_place` (docs/APP-HOST.md
"Images") instead of the generated `load_images`, which copies every
picture into the heap at
mount (7,167,252 bytes of IMG entries with the 116 big-number tiles and
the glyph images): the IMG entries MicroTS bakes in the formats
`images.json` selects (RGB565, RGBA4444 or RGBA8888, as in the JS bundle)
stay in flash and a texture is decoded when first drawn; `mount` bounds the
decoded copies with a 2 MiB texture budget.

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

| Mode | `m5stack_stopwatch.bin` | Free in the 12 MiB app partition | Static DIRAM | Validated by |
| --- | --- | --- | --- | --- |
| JS | 11,401,616 bytes (bundle about 8.9 MB: 3.7 MB big-number artwork, 1.8 MB factory glyph images) | 9 % | 153,901 of 341,760 bytes | firmware build; does not start on the board |
| AOT | 10,674,384 bytes | 15 % | 152,461 of 341,760 bytes | device run |

AOT pictures stay in flash and are decoded under the runtime's texture
budget when drawn, so the 8 MiB PSRAM holds only the textures on screen
(at most about 1 MB for the launcher's three icon textures, the guide or
four big-number tiles) plus the code, the runtime heap and the two display
frames: 3.2 MB of PSRAM stay free on the launcher.

**Device run (AOT).** M5Stack StopWatch, ESP32-S3 revision v0.2, ESP-IDF
v6.1-dev-8142, PocketJS `98ead939`. The firmware boots to the launcher and
runs. The numbers are from the profiler build (`sdkconfig.profile`) with
input scripted through its console (`tools/profile.py run`): a 100 s tour of
the launcher, FluidBox, the four watch faces, the stopwatch and Audio.FFT,
without a reset and without a failed tick.

| Screen | Frames per second | Tick |
| --- | --- | --- |
| Launcher, still | 30.0 | 3.9 ms |
| Launcher, swipes | 19.7 | 27.1 ms |
| FluidBox | 14.9 | 43.7 ms |
| Watch face, classic, watch lying flat | 29.0 | 16.0 ms |
| Stopwatch, stopped | 30.0 | 13.8 ms |
| Stopwatch, running | 14.9 | 38.9 ms |

A screen that repaints the whole panel in every frame (FluidBox, the running
stopwatch, a watch face while it turns) runs at 15 frames per second.

The presenter sent the 613 frames of the tour behind a TE-timed scan
(`unsynced=0`), and no transfer started late for the next scan (`late=0`);
the longest time from a frame's flush to its last transfer was 41.9 ms. The
device's owner saw no tear on the panel with this presentation; the picture
was not captured. The app task used 69,528 of 262,144 stack bytes and the
helper task 1,460 of 12,288.

**JS mode does not start on the board.** The firmware restarts in a loop:

- The runtime refuses a package larger than its `heap_limit`
  (`boot failed: Package(TooLarge)`): the bundle has 8,968,680 bytes and
  `main/main.c` sets the limit to 4 MiB.
- With a limit of 12 MiB the package loads and QuickJS stops after about
  6.5 s with `SyntaxError: invalid assignment left-hand side`.

The JS bundle runs in the PocketJS simulator (`tools/screenshots.ts`). On
the board, use AOT mode.
