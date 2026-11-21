# m5_bsp_sticks3

Board support for the M5Stack StickS3 (SKU K150, ESP32-S3-PICO-1-N8R8). The
component powers the board up through the M5PM1 PMIC and returns ESP-IDF
handles and small driver APIs. It has no PocketJS dependency.
`REQUIRED_IDF_TARGETS esp32s3`, ESP-IDF `>=6.0,<6.3`.

## API

| Header | Function |
| --- | --- |
| `m5_sticks3/board.h` | Pin map; `m5_sticks3_i2c_bus` (I2C1, SDA 47, SCL 48, 100 kHz); `m5_sticks3_power_init`; `m5_sticks3_keys_init`, `m5_sticks3_key_pressed`, `m5_sticks3_key_press_time_us` |
| `m5_sticks3/pm1.h` | M5PM1 register access: GPIO function/mode/drive/output, 5 V output, charge enable, power source, VBAT/VIN/5VOUT voltages, power-key events, shutdown |
| `m5_sticks3/power.h` | Battery level and voltage, charging, USB power, power off |
| `m5_sticks3/display.h` | ST7789 panel on SPI3 (`esp_lcd` IO and panel handles), clear, LEDC backlight |
| `m5_sticks3/speaker.h` | ES8311 playback on the TX side of a full-duplex I2S0 channel with the amplifier on PMIC G3; sine tones, tones started at an `esp_timer` time, master volume |
| `m5_sticks3/mic.h` | ES8311 capture on the RX side of the same channel: start/stop, RMS and peak level (dBFS) of the samples since the previous call |
| `m5_sticks3/imu.h` | BMI270 accelerometer and gyroscope (I2C 0x68, 400 kHz) in M5Unified's StickS3 axis frame |

`m5_sticks3_power_init` must run before the display, the speaker and the
IMU: PMIC G2 switches the LCD rail and the function creates the I2C bus. It
also configures G0 (charger `CHG_STAT` input), G3 (amplifier enable, low),
disables the PMIC I2C idle sleep and watchdog, switches the 5 V boost output
off and enables charging, as the UIFlow2 StickS3 startup does.

## Board facts

| Item | Value |
| --- | --- |
| LCD | ST7789 (M5GFX: `st7789v2`; M5Stack docs: ST7789P3), 135×240, GRAM 240×320, column offset 52, row offset 40, inverted colours, RGB565 big-endian |
| LCD bus | SPI3, MOSI 39, SCLK 40, CS 41, DC 45, RST 21, 40 MHz write clock (M5GFX `specs::sticks3`) |
| Backlight | GPIO 38, LEDC 256 Hz, 9 bit, M5GFX `Light_PWM` curve with offset 16 |
| Keys | KEY1 GPIO 11 (front, M5Unified BtnA), KEY2 GPIO 12 (side, BtnB), active low, external pull-ups; any-edge interrupt timestamps presses with `esp_timer`, falling edges within 20 ms of a rising edge are bounce |
| Power key | M5PM1 power key: click powers on/resets, double click powers off (handled by the PMIC) |
| I2C (internal) | I2C1, SDA 47, SCL 48: M5PM1 0x6E, BMI270 0x68, ES8311 0x18 |
| PMIC GPIO | G0 charger status (low = charging), G2 LCD power, G3 speaker amplifier enable |
| Audio | ES8311, I2S MCLK 18, BCLK 17, WS 15, DOUT 14, DIN 16; one full-duplex I2S0 controller at 22050 Hz, 16-bit stereo Philips slots for speaker (TX) and microphone (RX); codec MCLK taken from BCLK (×8) |
| IMU | BMI270 at 0x68, ±4 g (8192 LSB/g) and ±2000 dps (16.384 LSB/dps), both 100 Hz, advanced power save off; board frame X = +Y, Y = −X, Z = +Z of the sensor |
| IR | TX GPIO 46, RX GPIO 42 (pins defined; no driver in this component) |
| Battery level | `(mV − 3300) × 100 / 800`, clamped to 0..100 (M5Unified `Power_Class::getBatteryLevel`) |

## IMU

`m5_sticks3_imu_init` reads the chip ID (0x24), soft-resets the BMI270,
disables advanced power save, uploads Bosch's 8192-byte configuration file
in 128-byte bursts through INIT_ADDR/INIT_DATA, sets INIT_CTRL, polls
INTERNAL_STATUS for 0x01 (every 20 ms, five times), then writes ACC_CONF
0x98 (100 Hz, OSR2/AVG2, performance), ACC_RANGE ±4 g, GYR_CONF 0x98,
GYR_RANGE ±2000 dps and enables both sensors. M5Unified leaves ACC_RANGE at
the reset value (±8 g); this driver keeps the ±4 g of the StopWatch copy.
`m5_sticks3_imu_read` reads the 12 data bytes from ACC_X_LSB (0x0C) and
maps them to the board frame M5Unified uses for StickS3
(`axis_order_yxz | axis_invert_y`). Which screen edges the board axes point
to has not been checked on a device.

## Microphone

M5Unified drives the speaker on I2S0 and the microphone on I2S1 with the
same BCLK/WS/MCLK pins and runs one at a time. This component creates TX and
RX on I2S0 as a full-duplex pair, initialises both with the same standard
configuration (DOUT 14 and DIN 16), and programs the ES8311 once with the
union of M5Unified's two register lists: CLOCK_MANAGER 0xBF (DAC and ADC
clocks), ADC input Mic1p-Mic1n with PGA gain 0 dB (REG14 0x10), ADC volume
maximum (REG17 0xFF), ADC equalizer bypass and DC offset cancellation (REG1C
0x6A). `m5_sticks3_mic_start` enables RX and starts a capture task that
reads 256-frame blocks (11.6 ms) and accumulates the sum of squares and the
peak magnitude of each slot; `m5_sticks3_mic_take_level` returns the louder
slot's RMS and peak in dBFS (floor −96) and resets the window. The level is
relative to the converter's full scale after the codec gain; it is not a
calibrated sound pressure level. M5Unified applies a further ×16
(`magnification`) in software, which this component does not.

