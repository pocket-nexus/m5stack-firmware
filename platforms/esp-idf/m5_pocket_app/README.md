# m5_pocket_app

PocketJS application host shared by the M5Stack programs: the JS/AOT mode
switch (`CONFIG_M5_POCKET_APP_MODE`), the `m5_pocket_app()` CMake helper, the
C API that runs the application with the board's display and inputs
(`m5_pocket/app.h`) and the device data bridge (`m5_pocket/bridge.h`,
`js/m5-bridge.ts`).

Targets: esp32p4, esp32s3. ESP-IDF `>=6.0,<6.3`.

The adoption guide, API reference and limits are in
[docs/APP-HOST.md](../../../docs/APP-HOST.md). `tests/aot-smoke` is the
reference program.

| File | Role |
| --- | --- |
| `components.cmake` | Included by the project CMakeLists.txt: mode, component directories, CLI and Cargo defaults |
| `project_include.cmake` | `m5_pocket_app()` |
| `aot_archive.cmake` | Copies and prepares the AOT library for the link |
| `src/app.c` | Display, renderer and input creation; app task and frame loop |
| `src/app_js.c` | JS mode: runtime, QuickJS guest, `globalThis.m5` |
| `src/app_aot.c` | AOT mode: calls the library through `m5_pocket/aot.h` |
| `src/bridge.c` | Value slots, event FIFO, request parsing |
| `src/input.c` | The program's input callbacks as one `pocket_input_source_v1` |
| `src/internal_call.c` | Helper task with an internal-RAM stack for an app task whose stack is in PSRAM |
