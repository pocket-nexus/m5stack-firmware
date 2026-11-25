# m5_bsp_corep4x

Board support for the M5Stack CoreP4X (C157: Stamp-P4X with an ESP32-P4NRW32X
of chip revision 3.x, 32 MiB PSRAM, 16 MiB flash), taken from M5Stack's
factory firmware. The component has no PocketJS dependency: it powers the
board up and returns ESP-IDF handles (`esp_lcd_panel_handle_t`,
`esp_lcd_touch_handle_t`, `i2c_master_bus_handle_t`, `esp_eth_handle_t`) and
small driver APIs. Public headers are in `include/m5_corep4x/` (`bsp.h`,
`imu.h`, `rtc.h`, `ethernet.h`, `audio.h`); every public symbol starts with
`m5_corep4x_`. Target: `esp32p4`, ESP-IDF `>=6.0,<6.3`.

## Source

Repository `https://github.com/m5stack/M5CoreP4X-UserDemo`, commit
`cd1e1bd0bfd4b6e51a4898999b33a9392ea11f65` (main, 2026-11-12), MIT. The
expander and PMIC protocols come from M5Stack's Arduino/ESP-IDF libraries the
factory firmware vendors.

| File here | Taken from | License |
| --- | --- | --- |
| `src/board.c` | `components/m5_corep4_component/m5_corep4_component.cpp`: `m5_corep4_tool::init`, `set_status` (POWER_*), `set_charge_current`; `m5_corep4_status.cpp`; the power sequence of `main/main.cpp` | MIT |
| `src/display.c` | `m5_corep4_component.cpp`: `lcd_init`, `set_lcd_power`, `reset_lcd`, `reset_touch`, `set_lcd_brightness`, `touch_init`, `touch_enter_sleep`, `touch_resume`, the LCD/touch steps of `sleep_before_work`; panel constants of `m5_corep4_component.h` | MIT |
| `src/power.c` | `m5_corep4_component.cpp`: `p4_pm1_irq_init`, `enter_deep_sleep`, the rail steps of `sleep_before_work`, `corep4_hold_sleep_reset_pins` / `corep4_unhold_sleep_reset_pins` | MIT |
| `src/ioe1.c`, `src/ioe1.h` | `m5stack/M5IOE1` tag `1.0.9` (commit `846eec7d05e25c09013be2acdb8804487f48a62e`), `src/M5IOE1.cpp`: begin with address detection, pin mode, drive mode, digital write/read, PWM, I2C sleep | MIT |
| `src/pm1.c`, `src/pm1.h` | `m5stack/M5PM1` tag `1.0.7` (commit `be9a5456c007c333e7ac963f33bfde1ffa5d82ee`), `src/M5PM1.cpp`: begin, registers, watchdog, GPIO function/mode/pull/drive/wake, ADC, I2C sleep, system commands | MIT |
| `src/st7102.c`, `src/st7102.h` | `components/m5_corep4_component/third_party_component/esp_lcd_st7102/esp_lcd_st7102.c` (Espressif `esp_lcd_st7102` 1.0.0 as vendored by M5Stack) | Apache-2.0 |
| `src/panel_init_st7102.h` | `st7102_init_cmds` in `third_party_component/esp_lcd_st7102/include/esp_lcd_st7102.h` | Apache-2.0 |
| `src/cst3530.c`, `src/cst3530.h` | `third_party_component/esp_lcd_touch_cst3xx/esp_lcd_touch_cst3xx.c` (Espressif/Waveshare driver as vendored by M5Stack), without the firmware updater | Apache-2.0 |
| `src/imu.c` | `components/otool_esp_component/otool_lsm6ds3trc/lsm6ds3trc_tools.cpp`; `imu_init`, `imu_configure_motion_interrupt`, `imu_prepare_for_sleep` of `m5_corep4_component.cpp` | MIT |
| `src/rtc.c`, `src/rtc_regs.h` | `components/otool_esp_component/otool_ic_rx8130/rx8130_tools.cpp` (`init`, `initBat`, `clearIrqFlags`, `disableIrq`, `setTime`, `getTime`, `setTimerIrq`) and the RTC steps of `sleep_before_work` | MIT |
| `src/ethernet.c` | `third_party_component/p4_ethernet_init/p4_ethernet_init.c`, the PHY reset of `main/main.cpp`, the pin release of `sleep_before_work` | MIT |
| `src/audio.c` | `components/otool_esp_component/otool_audio/` (codec bring-up) and the audio power order of `main/main.cpp` | MIT |

