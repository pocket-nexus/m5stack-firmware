# m5_bsp_tab5

Board support for the M5Stack Tab5 (ESP32-P4NRW32: 32 MiB PSRAM, 16 MiB flash),
taken from M5Stack's factory firmware. The component has no PocketJS
dependency: it powers the board up and returns ESP-IDF handles
(`esp_lcd_panel_handle_t`, `esp_lcd_touch_handle_t`, `i2c_master_bus_handle_t`)
and small driver APIs. The public header is `include/m5/tab5.h`; every public
symbol starts with `m5_tab5_`. Target: `esp32p4`, ESP-IDF `>=6.0,<6.3`.

## Source

Repository `https://github.com/m5stack/M5Tab5-UserDemo`, commit
`fe8aa051beb9385468f7e46b909e764169df3f7a` (main, 2026-10-29).

| File here | Taken from | License |
| --- | --- | --- |
| `src/board.c` | `platforms/tab5/components/m5stack_tab5/m5stack_tab5.c`: `bsp_i2c_init`, `bsp_io_expander_pi4ioe_init`, `bsp_set_charge_qc_en`, `bsp_set_charge_en`, `bsp_set_usb_5v_en`, `bsp_set_ext_5v_en`, `bsp_set_ext_antenna_enable`, `bsp_set_wifi_power_enable`, `bsp_headphone_detect`, `bsp_usb_c_detect`, `bsp_generate_poweroff_signal`, `bsp_reset_tp` | Apache-2.0 |
| `src/display.c` | same file: `bsp_detect_display_type`, `bsp_display_brightness_init/set`, `bsp_enable_dsi_phy_power`, `bsp_display_new_with_handles`, `bsp_display_new_with_handles_to_st7123`, `bsp_touch_new`, `bsp_display_indev_init_to_st7123`, `bsp_display_get_panel_ic` | Apache-2.0 |
| `src/panel_init_ili9881c.h` | `platforms/tab5/components/m5stack_tab5/include/bsp/ili9881_init_data.c` | Apache-2.0 |
| `src/panel_init_st7123.h` | `st7123_vendor_specific_init_default` in `m5stack_tab5.c` | Apache-2.0 |
| `src/esp_lcd_st7123.c`, `src/esp_lcd_st7123.h` | `platforms/tab5/components/m5stack_tab5/esp_lcd_st7123.c`, `priv_include/esp_lcd_st7123.h` | Apache-2.0 |
| `src/esp_lcd_st7121.c`, `src/esp_lcd_st7121.h` | `platforms/tab5/components/esp_lcd_st7121/` | Apache-2.0 |
| `src/sdcard.c` | `bsp_sdcard_init`, `bsp_sdcard_deinit` in `m5stack_tab5.c` | Apache-2.0 |
| `src/audio.c` | `bsp_audio_init`, `bsp_audio_codec_speaker_init`, `bsp_audio_codec_microphone_init`, `bsp_codec_init`, `bsp_codec_set_volume` in `m5stack_tab5.c` | Apache-2.0 |
| `src/rtc.c` | `platforms/tab5/main/hal/utils/rx8130/rx8130.cpp`, ported to C | MIT |
| `src/power_monitor.c` | `platforms/tab5/components/power_monitor_ina226/src/ina226.cpp` (an ESP-IDF port of jarzebski/Arduino-INA226), ported to C; settings from `main/hal/hal_esp32.cpp` | MIT |
| `src/imu.c` | `platforms/tab5/components/sensor_bmi270/src/accel_gyro_bmi270.c` and `main/hal/components/hal_imu.cpp` | MIT |
| `src/bmi270/bmi2.c`, `bmi270.c`, `bmi2.h`, `bmi270.h`, `bmi2_defs.h` | `platforms/tab5/components/sensor_bmi270/` (Bosch Sensortec BMI270 SensorAPI) | BSD-3-Clause |
| `src/interfaces.c` | `getCpuTemp`, `i2cScan`, `initPortAI2c` in `main/hal/hal_esp32.cpp`, `bsp_ext_i2c_init` in `m5stack_tab5.c`, `main/hal/components/hal_rs485.cpp` | MIT |

The files keep their copyright and `SPDX-License-Identifier` lines and are
reformatted with the repository's clang-format. C++ sources were ported to C;
the LVGL, esp_lvgl_port, USB host and SPIFFS parts of the factory BSP are not
taken. Chip drivers that the factory takes from the ESP component registry are
declared in `idf_component.yml`: `esp_lcd_ili9881c` 1.1.0,
`esp_lcd_touch_gt911` 1.2.1, `esp_lcd_touch_st7123` 1.0.2, `esp_lcd_touch`
1.2.1 and `esp_codec_dev` 1.5.11.

## Calls

