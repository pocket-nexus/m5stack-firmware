# m5_bsp_stopwatch

ESP-IDF board support for the M5Stack StopWatch (C152, ESP32-S3). The public
API is `include/m5_stopwatch/bsp.h`. The component has no PocketJS dependency.

- Targets: `esp32s3` only (`REQUIRED_IDF_TARGETS` and `targets` in
  `idf_component.yml`).
- ESP-IDF: `>=6.0,<6.3`.
- Registry dependencies (exact versions): `espressif/esp_lcd_touch` 1.2.1
  (public, the touch handle type is part of `bsp.h`) and
  `espressif/esp_codec_dev` 1.6.2.
- Compiled with `-Wall -Wextra -Werror`.

## Files

| File | Content |
| --- | --- |
| `include/m5_stopwatch/bsp.h` | Public API: pins, `m5_stopwatch_init`, display brightness, battery, power, vibration, RTC, IMU, audio. |
| `src/board.c` | Bring-up sequence (`m5_stopwatch_init`), brightness and power LED, battery filter and charge state, power-off, RTC and IMU wrappers. |
| `src/board.h` | Private declarations shared by the source files. |
| `src/pm1.c`, `src/pm1.h` | Register-level subset of the M5PM1 PMIC driver (I2C 0x6E, 100 kHz). |
| `src/ioe1.c`, `src/ioe1.h` | Register-level subset of the M5IOE1 I/O expander driver (I2C 0x4F, fallback 0x6F, 400 kHz); one mutex covers each read-modify-write. |
| `src/co5300.c`, `src/co5300.h` | CO5300 AMOLED controller as an `esp_lcd_panel_t` over a quad-mode `esp_lcd_panel_io_spi` handle. |
| `src/cst820.c`, `src/cst820.h` | CST820 touch controller as an `esp_lcd_touch` driver (I2C 0x15, 100 kHz). |
| `src/rx8130.c`, `src/rx8130.h` | RX8130CE RTC (I2C 0x32, 100 kHz). |
| `src/bmi270.c`, `src/bmi270.h` | BMI270 bring-up and raw data read (I2C 0x68, 400 kHz). |
| `src/bmi270_config.c` | Bosch BMI270 configuration file (8192 bytes), unchanged. |
| `src/audio.c` | ES8311 codec through `esp_codec_dev`, I2S0 full duplex, speaker amplifier enable. |
| `src/vibrate.c` | Vibration motor on IOE1 PWM channel 0, stopped by a one-shot `esp_timer`. |
| `licenses/` | License texts of the sources the code is derived from. |

## Pins

| Signal | Connection |
| --- | --- |
| Internal I2C (I2C_NUM_0) | SDA GPIO47, SCL GPIO48, internal pull-ups |
| CO5300 QSPI (SPI2, 80 MHz, mode 0) | SCLK GPIO40, D0 GPIO41, D1 GPIO42, D2 GPIO46, D3 GPIO45, CS GPIO39 |
| CO5300 TE | GPIO38, input with pull-up, not used by the BSP |
| CO5300 reset | IOE1 IO5 |
| Display power (L3B_EN) | IOE1 IO8 |
| CST820 interrupt | GPIO13, input with pull-up |
| CST820 reset | IOE1 IO4 |
| Key A (yellow) / key B (blue) | GPIO2 / GPIO1, active low, internal pull-ups |
| I2S0 | MCLK GPIO18, BCLK GPIO17, LRCK GPIO15, DOUT GPIO21, DIN GPIO16 |
| Speaker amplifier | IOE1 IO10 and GPIO14, both high to enable |
| Audio enable | IOE1 IO3 |
| CH442E mux control | IOE1 IO1 |
| Vibration motor | IOE1 IO9, PWM channel 0, 5 kHz |
| PM1 CHG_STAT / CHG_PROG | PM1 GPIO2 (input) / PM1 GPIO3 (output low) |

I2C devices on the internal bus: PM1 0x6E, IOE1 0x4F (or 0x6F), CST820 0x15,
RX8130CE 0x32, BMI270 0x68, ES8311 0x18.

## Power-up sequence

`m5_stopwatch_init` follows `Hal::init` of the factory firmware up to the
touch controller:

1. I2C master bus on I2C_NUM_0.
2. PM1: wake by address probe, read ID/model/HW/SW registers (one retry after
   800 ms), I2C sleep timeout off, button click delay 1 s, watchdog off, LDO
   power hold on (keeps the RTC supplied after power-off), charging on,
   GPIO3 (CHG_PROG) push-pull output low, GPIO2 (CHG_STAT) input without
   pull, single-click reset disabled, first battery reading. A missing PM1 is
   logged and bring-up continues, as in the factory firmware.
