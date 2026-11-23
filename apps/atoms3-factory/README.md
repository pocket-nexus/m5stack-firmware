# atoms3-factory

PocketJS recreation of the M5Stack AtomS3 factory demo,
[m5stack/M5AtomS3-UserDemo](https://github.com/m5stack/M5AtomS3-UserDemo)
at `8eaf37b3ccf9a8d7c4bf4c2dea0e7925ede6328d` (`src/main.cpp`,
`src/img_res.c`). It runs in `programs/atoms3` on the AtomS3 and the
AtomS3R, in AOT mode only (the QuickJS guest with this bundle does not fit
in the AtomS3's internal RAM).

## Files

| File | Content |
| --- | --- |
| `pocket.host.config.ts` | Host config: `app` section (`framework: "solid"`, `aot: true`, `model: "compiled"`, 128×128, 30 Hz); host contract `../../programs/atoms3/pocket.host.json` |
| `main.tsx` | JS-mode entry: `mount()` and the device bridge (`connectDevice`) |
| `app.tsx`, `Boot.tsx`, `Menu.tsx`, `RandomText.tsx` | Views |
| `app.ts` | Root model: the factory's `loop()`, one function object per screen, the IMU filter |
| `device.ts` | Device state module: board values, requests, and the simulated board used without hardware |
| `img/*.png` | The factory artwork, extracted byte-for-byte from `src/img_res.c` (128×128 RGB PNG; MIT, Copyright (c) 2023 M5Stack); `not_found.png` and `top5_list.png` are the factory's efont 24 and efont 14 text rendered from the efont bitmaps |
| `images.json` | Bakes every artwork as RGB565 (`psm: 0`), 32 KiB per image |
| `fonts.json` | Bakes printable ASCII (U+0020–U+007E) so SSIDs and bytes render |
| `pocket.fonts.json`, `fonts/` | The factory's efont faces as pixel-outline TrueType: regular = efont 16, bold = efont 16 bold, mono = efont 12 ([Fonts](#fonts)) |

The sources stay inside the MicroTS admission subset:
`bun run pocket compile --config apps/atoms3-factory/pocket.host.config.ts --aot --out <dir>`
passes.

## Navigation

As in the factory, with the one button under the screen:

- Boot: eight coloured rings grow from the bottom-left corner, one every
  200 ms, then 500 ms later the menu.
- Menu: a click shows the next function's artwork (Wi-Fi, I2C, UART, PWM,
  ADC, IR, IMU, then Wi-Fi again).
- A hold (500 ms, M5Unified `wasHold`) enters the shown function; another
  hold leaves it and shows the menu artwork again.
- Inside a function a click is the function's own action (below).

The program turns the button into PocketJS buttons with the M5Unified
click/hold state machine: a click is one frame of `BTN.CIRCLE`, a hold one
frame of `BTN.CROSS`. On a desktop or web host, those two buttons stand in
for click and hold.

## Screens reproduced

| Factory function | Screen | Click inside | Hardware (program) |
| --- | --- | --- | --- |
| `func_wifi_t` | Menu artwork top 30 px, black 128×98 canvas: "Scaning...", then every 500 ms one line `N. SSID RSSI` of the five strongest networks, two blank lines and "Top 5 list:", scrolling upward | pause / resume the listing | `esp_wifi` STA scan |
| `func_i2c_t` | Canvas: "Scaning...", then a white grid with `N. 0xAA` per device in random colours, or "Not found" in red | rescan | Port A I2C (SDA G2, SCL G1), addresses 1–126, at most 6 |
| `func_uart_t` | `uart_mon_01..04` artwork; last Grove→USB byte at (93, 33) and USB→Grove byte at (93, 43) in random colours | next mode: 9600 / 115200 baud, RX/TX on G2/G1 or G1/G2 | UART1 on Port A bridged to USB Serial/JTAG |
| `func_pwm_t` | `io_pwm_01..04` artwork, "F: 1Khz", duty `0xNN` | next mode (both pins, both pins, G1 only, G2 only); duty 0x0F in mode 0, 0 otherwise | LEDC 1 kHz 8-bit on G1/G2; each USB byte sets the duty |
| `func_adc_t` | `io_adc_01` artwork, G1 and G2 voltages `N.NV` every 90 ms | none | ADC1 channels 0/1, mean of 32 reads |
| `func_ir_t` | `ir_send_01/02` artwork, sent command `0xNN` in purple on a white box | next mode: USB byte → IR, or an incrementing command every second | NEC (extended) on GPIO4, address = low 16 bits of the MAC |
| `func_imu_t` | Canvas: two circles and a cross in #CCCC33 and a ball that follows roll and pitch | none | MPU6886 (AtomS3) or BMI270 (AtomS3R), Madgwick filter in `app.ts` |

The factory's unused `io_test_img` artwork is not included.

## Known differences

- **Fonts.** See [Fonts](#fonts): text at 12 px, 16 px and 16 px bold uses
  the factory's efont glyphs pixel for pixel; the two 14 px and 24 px strings
  are images of the efont glyphs; the 6×8 GLCD `Font0` of the UART screen is
  replaced by efont 12 (glyphs about 1 px taller). Text is placed at the
  factory's coordinates; line boxes follow the PocketJS layout, so a string
  can sit a pixel off.
- **Random colours.** I2C addresses and UART bytes use one of 16 fixed
  colours picked by a pseudo-random index (`RandomText.tsx`). A Text node in
  the AOT view subset takes no dynamic colour, so the factory's random 24-bit
  colour is not possible.
- **Frame rate.** The factory loop runs about every 10 ms; the app runs at
  30 Hz, so its 500 ms, 90 ms and 1000 ms timers fire on 33 ms frame
  boundaries.
- **IMU filter.** The factory uses the Arduino MadgwickAHRS library (GPL,
  not copied) configured for 20 Hz while sampling about every 40–50 ms. The
  app implements the same published 6-axis Madgwick filter (beta 0.1) and
  integrates at the real 30 Hz sample period, so the ball reacts at the
  physical rate. Ball mapping keeps the factory's integer `map()` and the
  ±90° clamp.
- **Boot rings.** `fillArc` quarter rings are drawn as anti-aliased discs
  clipped by the screen; the visible shapes are the same rings.
- **Wi-Fi.** The scrolling text sprite is reproduced as a bottom-aligned
  list of the last 9 lines. SSIDs outside printable ASCII render without the
  missing glyphs. On leaving the function the program stops and
  de-initialises the radio to return its memory; the factory left Wi-Fi on.
  A scan with 0 networks stays on "Scaning..." as in the factory.
- **I2C.** The factory's column formula places entries 4–6 on top of
  entries 1–3 in the left column (`(32 * i > 2 ? 1 : 0) + 32`); the app
  reproduces this overlap.
- **UART.** The red-line clear of the RX value uses `fillRect` without a
  colour in the factory (the last drawing colour); the app clears with black.
  One byte per direction and frame is shown; all bytes are forwarded.
- **IR.** The repeat code follows the frame after its 32.8 ms gap (about
  100 ms after the frame start) instead of the factory's 110 ms delay.
- **PWM / IR.** At most 8 USB bytes are taken per frame.

## Fonts

M5GFX draws the factory's text with u8g2-format bitmap fonts built from
/efont/ Unicode BDF 0.4.2 (`efontCN_12`, `_14`, `_16`, `_16_b`, `_24`) and
the GLCD `Font0`. No TrueType edition of these faces exists, so
`fonts/EfontBiwidth12-Regular.ttf`, `EfontBiwidth16-Regular.ttf` and
`EfontBiwidth16-Bold.ttf` were generated from the u8g2 data of M5GFX 0.2.19
(`src/lgfx/Fonts/efont/lgfx_efont_cn.c`, arrays `lgfx_efont_cn_12`,
`lgfx_efont_cn_16`, `lgfx_efont_cn_16_b`; the 117 glyphs U+0020–U+00FF
present there): every set pixel became a square of the outline, the
units-per-em are 12 × 100 and 16 × 64, so at 12 px and 16 px each square
covers exactly one pixel. The PocketJS baker (`bakeSlot`) renders them with
no partially covered pixel (checked for `1. Mg-45` at 12 px and `F:1Kz` at
16 px bold).

| Role | Face | Used for |
| --- | --- | --- |
| `regular` | efont 16 | "Scaning...", I2C addresses (`text-base`) |
| `bold` | efont 16 bold | "F: 1Khz", PWM duty, ADC volts, IR command (`text-base font-bold`) |
| `mono` | efont 12 | Wi-Fi lines and UART bytes (`text-xs font-mono`) |

The app needs three sizes in three roles, so no other size can be
pixel-exact; "Top 5 list:" (efont 14) and "Not found" (efont 24, red) are
the only texts at other sizes and are static, so they are PNG images
(`img/top5_list.png`, `img/not_found.png`) drawn from the same efont data
at the factory's positions. The three atlases take 38,064 B of flash
(Inter took 193 KB for the six sizes before).

`tools/fonts/efont/build_efont.py` regenerates the three faces and the two
images byte for byte from `lgfx_efont_cn.c`; see
`tools/fonts/efont/README.md`.

Licence: /efont/ licence (BSD 3-clause style), (c) Copyright 2000-2003
/efont/ The Electronic Font Open Laboratory; the text is
`fonts/efont-COPYRIGHT.txt`, copied from M5GFX
`src/lgfx/Fonts/efont/COPYRIGHT.txt`. The TrueType files and the two images
are binary redistributions and carry that notice.

## Device bridge

`device.ts` follows `docs/APP-HOST.md`. Board → app: `attached`,
`irAddress`, `imu` (array), `wifi1..wifi5` (text), `wifiRssi` (array),
events `wifiDone`, `i2cDone`, `uartRx`, `uartTx`, `usbRx`, arrays
`i2cAddresses`, `adc`. App → board requests: `enter <function>`,
`leave <function>`, `wifiScan`, `i2cScan`, `uart <baud> <rx> <tx>`,
`pwm <g1> <g2>`, `adc`, `ir <address> <command>`. Without a board
(`attached` false) the model simulates scan results, ADC values and a
slow IMU wobble.

## Validation

`pocket compile --aot` (admission) and the JS bundle compile pass;
`programs/atoms3` builds in AOT mode, and under QEMU the application ran 402
frames through boot, menu, Wi-Fi and IMU with simulated board values and 0
frame errors (`programs/atoms3/README.md`). No desktop/web screenshots were
taken and no device run happened.
