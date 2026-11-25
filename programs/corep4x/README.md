# programs/corep4x

ESP-IDF program for the M5Stack CoreP4X (C157). It runs the factory demo
`apps/corep4x-factory` on the board, with the board support
`platforms/esp-idf/m5_bsp_corep4x` and the shared app host
`platforms/esp-idf/m5_pocket_app` (`docs/APP-HOST.md`).

## Features

- The factory UI in JS or AOT mode at 60 Hz: the startup, info and function
  screens and the 14 function windows (`apps/corep4x-factory/README.md`).
- Display and touch: the 480x480 ST7102 MIPI-DSI panel, mounted upside down
  and drawn turned 180 degrees, and the CST3530 touch controller.
- Power: battery voltage, charging current, eight switchable power rails,
  backlight, the power button, power off and restart; M5-Bus GPIO levels and
  configuration.
- IMU (LSM6DS3TR-C) with a 3D view of the board that follows its motion.
- RTC (RX8130CE): date and time set by hand or by NTP.
- Audio: music playback with a spectrum, 10 s recordings from the
  microphones with waveforms or spectrograms and their playback, the
  startup sound.
- microSD: listing, folder sizes, a speed test, card insertion and removal.
- Ethernet (IP101): DHCP and ping. I2C scans of the internal bus and Port A.
  IR send and receive (NEC).
- Deep sleep, woken by the power button, an RTC timer, touch or motion.

Not supported: Wi-Fi (the ESP32-C6 module A172 Stamp-AddOn C6 For P4), the
USB-A host, the CST3530 firmware update, and the factory's echo cancellation
and speech recognition.

## Build

ESP-IDF `>=6.0,<6.3`, Bun, and the rustup toolchain of
`third_party/pocketjs/rust-toolchain.toml` with
`riscv32imafc-unknown-none-elf`.

```sh
. $IDF_PATH/export.sh
cd programs/corep4x
# JS mode (CONFIG_M5_POCKET_APP_MODE_JS, the default), build directory build/
idf.py set-target esp32p4 build
# AOT mode (CONFIG_M5_POCKET_APP_MODE_AOT), build directory build-aot/
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot" set-target esp32p4 build
idf.py -p <port> flash monitor          # or -B build-aot ... flash monitor
```

The image needs an ESP32-P4 of chip revision 3.x
(`CONFIG_ESP32P4_REV_MIN_300`), the one the CoreP4X carries. AOT mode keeps
its own `dependencies.aot.lock`. `aot/src/lib.rs` maps the board values to
the `device.ts` fields of the application; the two lists change together.

## Board

| Part | Detail |
| --- | --- |
| SoC | Stamp-P4X: ESP32-P4NRW32X, chip revision 3.x, 32 MiB PSRAM at 200 MHz, 16 MiB flash |
| Panel | 2.64" 480x480 ST7102, MIPI-DSI 2 lanes at 700 Mbps, DPI 24 MHz, RGB565 framebuffer in PSRAM; mounted upside down |
| Touch | CST3530 at 0x58, interrupt GPIO1, up to 5 contacts |
| Backlight | M5IOE1 IO9, PWM channel 0 at 50 kHz |
| Power | M5PM1 at 0x6E (battery, charger, M5-Bus 5 V boost, power button, wake sources, IRQ to GPIO0); M5IOE1 expander (rails, resets, microSD detect, charger status) |
| Audio | ES8311 DAC (0x18) + AW8737A amplifier, ES7210 4-channel ADC (0x40); I2S0 at 16 kHz |
| Sensors | LSM6DS3TR-C IMU (0x6A or 0x6B), RX8130CE RTC (0x32) |
| Storage | microSD on SPI3 (CS 50, MOSI 7, MISO 8, CLK 10) at 20 MHz; SPIFFS `storage` partition |
| Network | IP101 Ethernet PHY over RMII; optional ESP32-C6 module (A172 Stamp-AddOn C6 For P4) on SDIO, not driven |
| Interfaces | IR TX GPIO49 / RX GPIO13; Port A I2C (SDA 18, SCL 16); M5-Bus GPIOs; USB-A host port (not driven) |

