# m5_bsp_sticks3

Board support for the M5Stack StickS3 (SKU K150, ESP32-S3-PICO-1-N8R8). The
component powers the board up through the M5PM1 PMIC and returns ESP-IDF
handles and small driver APIs. It has no PocketJS dependency.
`REQUIRED_IDF_TARGETS esp32s3`, ESP-IDF `>=6.0,<6.3`.

## API

| Header | Function |
| --- | --- |
| `m5_sticks3/board.h` | Pin map; `m5_sticks3_i2c_bus` (I2C1, SDA 47, SCL 48, 100 kHz); `m5_sticks3_power_init`; `m5_sticks3_keys_init`, `m5_sticks3_key_pressed` |
| `m5_sticks3/pm1.h` | M5PM1 register access: GPIO function/mode/drive/output, 5 V output, charge enable, power source, VBAT/VIN/5VOUT voltages, power-key events, shutdown |
| `m5_sticks3/power.h` | Battery level and voltage, charging, USB power, power off |
| `m5_sticks3/display.h` | ST7789 panel on SPI3 (`esp_lcd` IO and panel handles), clear, LEDC backlight |
| `m5_sticks3/speaker.h` | ES8311 playback on I2S0 with the amplifier on PMIC G3; sine tones and master volume |

`m5_sticks3_power_init` must run before the display and the speaker: PMIC G2
switches the LCD rail. It also configures G0 (charger `CHG_STAT` input), G3
(amplifier enable, low), disables the PMIC I2C idle sleep and watchdog,
switches the 5 V boost output off and enables charging, as the UIFlow2
StickS3 startup does.

## Board facts

| Item | Value |
| --- | --- |
| LCD | ST7789 (M5GFX: `st7789v2`; M5Stack docs: ST7789P3), 135×240, GRAM 240×320, column offset 52, row offset 40, inverted colours, RGB565 big-endian |
| LCD bus | SPI3, MOSI 39, SCLK 40, CS 41, DC 45, RST 21, 40 MHz write clock (M5GFX `specs::sticks3`) |
| Backlight | GPIO 38, LEDC 256 Hz, 9 bit, M5GFX `Light_PWM` curve with offset 16 |
| Keys | KEY1 GPIO 11 (front, M5Unified BtnA), KEY2 GPIO 12 (side, BtnB), active low, external pull-ups |
| Power key | M5PM1 power key: click powers on/resets, double click powers off (handled by the PMIC) |
| I2C (internal) | I2C1, SDA 47, SCL 48: M5PM1 0x6E, BMI270 0x68, ES8311 0x18 |
| PMIC GPIO | G0 charger status (low = charging), G2 LCD power, G3 speaker amplifier enable |
| Audio | ES8311, I2S MCLK 18, BCLK 17, WS 15, DOUT 14, DIN 16; speaker on I2S0 at 22050 Hz, 16-bit stereo; codec MCLK taken from BCLK (×8) |
| IR | TX GPIO 46, RX GPIO 42 (pins defined; no driver in this component) |
| Battery level | `(mV − 3300) × 100 / 800`, clamped to 0..100 (M5Unified `Power_Class::getBatteryLevel`) |

## Sources

Code was taken from M5Stack repositories and ported from Arduino/C++ to
ESP-IDF C (`i2c_master`, `esp_lcd`, `i2s_std`, `ledc`). The files keep the
original copyright and license lines; this component's own code is MIT.

| Repository | Revision | Files | Used for |
| --- | --- | --- | --- |
| [m5stack/M5GFX](https://github.com/m5stack/M5GFX) | `28d47949bdc84f30881623476f555398e2194313` (2026-11-05) | `extras/board_spec/boards/m5sticks3.json`, `src/board_detect/m5/generated/esp32s3_board_data.hpp` (`sticks3`), `src/board_detect/m5/pmic_ops.hpp` (`sticks3_power_on`), `src/board_detect/m5/esp32s3_construct.inl` (`construct_sticks3`), `src/lgfx/v1/panel/Panel_ST7789.hpp`, `src/lgfx/v1/platforms/esp32/Light_PWM.inl` | Pins, SPI host and clock, panel offsets and inversion, ST7789 init sequence (`src/display.c`, LovyanGFX FreeBSD licence), backlight curve, LCD power-up |
| [m5stack/M5Unified](https://github.com/m5stack/M5Unified) | `96576a14247ddc935149145aa6b51943464120f1` (2026-11-06) | `src/utility/power/M5PM1_Class.{hpp,inl}`, `src/utility/Power_Class.inl`, `src/M5Unified.inl` (`board_M5StickS3`, `_speaker_enabled_cb_sticks3`) | M5PM1 driver (`src/pm1.c`), battery and charge state, key pins, ES8311 playback registers and amplifier enable (`src/speaker.c`) |
| [m5stack/uiflow-micropython](https://github.com/m5stack/uiflow-micropython) | `a61a29409f09f1f4bebae05e15c4bd6dcc4956b6` (2026-11-18) | `m5stack/boards/M5STACK_StickS3/board_init.c`, `m5stack/modules/startup/sticks3.py` | I2S pin list, amplifier enable through PMIC G3, startup power settings (5 V output off, charging on, volume 100) |

[m5stack/M5PM1](https://github.com/m5stack/M5PM1) `8f1f1a60` was consulted
for the register map; the port follows M5Unified's smaller driver.

## Not included

- BMI270 IMU and microphone capture: the StickS3 startup menu does not use
  them. The BMI270 needs Bosch's 8 KiB configuration blob.
- IR transmit/receive: pins only. The amplifier must be off while the IR
  receiver is used (M5Stack product notes).
- The speaker generates one sine voice; M5Unified's multi-channel mixer and
  WAV playback are not ported.
