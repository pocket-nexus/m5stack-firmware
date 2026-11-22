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
internal heap. On ESP32-P4 two PPA lanes are registered
(`display.ppa_lanes = 2`, split on 2-row multiples).

`aot/` is the per-app AOT crate, the template for `programs/<board>/aot/`.

## Build

```sh
. $IDF_PATH/export.sh
cd tests/aot-smoke

# JS mode (default)
idf.py -B build-js-s3 -DSDKCONFIG=build-js-s3/sdkconfig set-target esp32s3 build
idf.py -B build-js-p4 -DSDKCONFIG=build-js-p4/sdkconfig set-target esp32p4 build

# AOT mode; esp32s3 without PSRAM, as on the AtomS3
idf.py -B build-aot-s3 -DSDKCONFIG=build-aot-s3/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot;sdkconfig.nopsram" set-target esp32s3 build
idf.py -B build-aot-p4 -DSDKCONFIG=build-aot-p4/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot" set-target esp32p4 build
```

`sdkconfig.defaults.esp32s3` enables PSRAM; `sdkconfig.aot` selects AOT mode;
`sdkconfig.nopsram`, added last, disables PSRAM. AOT builds need rustup with
the `esp` toolchain (esp32s3) or the 1.98.1 toolchain with
`riscv32imafc-unknown-none-elf` (esp32p4); the first AOT build also builds the
MicroTS layout oracle (`bun tools/wasm.ts` in the submodule, wasm32 target).

Run the AOT esp32s3 image under Espressif's QEMU
(`idf_tools.py install qemu-xtensa`):

```sh
cd build-aot-s3
python -m esptool --chip esp32s3 merge-bin --fill-flash-size 8MB -o flash.bin @flash_args
qemu-system-xtensa -nographic -machine esp32s3 -drive file=flash.bin,if=mtd,format=raw \
    -serial mon:stdio -monitor none
```

## Results

ESP-IDF v6.1-dev-8142 (6.2 tools), Bun 1.4.2, rustup 1.98.1 (P4) and `esp`
1.97 nightly (S3), macOS arm64. No M5Stack hardware was connected; nothing
was flashed.

| Mode | Target | Build | App image | Flash code + data | DIRAM static (.data + .bss) |
| --- | --- | --- | --- | --- | --- |
| JS | esp32s3, PSRAM | links | 1,646,320 B | 1,107,700 + 473,044 B | 20,044 B |
| JS | esp32p4, PSRAM | links | 1,803,680 B | 1,729,556 B | 22,344 B |
| AOT | esp32s3, no PSRAM | links, QEMU PASS | 699,056 B | 478,096 + 161,904 B | 18,860 B |
| AOT | esp32p4, PSRAM | links | 806,384 B | 734,222 B | 21,160 B |

The Rust library before the link: 5.6 MB (`xtensa-esp32s3-none-elf`) and
12.3 MB (`riscv32imafc`, after removing 36 soft-float compiler-rt objects);
the linker keeps what the firmware uses.

QEMU run of the AOT esp32s3 image without PSRAM (`PASS mode=aot frames=120
errors=0 requests=3`): the two CROSS presses produced `tone 880 …`, the tap
produced `vibrate 30`, the display hashed 49 strip flushes, the longest frame
took 8.7 ms (emulated, not a timing measurement). Internal RAM: 377,872 B
free after the display buffers (2 × 7,680 B) were allocated, 273,864 B free
while running, 270,524 B minimum: host, runtime, app and the 64 KiB app task
stack took at most 107,348 B. The app task used at most 19,528 B of its stack.
The frame and flush counts are sampled by `main.c`'s 50 ms polling loop and
vary by a few between runs.

The JS image does not reach `app_main` under QEMU (USB-Serial-JTAG
connection monitor, linked through VFS, waits for a BBPLL calibration QEMU
does not emulate); QEMU has no ESP32-P4 machine.

The component manager writes `dependencies.lock` for the last target and mode
built; this test does not keep it (`.gitignore`).