Partitions (`partitions.csv`, 16 MiB flash): `nvs` 24 KiB, `phy_init` 4 KiB,
`factory` 12 MiB, `storage` 3 MiB SPIFFS mounted at `/webspiffs` for the
REC window's recording. The flash image carries an empty SPIFFS for
`storage`.

## Device bridge

`main/services.c` publishes the board values before every frame and passes
each request of the application to the service module that owns it.

| Module | Requests | Values and events |
| --- | --- | --- |
| `svc_power.c` | `rail <L3B\|LCD\|AudioIC\|AudioPA\|USBA\|P4USBC\|Grove\|M5BUS> <0\|1>`, `lcdOff`, `backlight <1..100>`, `charge <0\|1\|2>`, `powerPage <0\|1>`, `gpioConfig <gpio> <out> <level> <opendrain> <pull>`, `powerOff`, `restart [1]`, `sleepButton <0\|1>`, `sleepWake <s> <touch> <imu>`, `deepSleep <s> <touch> <imu>` | `pm1Ready`, `pybReady`, `firmwareVersion`, `rail<Name>` (read back every second), `backlight`, `charge`, `vbatFiltered`, `gpioLevels`, `gpioConfigError`, `sleepError`; events `gpioConfig`, `powerOff` |
| `svc_rtc.c` | `timerPage <0\|1>`, `rtcSet Y M D h m s`, `rtcNtp` | `rtcReady`, `rtcYear`..`rtcSecond` (every second), `ntpReady` |
| `svc_storage.c` | `sdList <path>`, `sdTest` | `sdReady`, `sdStatus`, `sdListing`, `sdPathError`, `sdFolderSize`, `sdTestText`; events `sdListed`, `sdTest` |
| `svc_i2c.c` | `i2cScan <0\|1>` | `i2cAddresses`; event `i2cScan` |
| `svc_canvas.c` | `touchCanvas <0\|1>`, `touchCanvasPause <0\|1>`, `touchCanvasClean`, `touchIndicator <0\|1>` | `touchVisited`, `touchPillDark`, `touchTexture`, `touchStatus`, `touchIndicator` |
| `svc_imu.c` | `imu <0\|1>`, `imuRecenter` | `imuState`, `imuError`, `imuTexture`, `imuMarkers`, `imuMarker{0,1}{X,Y}` |
| `svc_audio.c` | `rec <0\|1>`, `recView <spectrum> <ref>`, `recStart`, `recEnd`, `recPlay`, `recPlayStop`, `music <0\|1>`, `musicPlay`, `musicStop`, `volume <0..100>`, `bootSound <0\|1>` | `recState`, `recStopping`, `recPlayStopping`, `recBusy`, `recPlayBusy`, `recSpectrum`, `recDb0..2`, `recTexture`, `musicState`, `musicElapsedMs`, `musicDurationMs`, `musicBars`, `volume`, `bootSound` |
| `svc_net.c` | `pingPage <0\|1>`, `ping <mask>` | `ethState`, `ethIp`, `ethGateway`, `pingRunning`, `pingRows`, `pingSummary`, `pingSummaryTone` |
| `svc_ir.c` | `ir <0\|1>`, `irInit`, `irSend <address> <command> <repeat> <auto>`, `irClear` | `irReady`, `irInitializing`, `irSending`, `irResult`, `irResultTone`, `irFrames`, `irAddress`, `irCommand`, `irRepeat`; event `irNext` |

The host publishes `pointerX`, `pointerY` and `pointerDown`; `services.c`
publishes `hostAttached`. Preferences are the factory's NVS namespace
`corep4_prefs` (`boot_sound`, `touch_ind`).

The board draws three pictures into program textures
(`m5_pocket_app_set_texture`): the TOUCH canvas (`svc_canvas.c`), the IMU
view with the textured device model (`imu_view.c`, a C port of the
factory's Mahony filter and renderer) and the REC charts (`audio_draw.c`).