3. IOE1 at 0x4F, then 0x6F: I2C sleep off and 400 kHz; IO9, IO8, IO10, IO4,
   IO5, IO1, IO3 push-pull outputs; L3B_EN 1, TP_RST 1, SPK_PA 0, OLED_RST 1,
   MUX_CTR 0, AU_EN 1; PWM frequency 5 kHz; GPIO14 output low; motor PWM duty
   0. L3B_EN is read back every 80 ms and rewritten until it reads high; the
   BSP gives up after 25 retries with `ESP_ERR_TIMEOUT` (the factory loop has
   no limit).
4. 50 ms delay.
5. Display: TE pin input, PM1 power LED off, SPI2 bus in quad mode
   (`max_transfer_sz` from the configuration or 466 x 32 x 2 bytes), panel IO
   (80 MHz, mode 0, queue depth 10, 32-bit commands, 8-bit parameters), CO5300
   init list, COLMOD 0x55, the whole 480 x 480 controller memory filled with
   black in 16-row strips, then the configured brightness. The BSP does not
   register `on_color_trans_done`; the display driver of the caller does.
6. Touch: IOE1 IO4 low for 10 ms, high, 50 ms wait, then the CST820 chip ID
   (0xA7) and firmware version (0xA9) must read non-zero. A missing controller
   leaves `handles.touch` NULL.
7. Keys A and B: GPIO inputs with pull-ups.

The RTC starts on the first `m5_stopwatch_rtc_get` or `m5_stopwatch_rtc_set`.
`m5_stopwatch_imu_init` and `m5_stopwatch_audio_init` are separate calls.
`m5_stopwatch_audio_init` leaves the speaker amplifier off; the factory
firmware enables it at the end of its audio init, the BSP leaves that to
`m5_stopwatch_audio_speaker_enable`.

## Display

The CO5300 has 480 x 480 pixels of memory. Commands are sent as the 32-bit
command word `(0x02 << 24) | (cmd << 8)` with parameters on one data line;
pixels are sent with `esp_lcd_panel_io_tx_color` and the command word
`(0x32 << 24) | (0x2C << 8)`, pixel data on four lines. CASET and RASET set
the window; `draw_bitmap` returns `ESP_ERR_INVALID_ARG` for an odd x start or
an odd width. Pixels are RGB565 with the high byte sent first. `mirror` and
`swap_xy` are not implemented. `esp_lcd_panel_reset` sends SWRESET (0x01) and
waits 120 ms; `m5_stopwatch_init` does not call it, as the factory firmware
does not reset the panel.

Column offset: the factory firmware drives a 468 x 466 window at memory column
6 (`Panel_CO5300` with `offset_x` 6). The visible area is 466 x 466 and sits
two columns into that window, so the BSP places frame column 0 at memory
column 8 (`CO5300_COLUMN_OFFSET` in `src/co5300.c`, row offset 0). This value
has not been confirmed on a device.

Brightness: `m5_stopwatch_display_set_brightness` maps 0..100 % to
0..255 for command 0x51. As in the factory `setBackLightBrightness`, the PM1
power LED (PWR_CFG LED_EN level) is turned off for any value above 0 and on
at 0. Panel IO calls from several tasks are not serialized by the BSP.

Touch coordinates are the raw 12-bit CST820 values, clamped to 0..465. The
factory firmware passes the same raw values to LVGL; no offset is applied for
the 2-column frame shift above.

## Battery

`m5_stopwatch_battery_read` reads VBAT and VIN from the PM1 ADC (mV) and
CHG_STAT (PM1 GPIO2). The battery voltage goes through the factory low-pass
filter, `(7 * old + new + 4) / 8`, seeded with the first reading. The level
is linear from 3300 mV (0 %) to 4200 mV (100 %). External power is VIN above
4000 mV; charging is external power with CHG_STAT low (`isBatteryCharging`
with `strict`). The factory firmware samples once per second in a task; the
BSP filters once per call.

## IMU

`m5_stopwatch_imu_init` reads the chip ID (0x24), soft-resets the BMI270,
disables advanced power save, uploads the configuration file in 128-byte
bursts through INIT_ADDR/INIT_DATA, sets INIT_CTRL, checks INTERNAL_STATUS
for 0x01 (polled every 20 ms, five times), then writes ACC_CONF 0x98
(100 Hz, OSR2/AVG2, performance), ACC_RANGE ±4 g, GYR_CONF 0x98 (100 Hz,
OSR2, performance), GYR_RANGE ±2000 dps with OIS range ±2000 dps, and enables
both sensors. Advanced power save stays off. Interrupt pin mapping, the AUX
(BMM150) interface and the gyroscope cross-sensitivity correction of the
SensorAPI are not ported. Readings are scaled by 8192 LSB/g and
16.384 LSB/dps; X and Y are swapped as in `Hal::updateImuData`.

## Audio

I2S0 master, standard Philips format, 16-bit stereo slots, MCLK on, at
44.1 kHz; the ES8311 is opened through `esp_codec_dev` with one channel and
16 bits, input gain 30 dB. `m5_stopwatch_audio_write` and
`m5_stopwatch_audio_read` block until the samples are transferred.

## Vibration