## Sources

Code was taken from M5Stack repositories and ported from Arduino/C++ to
ESP-IDF C (`i2c_master`, `esp_lcd`, `i2s_std`, `ledc`). The files keep the
original copyright and license lines; this component's own code is MIT.

| Repository | Revision | Files | Used for |
| --- | --- | --- | --- |
| [m5stack/M5GFX](https://github.com/m5stack/M5GFX) | `28d47949bdc84f30881623476f555398e2194313` (2026-11-05) | `extras/board_spec/boards/m5sticks3.json`, `src/board_detect/m5/generated/esp32s3_board_data.hpp` (`sticks3`), `src/board_detect/m5/pmic_ops.hpp` (`sticks3_power_on`), `src/board_detect/m5/esp32s3_construct.inl` (`construct_sticks3`), `src/lgfx/v1/panel/Panel_ST7789.hpp`, `src/lgfx/v1/platforms/esp32/Light_PWM.inl` | Pins, SPI host and clock, panel offsets and inversion, ST7789 init sequence (`src/display.c`, LovyanGFX FreeBSD licence), backlight curve, LCD power-up |
| [m5stack/M5Unified](https://github.com/m5stack/M5Unified) | `96576a14247ddc935149145aa6b51943464120f1` (2026-11-06) | `src/utility/power/M5PM1_Class.{hpp,inl}`, `src/utility/Power_Class.inl`, `src/M5Unified.inl` (`board_M5StickS3`, `_speaker_enabled_cb_sticks3`, `_microphone_enabled_cb_sticks3`), `src/utility/IMU_Class.inl` (`board_M5StickS3` axis order), `src/utility/Mic_Class.inl` (standard-mode slot format) | M5PM1 driver (`src/pm1.c`), battery and charge state, key pins, ES8311 playback and capture registers and amplifier enable (`src/speaker.c`), IMU axis frame (`src/imu.c`) |
| [m5stack/uiflow-micropython](https://github.com/m5stack/uiflow-micropython) | `a61a29409f09f1f4bebae05e15c4bd6dcc4956b6` (2026-11-18) | `m5stack/boards/M5STACK_StickS3/board_init.c`, `m5stack/modules/startup/sticks3.py` | I2S pin list, amplifier enable through PMIC G3, startup power settings (5 V output off, charging on, volume 100) |

[m5stack/M5PM1](https://github.com/m5stack/M5PM1) `8f1f1a60` was consulted
for the register map; the port follows M5Unified's smaller driver.

The BMI270 driver is a copy of this repository's
`platforms/esp-idf/m5_bsp_stopwatch/src/bmi270.{c,h}` and `bmi270_config.c`
with the symbol prefix changed (`m5_sticks3_bmi270_`) and the I2C address
passed in; the configuration array is byte-identical. Its sources:

| Source | Revision | Files used | Used for |
| --- | --- | --- | --- |
| [lbuque/BMI270_BMM150_Sensor](https://github.com/lbuque/BMI270_BMM150_Sensor) | tag `0.1.2`, commit `ae8bc480bca638d3458e65d5e9a4fff4270c1430` | `src/bmi270_bmm150.c` (`configure_bmi2_sensor`, scaling) | Sensor configuration and scale factors (`src/bmi270.c`) |
| Bosch BMI270 SensorAPI (submodule of the above) | tag `v2.113.0`, commit `41129fcfe39c583ee5462d79195741945d51c1fe` | `bmi270.c` (`bmi270_config_file`), `bmi2.c` (`bmi2_sec_init`, `bmi2_soft_reset`, `bmi2_write_config_file`), `bmi2_defs.h` | Configuration file (`src/bmi270_config.c`, unchanged) and the init register sequence (`src/bmi270.c`) |

## Licenses

| Code | License | Text |
| --- | --- | --- |
| Code derived from M5Stack repositories (`board.c`, `pm1.c`, `power.h`, `speaker.c`, `imu.c`, `display.c` panel sequence) | MIT, M5Stack (LovyanGFX code: FreeBSD) | headers of each file |
| `src/bmi270_config.c` (BMI270 configuration file) | BSD-3-Clause, Bosch Sensortec GmbH | `licenses/Bosch-BMI270-SensorAPI-BSD-3-Clause.txt` |
| `src/bmi270.c`, `src/bmi270.h` | MIT (lbuque) and BSD-3-Clause (Bosch) | `licenses/BMI270_BMM150_Sensor-MIT.txt`, `licenses/Bosch-BMI270-SensorAPI-BSD-3-Clause.txt` |
| `mic.c`, `audio.h`, `mic.h`, `imu.h` | MIT | — |

Firmware that contains this component reproduces the BSD notice in its
documentation, as the BSD-3-Clause licence requires for binary
redistribution.

## Not included

- BMI270 interrupts, FIFO, feature engine (step counter, gestures), the AUX
  (BMM150) interface and offset calibration: readings are the raw data
  registers.
- IR transmit/receive: pins only. The amplifier must be off while the IR
  receiver is used (M5Stack product notes).
- The speaker generates one sine voice; M5Unified's multi-channel mixer and
  WAV playback are not ported.
