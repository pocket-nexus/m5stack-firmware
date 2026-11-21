# m5_bsp_atoms3

Board support for the M5Stack AtomS3 (SKU C123): ESP32-S3FN8, 8 MB flash,
no PSRAM. The component brings up the hardware and returns ESP-IDF handles
and small driver APIs. It has no PocketJS dependency. Target: `esp32s3`,
ESP-IDF `>=6.0,<6.3`.

## Board facts

| Function | Hardware | Pins and settings |
| --- | --- | --- |
| Display | 0.85" 128×128 IPS, GC9107 | SPI3, MOSI 21, SCLK 17, DC 33, CS 15, RST 34, 40 MHz, mode 0 |
| Panel window | GC9107 GRAM 128×160 | rows 32..159 visible: x gap 0, y gap 32; MADCTL 0x08 (BGR), COLMOD 0x55, no inversion |
| Backlight | LEDC | GPIO16, channel 7 (timer 3), 256 Hz, 9 bits, offset 48, default 127 |
| Button | BtnA under the screen | GPIO41, active low, external pull-up |
| IMU | MPU6886 (WHO_AM_I 0x19) at 0x68 | internal I2C, SDA 38, SCL 39, I2C port 1, 400 kHz |
| IR LED | | GPIO4, RMT TX, 38 kHz carrier, 33 % duty |
| Port A (Grove) | HY2.0-4P | G2 = GPIO2 (SDA), G1 = GPIO1 (SCL) |
| USB | USB Serial/JTAG | console and the factory's `USBSerial` |

**AtomS3R differs**: ESP32-S3-PICO-1-N8R8 with 8 MB PSRAM, BMI270 + BMM150
IMU on I2C SDA 45 / SCL 0, panel on MOSI 21, SCLK 15, DC 42, CS 14,
RST 48, and an I2C backlight driver at address 0x30 (M5GFX
`Light_M5StackAtomS3R`). This component does not support the AtomS3R.

Early AtomS3 units can carry an ST7735S panel instead of the GC9107 (M5GFX
reads the panel ID over 3-wire SPI and picks a driver). This component
drives the GC9107 only; it does not read the panel ID.

## API

| Header | Functions |
| --- | --- |
| `m5_atoms3/board.h` | pin map |
| `m5_atoms3/display.h` | `m5_atoms3_display_init` (SPI bus, panel IO, GC9107 `esp_lcd_panel_handle_t`), `m5_atoms3_backlight_init`, `m5_atoms3_backlight_set` |
| `m5_atoms3/button.h` | GPIO41 setup and the M5Unified click/hold state machine (`m5_atoms3_button_update`: CLICKED on release, HOLD after 500 ms) |
| `m5_atoms3/imu.h` | internal I2C bus, MPU6886 init and samples in g and dps |
| `m5_atoms3/ir.h` | extended-NEC frame plus repeat code on the IR LED |
| `m5_atoms3/port_a.h` | Port A I2C scan, UART bridge, 1 kHz PWM, ADC; USB Serial/JTAG read and write |

The display IO has no color-transfer callback, so `pocket_display_esp_lcd`
can own it. The panel takes big-endian RGB565.

## Sources

All files were ported to plain ESP-IDF C and reformatted with the
repository clang-format. Copyright and SPDX lines name the origin.

| Source | Revision | Files | Used in |
| --- | --- | --- | --- |
| [m5stack/M5GFX](https://github.com/m5stack/M5GFX) | tag 0.2.19, `53a7184601f3667b030ba141c58b87ce2acfaa2a` | `src/M5GFX.cpp` (board_M5AtomS3 autodetect: pins, SPI3, 40 MHz, offsets, `_set_pwm_backlight`), `src/lgfx/v1/panel/Panel_GC9A01.hpp` (`Panel_GC9107` init list, `Panel_GC9xxx` MADCTL table), `src/lgfx/v1/panel/Panel_LCD.cpp` (COLMOD/MADCTL/INVOFF after init), `src/lgfx/v1/platforms/esp32/Light_PWM.cpp` (duty formula) | `src/panel_gc9107.c`, `src/display.c`, `include/m5_atoms3/board.h` |
| [m5stack/M5Unified](https://github.com/m5stack/M5Unified) | tag 0.3.0, `96576a14247ddc935149145aa6b51943464120f1` | `src/utility/Button_Class.hpp`, `src/utility/Button_Class.inl`, `src/utility/imu/MPU6886_Class.inl`, `src/M5Unified.inl` (GPIO41 button, internal/external I2C pins) | `src/button.c`, `src/imu_mpu6886.c` |
| [m5stack/M5AtomS3-UserDemo](https://github.com/m5stack/M5AtomS3-UserDemo) | `8eaf37b3ccf9a8d7c4bf4c2dea0e7925ede6328d` | `lib/infrared_tools/src/ir_builder_rmt_nec.c`, `lib/infrared_tools/include/ir_timings.h` (Espressif, Apache-2.0), `src/main.cpp` (`ir_tx_init`, `ir_tx_send`, Port A functions) | `src/ir_nec.c`, `src/port_a.c` |
| ESP-IDF | v6.1-dev-8142-g188e3e55bb2 | `components/esp_lcd/src/esp_lcd_panel_st7789.c` (esp_lcd panel structure, Apache-2.0) | `src/panel_gc9107.c` |

Licences: M5GFX, M5Unified and M5AtomS3-UserDemo are MIT; the LovyanGFX
files inside M5GFX are FreeBSD (BSD-2-Clause); infrared_tools and the
ESP-IDF driver are Apache-2.0.

Differences from the sources:

- The infrared_tools builder wrote `rmt_item32_t` items for the legacy RMT
  driver and the factory waited 110 ms between the frame and the repeat
  code. `m5_atoms3_ir_send_nec` queues both on an RMT TX channel with a copy
  encoder; the repeat code follows the frame's 32.8 ms ending gap, about
  100 ms after the frame start.
- The factory's Arduino calls (`Wire`, `Serial`, `ledcSetup`, `analogRead`,
  `USBSerial`) are replaced by the ESP-IDF i2c_master, uart, ledc,
  adc_oneshot and usb_serial_jtag drivers with the same pins, rates and
  resolutions.
- The MPU6886 driver keeps M5Unified's register sequence and its ±8 g /
  ±2000 dps ranges; it reads with the data-ready check and without FIFO.
