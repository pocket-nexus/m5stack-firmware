# Repository structure

```text
M5Stack-sample/
├─ third_party/pocketjs/      PocketJS checkout pinned to v0.17.0 (submodule)
├─ platforms/esp-idf/         ESP-IDF components owned by this repository
│  └─ m5_bsp_<board>/         board support taken from M5Stack sources
├─ platforms/rust/            Rust crates of the shared AOT host
├─ programs/<board>/          IDF project: main, host contract, host config,
│                             sdkconfig.defaults, partitions
├─ apps/<board>-factory/      factory demo application (TSX views, TS models)
├─ tools/                     formatter and repository scripts
└─ docs/                      reference documentation
```

## Ownership

**Board support is per board and has no PocketJS dependency.**
`m5_bsp_<board>` brings up the hardware (power, I2C, expanders, panel, touch,
buttons, sensors, audio) and returns ESP-IDF handles.

**The program composes and configures.** `programs/<board>/main` creates the
board devices, wraps them in PocketJS display and input drivers and starts the
application. `pocket.host.json` is the board's host contract;
`pocket.host.config.ts` names it and holds the application in its `app`
section.

**The application is one TSX/TS project per board.** Its sources stay inside
the MicroTS admission subset so the same files build in JS mode and in AOT
mode.