| Call | Effect |
| --- | --- |
| `m5_tab5_init` | System I2C (port 0, SDA GPIO31, SCL GPIO32) and the factory expander sequence below |
| `m5_tab5_set_switch`, `m5_tab5_get_switch` | One expander output (table below) |
| `m5_tab5_headphone_detect`, `m5_tab5_usb_c_detect` | Expander inputs 0x43 P7 and 0x44 P6 |
| `m5_tab5_power_off` | Three pulses on 0x44 P4, the power path's off request |
| `m5_tab5_touch_reset` | Touch and LCD reset through 0x43 P5 and P4 |
| `m5_tab5_panel`, `m5_tab5_panel_name` | Panel variant from the touch controller (below) |
| `m5_tab5_display_new` | LDO3 at 2.5 V for the DSI PHY, 2-lane DSI, RGB565 DPI panel with 1 to 3 PSRAM frame buffers |
| `m5_tab5_backlight_set` | LEDC timer 0 channel 1, 12-bit, 5 kHz on GPIO22 |
| `m5_tab5_touch_new` | GT911 at 0x14 or ST712x at 0x55 in panel coordinates (720 x 1280), interrupt GPIO23 |
| `m5_tab5_sdcard_mount` | 4-bit SDMMC slot 0 (CLK 43, CMD 44, D0-D3 39-42), IO power from LDO4, FAT |
| `m5_tab5_audio_*` | I2S1 at 48 kHz: ES8388 (0x10) on a 16-bit stereo TX channel, ES7210 (0x40) on a 4-slot TDM RX channel `[MIC-L, AEC, MIC-R, MIC-HP]`; MCLK 30, BCLK 27, WS 29, DOUT 26, DIN 28 |
| `m5_tab5_rtc_*` | RX8130CE at 0x32 |
| `m5_tab5_power_monitor_*` | INA226 at 0x41: 5 mOhm shunt, 0.3 mA current LSB, 16 averages of 1.1 ms conversions |
| `m5_tab5_imu_*` | BMI270 at 0x68: +-4 g and +-1000 dps at 200 Hz, any-motion interrupt on INT1 |
| `m5_tab5_cpu_temperature` | On-chip temperature sensor, 20 to 100 degrees C range |
| `m5_tab5_i2c_scan`, `m5_tab5_port_a_i2c_*` | Probe 0x10..0x7F; Port A bus on I2C1 (SDA 53, SCL 54) |
| `m5_tab5_rs485_*` | UART1 half duplex (TX 20, RX 21, DE 34) |

## Panel variants

`m5_tab5_panel` probes the touch controller the way the factory BSP does:
an ACK at 0x14 is the GT911 of the ILI9881C panel; otherwise an ACK at 0x55 is
an ST712x, whose firmware byte (register 0x0000) is 1 on the ST7121 and 3 on
the ST7123. Without an answer it falls back to the ILI9881C.

| Variant | DSI lane rate | DPI clock | H pulse / back / front | V pulse / back / front |
| --- | --- | --- | --- | --- |
| ILI9881C | 730 Mbps | 60 MHz | 40 / 140 / 40 | 4 / 20 / 20 |
| ST7123 | 965 Mbps | 70 MHz | 2 / 40 / 40 | 2 / 8 / 220 |
| ST7121 | 965 Mbps | 70 MHz | 2 / 40 / 40 | 20 / 24 / 200 |

All variants use 2 data lanes and RGB565 frame buffers. The ST7121 runs the
default initialization table of `esp_lcd_st7121.c`.

## IO expanders (PI4IOE5V6408)

| Expander | Pin | Function | After `m5_tab5_init` |
| --- | --- | --- | --- |
| 0x43 | P0 | external antenna RF switch (`M5_TAB5_SWITCH_EXT_ANTENNA`) | low (internal antenna) |
| 0x43 | P1 | speaker amplifier NS4150B (`M5_TAB5_SWITCH_SPEAKER`) | high |
| 0x43 | P2 | external 5 V (`M5_TAB5_SWITCH_EXT_5V`) | high |
| 0x43 | P4 | LCD reset | pulsed low 10 ms, then input with pull-up |
| 0x43 | P5 | touch reset | high |
| 0x43 | P6 | camera (`M5_TAB5_SWITCH_CAMERA`) | high |
| 0x43 | P7 | headphone detect | input |
| 0x44 | P0 | ESP32-C6 power (`M5_TAB5_SWITCH_WIFI`) | high |
| 0x44 | P3 | USB-A host 5 V (`M5_TAB5_SWITCH_USB_A_5V`) | high |
| 0x44 | P4 | power-off request | low |
| 0x44 | P5 | quick charge, active low (`M5_TAB5_SWITCH_QUICK_CHARGE`) | low (enabled) |
| 0x44 | P6 | USB-C VBUS detect | input, interrupt enabled |
| 0x44 | P7 | charge enable IP2326 (`M5_TAB5_SWITCH_CHARGE`) | low (off) |

The factory HAL turns charging on after this sequence; `programs/tab5` does
the same.

## Conventions kept from the factory

- The RTC stores `tm_mon` (0..11) as is and the year modulo 100 since 2000,
  so a clock set by the factory firmware reads back unchanged.
- IMU readings use the factory HAL's axis mapping and scaling (acceleration in
  g, rotation in tens of degrees per second).
