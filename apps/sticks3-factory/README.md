# sticks3-factory

PocketJS recreation of the M5Stack StickS3 factory startup UI: the UIFlow2
startup menu, `StickS3_Startup` in
[m5stack/uiflow-micropython](https://github.com/m5stack/uiflow-micropython)
`m5stack/modules/startup/sticks3.py`. Built by `programs/sticks3`.

## Which factory firmware

StickS3 has no `*-UserDemo` repository. The firmware recreated here is
UIFlow2 MicroPython with boot option 1 ("Show startup menu and network
setup"). Evidence:

- The StickS3 UIFlow2 quick start
  (<https://docs.m5stack.com/en/uiflow2/sticks3/program>) describes the screen
  a StickS3 shows with UIFlow2 and that boot option: "WiFi OK" / "WiFi NG"
  on the device screen, and "press the Button on the front of the device to
  view the currently generated valid Access Code". That is `CloudApp` in
  `sticks3.py`, which the StickS3 startup starts first
  (`fw.start(cloud_app)`).
- `m5stack/modules/startup/__init__.py` selects `StickS3_Startup` for
  `M5.BOARD.M5StickS3` when `boot_option` is 1, and `m5stack/fs/user/boot.py`
  defaults `boot_option` to 1 when NVS has none, i.e. on a freshly flashed
  device.
- The board directory `m5stack/boards/M5STACK_StickS3/` ships the startup
  artwork (`files/system/sticks3/*.jpg`, 135×240) and includes
  `manifest_sticks3.py`, which freezes `startup/sticks3.py`.
- The product page (<https://docs.m5stack.com/en/core/StickS3>, SKU K150) and
  the SKU page list UiFlow2, Arduino, ESP-IDF, ESP-Claw and Xiaozhi as
  software. Neither page names a preinstalled image, so the UIFlow2 startup
  is the best-documented candidate, not a confirmed one.

Source revision: uiflow-micropython `a61a29409f09f1f4bebae05e15c4bd6dcc4956b6`
(2026-11-18).

## Screens and navigation

| Screen | Factory class | Artwork | Live content |
| --- | --- | --- | --- |
| Cloud (boot screen) | `CloudApp` | `a11.jpg` | WiFi OK/NG and SERVER OK/NG tags at (98, 27) and (98, 55); access code and nickname |
| Launcher, CLOUD | `LauncherApp` | `1a.jpg` | Wi-Fi/cloud state dot (6, 6), charge icon (59, 3), battery `NN%`, firmware version |
| Launcher, USB | `LauncherApp` | `1b.jpg` | as above |
| Launcher, SETUP | `LauncherApp` | `1e.jpg` | as above |
| USB | `UsbApp` | `usb.jpg` | battery level next to the artwork's `%` |
| Setup, access point | `SetupApp` `_STATE_AP` | `a1.jpg` | AP name `S3_XXXX`, underlined |
| Setup, portal | `_STATE_WEB` | `a2.jpg` | QR code for `http://192.168.4.1` at (13, 49), 109 px |
| Setup, select Wi-Fi | `_STATE_WAIT_WIFI` | `a3.jpg` | — |
| Setup, Wi-Fi error | `_STATE_WIFI_NG` | `a4.jpg` | — |
| Setup, Wi-Fi linked | `_STATE_WIFI_OK` | `a7.jpg` | — |
| Setup, server error | `_STATE_SERVER_NG` | `a9.jpg` | — |
| Setup, server linked | `_STATE_SERVER_OK` | `a11.jpg` | as Cloud |

Keys, as in `Framework.run`: KEY1 (front, BtnA) single click plays 4000 Hz
for 50 ms and enters; KEY1 double click plays 3500 Hz and goes back; KEY2
(side, BtnB) single click plays 6000 Hz and moves to the next launcher icon.
A click is decided 500 ms after the last press (M5Unified `Button_Class`
hold threshold), so a single click acts after that delay, as on the device.
Every app other than the launcher returns to the launcher on KEY1, and the
launcher restarts at CLOUD each time it opens (`LauncherApp.on_launch`).
Startup plays 4000 Hz for 50 ms.

The live values refresh every 100 ms (UIFlow: 100 ms in Setup, 500 ms in the
launcher, 1000 ms in Cloud and USB).

## Files

| File | Role |
| --- | --- |
| `pocket.host.config.ts` | Host config: `app` section (Solid, `aot: true`, `model: "compiled"`, 30 Hz) and the program's host contract `../../programs/sticks3/pocket.host.json` |
| `main.tsx` | JS-mode `mount()` entry |
| `app.tsx`, `app.ts` | Root view and model: pages, key decoding, setup state machine |
| `Launcher.tsx`, `Cloud.tsx`, `Usb.tsx`, `Setup.tsx` | Screens; props only |
| `device.ts` | State module for host values and host requests (battery, charging, Wi-Fi and cloud status, AP name, portal clients, version; tone, volume, brightness, setup AP on/off) |
| `images/` | Factory artwork as PNG |
| `images.json` | Stores every image as RGB565 (`psm` 0) in the JS bundle |

`pocket.host.config.ts` lives here, not in `programs/sticks3/`: PocketJS
resolves `app.entry` relative to the config file and rejects `..`, and AOT
model admission registers the config's directory as the application
directory.

## Artwork

The PNGs are the JPEGs of `m5stack/boards/M5STACK_StickS3/files/system/sticks3/`
(MIT, M5Stack) decoded with ImageMagick and padded with black to
power-of-two sizes, which the PocketJS image baker requires (135×240 →
256×256, 16×22 → 16×32, 34×24 → 64×32). Each view clips the padding with an
`overflow-hidden` box of the original size.

| PNG | Factory file |
| --- | --- |
| `launcher-cloud.png`, `launcher-usb.png`, `launcher-setup.png` | `1a.jpg`, `1b.jpg`, `1e.jpg` |
| `cloud.png` | `a11.jpg` |
| `usb.png` | `usb.jpg` |
| `setup-ap.png`, `setup-web.png`, `setup-wifi.png`, `setup-wifi-ng.png`, `setup-wifi-ok.png`, `setup-server-ng.png` | `a1.jpg`, `a2.jpg`, `a3.jpg`, `a4.jpg`, `a7.jpg`, `a9.jpg` |
| `charge.png`, `no-charge.png` | `CHG.jpg`, `noCHG.jpg` |
| `state-wifi-never-set.png`, `state-wifi-ng.png`, `state-wifi-ok-server-ng.png`, `state-wifi-ok-server-ok.png` | `wifiNeverSet.jpg`, `wifiNG.jpg`, `wifiOKServerNG.jpg`, `wifiOKServerOK.jpg` |
| `ng.png`, `wifi-ok.png`, `server-ok.png` | `ng.jpg`, `wifi_ok.jpg`, `server_ok.jpg` |
| `setup-qr.png` | Generated: M5GFX `src/lgfx/utility/lgfx_qrcode.c` (`28d47949`) encoding `http://192.168.4.1` at ECC level 0, version 2 (25 modules), drawn as `LGFXBase::qrcode(text, 13, 49, 109, 1)` draws it (4 px modules, 4 px white border), padded to 128×128 |

Unused factory files: `1c.jpg` (APP.LIST), `1d.jpg` (LoRa.CHAT) and
`placeholder.jpg` belong to apps the StickS3 startup comments out; `a5`,
`a6`, `a8`, `a10` are alternative server-host variants it never shows.

## Known differences from the factory firmware

- **Fonts.** UIFlow draws the labels in Montserrat 14/18/24. PocketJS bakes
  Inter for application text and `pocket compile` has no font override, so
  the labels use Inter at the same pixel sizes (`text-sm`, `text-lg`,
  `text-2xl`). Glyph widths and baselines differ by a few pixels.
- **Nickname truncation.** UIFlow shortens a nickname wider than 129 px with
  `...` in the middle (`Label.LONG_DOT`); here it is clipped at 129 px.
- **UIFlow cloud.** The firmware has no M5Things client (UIFlow's
  `_HAS_SERVER = False` path): SERVER always shows NG, the access code and
  nickname stay empty, and Setup ends in the server-error screen after a
  successful Wi-Fi connection. The launcher state dot shows black (no SSID),
  red (Wi-Fi not connected) or yellow (Wi-Fi connected, no server).
- **Wi-Fi provisioning.** The screens and the state machine are reproduced;
  the access point, captive DNS, portal page and station connection belong
  to the firmware host and reach the app through `device.ts`. Until the
  device data bridge is connected, `device.ts` keeps its defaults: battery
  100 %, not charging, no SSID, AP name `S3_0000`, so Setup stays on the
  access-point screen and no tone is played by the app.
- **Firmware version.** The launcher shows `device.firmwareVersion`
  (`v0.1.0`) instead of the UIFlow version string.
- **Key timing.** Clicks are counted on key presses; M5Unified counts
  releases and ignores holds longer than 500 ms.
- **JPEG decoding.** The artwork is decoded once at build time by
  ImageMagick and stored as RGB565; UIFlow decodes the JPEGs on the device
  with M5GFX's decoder. Pixel values can differ by a few levels.

## Validation

- `bun run pocket compile --config apps/sticks3-factory/pocket.host.config.ts --aot --out <dir>`:
  AOT admission and source generation pass.
- JS bundle: 1.88 MB (`pak` 1.66 MB, script 222 KB).
- Sim screenshots (PocketJS `programs/sim` with the bundle, 135×240): boot
  Cloud screen, launcher CLOUD/USB/SETUP cycling, USB, double-click back and
  Setup access-point screen render the factory artwork with the live labels.
  Kept in `.pocket-build/validation/sticks3/`.
- No device run.