The files keep their copyright and `SPDX-License-Identifier` lines and are
reformatted with the repository's clang-format. C++ sources were ported to C.
The LVGL port, ESP-Hosted Wi-Fi, the USB host and the CST3530 firmware
updater (`esp_lcd_touch_cst3530_update`) are not taken. Chip drivers the
factory takes from the ESP component registry are declared in
`idf_component.yml`: `esp_lcd_touch` 1.2.1, `esp_codec_dev` 1.5.11 and
`ip101` 1.1.0 (the IP101 PHY driver left `esp_eth` in ESP-IDF 6.0; the
factory bundles a copy).

## Calls

| Call | Effect |
| --- | --- |
| `m5_corep4x_init` | Internal I2C (port 0, SDA GPIO11, SCL GPIO9, 400 kHz, internal pull-ups), M5IOE1 (auto-detected), M5PM1 (0x6E, 100 kHz), the factory power sequence below |
| `m5_corep4x_set_switch`, `m5_corep4x_get_switch` | One power switch (table below) |
| `m5_corep4x_set_charge`, `m5_corep4x_charge` | Off, 180 mA or 650 mA |
| `m5_corep4x_power_read` | PM1 VBAT, VIN, 5VINOUT (mV), power source; charger status on IOE1 IO6 |
| `m5_corep4x_power_off`, `m5_corep4x_reboot` | PM1 SYS_CMD 0xA1 / 0xA2 after 120 ms |
| `m5_corep4x_sd_detected`, `m5_corep4x_sd_detect_enable` | microSD detect on IOE1 IO13 (low = inserted), pull-up on or off |
| `m5_corep4x_display_new`, `m5_corep4x_display_del` | LCD power, reset (100 ms low, 120 ms), LDO3 at 2.5 V for the DSI PHY, 2-lane DSI at 700 Mbps, ST7102 RGB565 DPI panel at 24 MHz (HSYNC 2, HBP 40, HFP 40, VSYNC 4, VBP 8, VFP 200) with 1 to 3 PSRAM frame buffers; delete turns the panel off, cuts LCD power and releases the LDO |
| `m5_corep4x_backlight_set` | IOE1 PWM channel 0 on IO9, 50 kHz, 12-bit duty; IO9 push-pull |
| `m5_corep4x_touch_new`, `_touch_info`, `_touch_sleep`, `_touch_wake`, `_touch_power_down` | CST3530 at 0x58, interrupt GPIO1, up to 5 contacts; information block 0xD0030000; sleep 0xD00022AB; wake by reset pulse; power down holds the reset low |
| `m5_corep4x_pmic_irq_start`, `_pmic_irq_take`, `_wake_source` | PM1 GPIO2 as IRQ output to GPIO0, button and GPIO IRQ status, wake source register |
| `m5_corep4x_sleep_rails_off`, `m5_corep4x_deep_sleep` | The last steps of the factory sleep sequence and the deep-sleep entry (below) |
| `m5_corep4x_imu_*` | LSM6DS3TR-C at 0x6A or 0x6B: 104 Hz, +-2 g, 2000 dps; wake-up interrupt on INT1 for motion wake |
| `m5_corep4x_rtc_*` | RX8130CE at 0x32: time, timer interrupt for RTC wake |
| `m5_corep4x_eth_*` | IP101 over RMII (below) |
| `m5_corep4x_audio_*` | ES8311 DAC (0x18) and ES7210 ADC (0x40) on I2S0 (below) |