Strength 1..100 maps to a duty of `25 + strength * 75 / 100` percent on IOE1
PWM channel 0; strength 0 or duration 0 stops the motor. A one-shot
`esp_timer` writes duty 0 after the duration; a new call replaces a running
vibration.

## Provenance

| Source | Revision | Files used | Used for |
| --- | --- | --- | --- |
| [m5stack/M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo) | commit `81099c784991dc77f522de1a5bf62ec5de9ae0b5` | `main/hal/hal.cpp`, `hal_pmic.cpp`, `hal_ioe.cpp`, `hal_display.cpp`, `hal_rtc.cpp`, `hal_imu.cpp`, `hal_audio.cpp`, `hal_button.cpp`, `drivers/cst820/cst820.cpp`, `drivers/cst820/cst820.h`, `drivers/rx8130/rx8130.cpp`, `drivers/rx8130/rx8130.h`, `repos.json`, `dependencies.lock` | Pins, bring-up order, PM1/IOE1 settings, CO5300 init list and window, brightness and LED logic, battery filter and charge logic, CST820 decoding, RX8130 driver, IMU axis mapping, I2S and codec settings, vibration mapping (`board.c`, `co5300.c`, `cst820.c`, `rx8130.c`, `audio.c`, `vibrate.c`) |
| [m5stack/M5PM1](https://github.com/m5stack/M5PM1) | tag `1.0.6`, commit `8f1f1a60b3040088cd0ca2b9eb9d024ccc4d907f` | `src/M5PM1.h`, `src/M5PM1.cpp`, `src/M5PM1_i2c_compat.h` | Register map and the functions in `pm1.c` |
| [m5stack/M5IOE1](https://github.com/m5stack/M5IOE1) | tag `1.0.8`, commit `37db04861687858b6748f5dbd1a84439a6635c46` | `src/M5IOE1.h`, `src/M5IOE1.cpp` | Register map and the functions in `ioe1.c` |
| [m5stack/M5GFX](https://github.com/m5stack/M5GFX) (LovyanGFX code) | tag `0.2.19`, commit `53a7184601f3667b030ba141c58b87ce2acfaa2a` | `src/lgfx/v1/panel/Panel_AMOLED.cpp` | QSPI command and pixel framing, CASET/RASET, even x and width, COLMOD value, sleep-out delay (`co5300.c`) |
| [m5stack/M5GFX](https://github.com/m5stack/M5GFX) | master, commit `28d47949bdc84f30881623476f555398e2194313` | `src/board_detect/m5/pmic_ops.hpp` (`stopwatch_power_on`), `src/board_detect/m5/generated/esp32s3_board_data.hpp` (`stopwatch`) | Cross-check of pins, panel offset and bus frequency; no code copied |
| [lbuque/BMI270_BMM150_Sensor](https://github.com/lbuque/BMI270_BMM150_Sensor) | tag `0.1.2`, commit `ae8bc480bca638d3458e65d5e9a4fff4270c1430` | `src/bmi270_bmm150.c` (`configure_bmi2_sensor`, scaling) | Sensor configuration and scale factors (`bmi270.c`) |
| Bosch BMI270 SensorAPI (submodule of the above) | tag `v2.113.0`, commit `41129fcfe39c583ee5462d79195741945d51c1fe` | `bmi270.c` (`bmi270_config_file`), `bmi2.c` (`bmi2_sec_init`, `bmi2_soft_reset`, `bmi2_write_config_file`), `bmi2_defs.h` | Configuration file (`bmi270_config.c`, unchanged) and the init register sequence (`bmi270.c`) |

The factory CST820 driver files carry no copyright header; the
M5StopWatch-UserDemo repository is MIT licensed (Copyright 2026 M5Stack
Technology CO LTD), and `cst820.c` carries that notice.

## Licenses

| Code | License | Text |
| --- | --- | --- |
| Code derived from M5StopWatch-UserDemo, M5PM1, M5IOE1 | MIT, M5Stack Technology CO LTD | `licenses/M5StopWatch-UserDemo-MIT.txt`, `licenses/M5PM1-MIT.txt`, `licenses/M5IOE1-MIT.txt` |
| AMOLED QSPI framing in `co5300.c` (LovyanGFX `Panel_AMOLED`) | FreeBSD (`BSD-2-Clause-Views`), lovyan03 | `licenses/LovyanGFX-FreeBSD.txt` |
| `bmi270_config.c` (BMI270 configuration file) | BSD-3-Clause, Bosch Sensortec GmbH | `licenses/Bosch-BMI270-SensorAPI-BSD-3-Clause.txt` |
| `bmi270.c`, `bmi270.h` (settings from lbuque/BMI270_BMM150_Sensor, register sequence from the SensorAPI) | MIT (lbuque) and BSD-3-Clause (Bosch) | `licenses/BMI270_BMM150_Sensor-MIT.txt`, `licenses/Bosch-BMI270-SensorAPI-BSD-3-Clause.txt` |
| `board.h` | MIT | — |

Firmware that contains this component reproduces the BSD notices in its
documentation, as both BSD licenses require for binary redistribution.
