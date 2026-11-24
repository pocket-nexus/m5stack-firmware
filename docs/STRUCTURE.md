# Repository structure

```text
m5stack-firmware/
├─ third_party/pocketjs/      PocketJS checkout pinned to 618c1ba9, after v0.17.0 (submodule)
├─ platforms/esp-idf/         ESP-IDF components owned by this repository
│  ├─ m5_bsp_<board>/         board support taken from M5Stack sources
│  └─ m5_pocket_app/          application host: JS/AOT mode, device bridge,
│                             display and input composition
├─ platforms/rust/
│  ├─ m5_pocket_aot/          AOT host crate: runtime, adapters, bridge
│  └─ m5_pocket_aot_build/    build-script helper of the per-app AOT crates
├─ programs/<board>/          IDF project: main, host contract,
│                             sdkconfig.defaults, partitions
│  └─ aot/                    the program's AOT library crate
├─ apps/<board>-factory/      factory demo application (TSX views, TS models),
│                             its host config (with app.fonts) and fonts/
├─ apps/sticks3-demos/        StickS3 application: demos of the board in place
│                             of a factory recreation, same layout
├─ tests/                     aot-smoke (both modes, both targets) and tool tests
├─ tools/                     Pocket CLI launcher, formatter, frame profiler host
│                             tool, simulator screenshots (screenshots.ts)
└─ docs/                      reference documentation (APP-HOST.md: the host)
   └─ images/                 README screenshots, written by tools/screenshots.ts
```

## Ownership

**Board support is per board and has no PocketJS dependency.**
`m5_bsp_<board>` brings up the hardware (power, I2C, expanders, panel, touch,
buttons, sensors, audio) and returns ESP-IDF handles.

**The program composes and configures.** `programs/<board>/main` creates the
board devices, wraps them in PocketJS display and input drivers and starts the
application. `programs/<board>/pocket.host.json` is the board's host
contract. The application's `pocket.host.config.ts` names that contract by
relative path and holds the application in its `app` section; it lives beside
the sources because the Pocket CLI rejects `..` in the app entry path and the
AOT model check covers only files under the config's directory.

**The application host is shared.** `m5_pocket_app` runs the program's one
application in the mode Kconfig selects (`CONFIG_M5_POCKET_APP_MODE_JS`, the
default, or `CONFIG_M5_POCKET_APP_MODE_AOT`) with the board's display and
inputs, and moves board values and application requests through the device
bridge. See [APP-HOST.md](APP-HOST.md).

**The application is one TSX/TS project per board.** Its sources stay inside
the MicroTS admission subset so the same files build in JS mode and in AOT
mode.
