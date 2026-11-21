# aot-smoke

A minimal ESP-IDF program that adopts `m5_pocket_app` the way a board program
does, without a board: a transfer-only display (two 16-row RGB565 strips whose
flushes are hashed, `main/transfer_display.c`) and scripted input. It proves
that the host, the device bridge and both application modes build for
ESP32-S3 and ESP32-P4.

The app (`app/`) is a button counter (`ActionHandler` on CROSS), a
touch-pressable focusable view and `device.ts` fields shown as text: battery,
charging, clock text, a spectrum array length, an event counter and the raw
pointer state. Pressing the counter and tapping the view queue the requests
`tone 880 <n>` and `vibrate 30`.

`main/main.c` runs 120 frames: CROSS is held at frames 10–11 and 30–31, a tap
lands on the view at frames 50–53, the bridge receives numbers, text and an
array every frame and the event `alarm` at frame 40. It then stops the app and
logs `PASS`/`FAIL` with the frame statistics, the request count and the
internal heap. On ESP32-P4 the PPA renderer is registered.

## Build

```sh
. $IDF_PATH/export.sh
cd tests/aot-smoke

# JS mode (default)
idf.py -B build-js-s3 -DSDKCONFIG=build-js-s3/sdkconfig set-target esp32s3 build
idf.py -B build-js-p4 -DSDKCONFIG=build-js-p4/sdkconfig set-target esp32p4 build
```

`sdkconfig.defaults.esp32s3` enables PSRAM for the JS build;
`sdkconfig.aot` selects AOT mode and `sdkconfig.nopsram` disables PSRAM (the
AtomS3 configuration). They are added through `SDKCONFIG_DEFAULTS`; AOT
commands are listed with the AOT results.

## Results

Firmware builds only. Nothing was flashed or run; no M5Stack hardware was
connected. ESP-IDF v6.1-dev-8142 (6.2 tools), Bun 1.4.2, rustup 1.98.1 (P4
runtime archive) and `esp` 1.97 nightly (S3 runtime archive), macOS arm64.

| Mode | Target | Result | App image | DIRAM used (static) |
| --- | --- | --- | --- | --- |
| JS | esp32s3 (PSRAM) | builds | 1,638,880 B | 56,215 B (16.45 %) |
| JS | esp32p4 (PSRAM) | builds | 1,795,744 B | 84,852 B (14.82 %) |

The component manager writes `dependencies.lock` for the last target and mode
built; this test does not keep it (`.gitignore`).