## Power switches

The M5IOE1 is found by address detection (0x4F, 0x6F, 0x70..0x76, 0x50; the
chip revision decides which addresses are accepted). Expander pins are
numbered from 0 here (the factory's `M5IOE1_PIN_n` is pin n-1).

| `m5_corep4x_switch_t` | Factory name | Output |
| --- | --- | --- |
| `L3B` | L3B | IOE1 IO12: shared peripheral rail (Ethernet PHY, IR, microSD); the factory also turns it on before it uses the IMU |
| `AUDIO` | AudioIC | IOE1 IO1: ES8311 and ES7210 |
| `SPEAKER` | AudioPA | IOE1 IO3: AW8737A amplifier |
| `LCD` | LCD | IOE1 IO10: LCD controller |
| `GROVE` | Grove | IOE1 IO5: Port A 5 V |
| `USB_A` | USBA | IOE1 IO2: USB-A 5 V out |
| `USB_C` | P4USBC | IOE1 IO4: USB-C OTG 5 V out |
| `M5BUS_5V` | M5BUS | PM1 PWR_CFG bit 3 (BOOST_EN) |

`m5_corep4x_init` follows the factory: touch and LCD resets released (open
drain), L3B on and 50 ms, the microSD detect and charger status inputs, PM1
watchdog off and its LED off, USB-A off for 200 ms and on, audio codecs on,
amplifier off (the audio code turns it on), LCD and Port A on, USB-C off for
200 ms and on, charging at 180 mA (PM1 GPIO1 at high impedance; 650 mA drives
it low; off clears PWR_CFG bit 0).

## Deep sleep

`m5_corep4x_sleep_rails_off` releases the ESP32-C6 socket (reset GPIO42 low,
GPIO40 and SDIO GPIO43-48 at high impedance), switches the amplifier, codecs,
Port A, USB-C, USB-A, L3B and M5-Bus 5 V off, waits 100 ms and sets the PM1
and expander I2C sleep to 3 s. `m5_corep4x_deep_sleep` then arms EXT1
any-low wake on GPIO0 (the PM1 IRQ: power button always, the RX8130 timer on
PM1 GPIO4 with RTC wake, the LSM6DS3TR-C on PM1 GPIO0 with IMU wake) and
GPIO1 (CST3530 interrupt, with touch wake; the controller stays powered),
programs the PM1 wake pins and IRQ masks (buttons always unmasked), clears
the PM1 timer and IRQ flags, releases the I2C pins, holds the PHY and C6
resets low through the PMU's HP-sleep pad hold and enters deep sleep. A wake
is a reset; `m5_corep4x_init` releases the holds after a deep-sleep reset and
`m5_corep4x_pmic_irq_start` clears the button IRQ of the wake press.

## Ethernet

IP101 at PHY address 1 on the internal EMAC: RMII REF_CLK GPIO32 (input),
TX_EN 33, TXD0 41, TXD1 35, CRS_DV 28, RXD0 29, RXD1 30, MDC 31, MDIO 36; PHY
reset GPIO52 (50 ms low, 100 ms settle). The PHY is powered by L3B.

## Audio

I2S0 full duplex: MCLK GPIO2, BCLK GPIO6, WS GPIO4, DOUT GPIO3 to the ES8311,
DIN GPIO5 from the ES7210 (TDM).

## Conventions kept from the factory

- The RX8130CE keeps the factory's register encoding (month 0-based,
  weekday as a plain number, year - 2000), so a board keeps its time when it
  moves between the factory firmware and this one (`include/m5_corep4x/rtc.h`).
- The CST3530 reports the coordinates of the upright picture minus one pixel
  on each axis: the factory mirrors both axes in the driver and turns the
  LVGL display 180 degrees. Programs subtract one (`programs/corep4x`).
- The panel is mounted upside down; the factory turns its picture 180
  degrees, the program presents the picture half-turned
  (`pocket_display_esp_dpi`, `half_turn`).
