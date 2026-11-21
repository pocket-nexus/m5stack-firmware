# Application host (m5_pocket_app)

**`platforms/esp-idf/m5_pocket_app` runs the one application of a program in
the mode the Kconfig choice selects, with the board's display and inputs, and
carries board data to the application and requests back.** The program keeps
board bring-up; it passes handles and callbacks to `m5_pocket_app_start`.

| Mode | Kconfig | Application | Runtime |
| --- | --- | --- | --- |
| JS (default) | `CONFIG_M5_POCKET_APP_MODE_JS` | bundle (`.pkb`) in the QuickJS guest | `pocket_runtime` archive + `pocket_guest_quickjs` |
| AOT | `CONFIG_M5_POCKET_APP_MODE_AOT` | MicroTS-compiled Rust static library | the application library drives `pocket_runtime::Runtime` |

AOT mode status: see [AOT mode](#aot-mode).

## Files

| Path | Content |
| --- | --- |
| `platforms/esp-idf/m5_pocket_app/components.cmake` | Top-level include: resolves the mode, adds the PocketJS component directories |
| `platforms/esp-idf/m5_pocket_app/project_include.cmake` | `m5_pocket_app()` CMake helper |
| `platforms/esp-idf/m5_pocket_app/include/m5_pocket/app.h` | Host C API |
| `platforms/esp-idf/m5_pocket_app/include/m5_pocket/bridge.h` | Device bridge C API |
| `platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts` | JS-mode bridge for `main.tsx` |
| `tools/pocket` | Pocket CLI of the submodule (`bun third_party/pocketjs/tools/pocket.ts`) |
| `tests/aot-smoke/` | Smoke program: the reference adoption |

## Adopt the component in a program

### Project CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../platforms/esp-idf/m5_pocket_app/components.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(<board>)
```

**IDF resolves component requirements before it loads Kconfig values**, so a
component cannot choose its `REQUIRES` from `CONFIG_*`. `components.cmake`
reads `CONFIG_M5_POCKET_APP_MODE_*` from `sdkconfig`, or from the
`sdkconfig.defaults` files (`SDKCONFIG_DEFAULTS`, each with its
`.<target>` variant, later files winning) when `sdkconfig` does not exist yet,
and stores it in the `M5_POCKET_APP_MODE` build property. It then adds to
`EXTRA_COMPONENT_DIRS`:

- `m5_pocket_app`, `pocket_runtime`, `pocket_display_esp_lcd`,
  `pocket_input_gpio`, `pocket_input_touch`;
- on ESP32-P4: `pocket_display_esp_dpi`, `pocket_render_esp_ppa`;
- in JS mode only: `pocket_guest_quickjs` (and through it the
  `espressif/quickjs-ng` managed component).

It sets these cache defaults when they are not defined: `POCKETJS_CLI` to
`tools/pocket`; in JS mode `POCKETJS_RUST_FROM_SOURCE=ON` (the runtime archive
is built from `third_party/pocketjs/programs/esp-idf/archive`) and, for
ESP32-S3, `POCKETJS_CARGO=~/.rustup/toolchains/esp/bin/cargo`. ESP32-P4 uses
the `cargo` on `PATH`; rustup selects the channel of
`third_party/pocketjs/rust-toolchain.toml` (1.98.1) from the archive
directory.

The component checks that the Kconfig value it is compiled with equals the
build property. After changing the mode in menuconfig, run
`idf.py reconfigure` (or `fullclean`).

### main/CMakeLists.txt

```cmake
idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES m5_pocket_app)
m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME <name>                            # [a-z][a-z0-9_]*
    HOST_CONFIG "../pocket.host.config.ts"
    [PROJECT_ROOT <dir>]                   # default: the host config's directory
    [AOT_CRATE <dir>]                      # default: <project>/aot
    [DEPENDS <file>...])
```

`m5_pocket_app()` embeds the `app` section of the host config and generates
`m5_pocket_app_embedded()` into the calling component. In JS mode it calls
`pocketjs_compile_app` (bundle + `pocket_profile.h`; rebuilt from its
dependency file when an application source, the host contract or the host
config changes).

The host config sits in the application directory (`AGENTS.md`): the Pocket
CLI rejects `..` in the app `entry`, and the AOT model check covers only files
under the config's directory. A program therefore names it across
directories, and the config names the program's host contract by relative
path:

```cmake
m5_pocket_app(TARGET ${COMPONENT_LIB} NAME sticks3
    HOST_CONFIG "../../../apps/sticks3-factory/pocket.host.config.ts")
```

```ts
// apps/sticks3-factory/pocket.host.config.ts
export default defineHostConfig({
  host: "../../programs/sticks3/pocket.host.json",
  app: { entry: "main.tsx", framework: "solid", aot: true, model: "compiled", /* ... */ },
});
```

`PROJECT_ROOT` defaults to the host config's directory. Bundle intermediates
and plans go to the build directory; `pocket compile` may still create an
ignored `.pocket/` or `dist/` beside the config.

### Kconfig

| Symbol | Default | Meaning |
| --- | --- | --- |
| `CONFIG_M5_POCKET_APP_MODE_JS` / `_AOT` | JS | Application mode (choice) |
| `CONFIG_M5_POCKET_APP_TASK_STACK` | 65536 | App task stack bytes |
| `CONFIG_M5_POCKET_APP_TASK_PRIORITY` | 5 | App task priority |
| `CONFIG_M5_POCKET_BRIDGE_SLOTS` | 48 | Distinct board → app value names |
| `CONFIG_M5_POCKET_BRIDGE_EVENTS` | 16 | Event FIFO depth |
| `CONFIG_M5_POCKET_BRIDGE_TEXT_MAX` | 96 | Longest text value, bytes |
| `CONFIG_M5_POCKET_BRIDGE_ARRAY_MAX` | 256 | Longest array value, elements |

An AOT-only program (for example AtomS3, no PSRAM) puts
`CONFIG_M5_POCKET_APP_MODE_AOT=y` in `sdkconfig.defaults`; its build never
adds the QuickJS guest component and never compiles a bundle.

## C API

`#include "m5_pocket/app.h"`.

```c
m5_pocket_app_config config;
m5_pocket_app_config_defaults(&config);      /* image, task defaults */
config.display.kind = M5_POCKET_DISPLAY_ESP_LCD;
config.display.lcd = (pocket_display_esp_lcd_config){
    .panel = panel, .io = io, .width = 135, .height = 240,
    .strip_rows = 20, .buffer_count = 2,
    .frame_buffer_budget = 2 * 135 * 20 * 2, .byte_swap = true};
config.input.context = board;
config.input.buttons = board_buttons;        /* POCKET_BTN_* mask */
config.input.touch = board_touch;            /* optional */
config.before_frame = board_publish;         /* sensors -> bridge */
config.on_request = board_request;           /* app requests */
config.hook_context = board;
m5_pocket_app *app;
ESP_ERROR_CHECK(m5_pocket_app_start(&config, &app));
```

`m5_pocket_app_start` creates the display adapter, the renderer and the input
sources, creates the app task, boots the application on it and returns after
the first frame was presented (or with the boot error). The task then ticks at
the cadence the booted application reports, calling `before_frame`, one
runtime tick, the request handler for each request, and `after_frame`.
`m5_pocket_app_stop` joins the task and destroys runtime, renderer, display
adapter and the host's input source; panel, IO and touch handles stay the
program's. `m5_pocket_app_get_stats` reports frames, errors, skipped slots,
the longest frame and the smallest unused stack seen.

### Displays

| `display.kind` | Use | Object |
| --- | --- | --- |
| `M5_POCKET_DISPLAY_ESP_LCD` | SPI/QSPI/I80 panel with GRAM (CO5300 466×466, ST7789V2 135×240, GC9107 128×128) | upstream `pocket_display_esp_lcd`, RGB565 strips, config in `display.lcd` |
| `M5_POCKET_DISPLAY_ESP_DPI` | ESP32-P4 MIPI-DSI framebuffer (Tab5) | upstream `pocket_display_esp_dpi`, config in `display.dpi` |
| `M5_POCKET_DISPLAY_CUSTOM` | Any `pocket_display_output_v1` the program owns | `display.custom`, `custom_context`, `custom_buffer_bytes` |

On ESP32-P4, `display.ppa = true` registers the PPA renderer
(`pocket_render_esp_ppa`, scale = physical ÷ logical width, mask budget
`ppa_mask_budget` or half the display buffer bytes); otherwise the software
rasterizer draws. `struct_size` of the embedded configs is set by the host.
The display's buffer bytes become the runtime's `frame_buffer_budget`.

### Inputs

All callbacks run on the app task once per frame, before the frame, and are
optional:

| Callback | Input class | Meaning |
| --- | --- | --- |
| `buttons(ctx)` | buttons | `POCKET_BTN_*` bits held now (`pocket_spec.h`) |
| `touch(ctx, &x, &y)` | contact | One contact in logical pixels; true while touched |
| `axes(ctx, deltas)` | relative axis | Deltas of axes 0 and 1, signed millidegrees |
| `motion(ctx, state)` | motion | Fill a `pocket_motion_state`; false when no new estimate |

The host always declares the buttons class. With `touch`, it publishes the
bridge values `pointerX`, `pointerY` (logical pixels) and `pointerDown` (0/1)
every frame in both modes, so models can implement swipes and drags from
`device.ts` fields. `input.sources`/`source_contexts`/`source_count` (up to 7)
register further `pocket_input_source_v1` objects, such as
`pocket_input_gpio` or `pocket_input_touch`, in both modes.

### Memory and task

| Field | Default | Note |
| --- | --- | --- |
| `heap_limit` | 1 MiB | Runtime heap limit |
| `js_heap_limit` | 2 MiB | QuickJS heap limit (JS mode) |
| `js_stack_limit` | 64 KiB | QuickJS stack limit; keep below the task stack |
| `js_prefer_psram` | false | QuickJS allocations from PSRAM first |
| `task_stack_bytes` | `CONFIG_M5_POCKET_APP_TASK_STACK` | App task stack |
| `task_stack_caps` | 0 (internal RAM) | `MALLOC_CAP_SPIRAM` puts the stack in PSRAM; then no flash writes from the hooks |
| `task_priority`, `task_core` | Kconfig, -1 | `-1` = no affinity |

Runtime allocations go through `pocket_host_alloc` (`pocket_runtime/src/hooks.c`):
PSRAM first, internal RAM when there is no PSRAM or it is full.

## Device bridge

**Every host-provided value and every request to the host lives in the
application's state module `apps/<board>-factory/device.ts`.** The bridge
moves values by name; a name is a `device.ts` field name. The application
sees the same fields in both modes.

### Board side (`m5_pocket/bridge.h`)

| Function | Direction | Delivery |
| --- | --- | --- |
| `m5_pocket_bridge_set_number(name, double)` | board → app | Latest value, once per change |
| `m5_pocket_bridge_set_bool(name, bool)` | board → app | Number 0/1 |
| `m5_pocket_bridge_set_text(name, utf8)` | board → app | Latest value, once per change, ≤ `TEXT_MAX` bytes |
| `m5_pocket_bridge_set_array(name, float*, n)` | board → app | Every publication, ≤ `ARRAY_MAX` elements |
| `m5_pocket_bridge_post_event(name, double)` | board → app | One-shot, FIFO, each exactly once |
| `on_request(ctx, const m5_pocket_request *)` | app → board | Each request, in order, on the app task |

Publishing functions are thread-safe (a FreeRTOS mutex) and can be called
from any task, not from an ISR. Before each frame the host delivers every
value changed since the previous frame, then every queued event. A value
published several times between two frames arrives once with its latest
content; events are not merged. A full slot table or event FIFO returns
`ESP_ERR_NO_MEM` and is counted in `m5_pocket_bridge_get_stats`.

A request is a string `"<name> <arg> <arg>..."`. The host parses it into
`m5_pocket_request`: `text`, `name`, and `args[]`/`argc` with the following
words that parse as numbers (at most 8). `text` holds the whole string for
requests with text arguments.

### device.ts

```ts
import { clear, copy, i32, push, type f32, type f64 } from "@pocketjs/framework/solid/std";

// Board -> app
export let battery: i32 = 0;
export let charging: boolean = false;
export let clock: string = "--:--";
export let spectrum: f32[] = [];
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;
export let pointerDown: boolean = false;
export let alarms: i32 = 0;

// App -> board
export let requests: string[] = [];
export function request(command: string): void { push(requests, command); }

// Host entry points (JS mode); one case per board -> app field.
export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "battery": battery = i32(value); break;
    case "charging": charging = value !== 0; break;
    // ...
  }
}
export function hostText(name: string, value: string): void { /* switch */ }
export function hostArray(name: string, values: f32[]): void { /* switch */ }
export function hostEvent(name: string, value: f64): void { /* switch */ }
export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}
```

Conventions:

- `device.ts` is a MicroTS state module: top-level `let` fields with explicit
  types, synchronous functions, no signals. Only the root model (`app.ts`) and
  other state modules import it; factories do not.
- A field the board writes has the bridge name of the C call. Supported field
  types: integer and float numbers, `boolean` (published as 0/1), `string`,
  `f32[]`/`i32[]` (arrays). An event is a number handled in `hostEvent`,
  usually by counting or queueing into a field.
- The application writes requests only through `request(...)`. Level
  requests (brightness, volume) are requests too: `request("brightness 80")`.
- Views read `device.ts` through root-model functions, for example
  `export function status(): string { return \`${device.battery}%\`; }` in
  `app.ts`, used as `{status()}`. Plain fields are not signals: memos must not
  read them.
- The root model re-exports the five host entry points, because `main.tsx` can
  only reach root exports (a module imported by `main.tsx` directly would be a
  separate copy, not the model's state):

```ts
// app.ts
import * as device from "./device";
export function hostNumber(name: string, value: f64): void { device.hostNumber(name, value); }
export function hostText(name: string, value: string): void { device.hostText(name, value); }
export function hostArray(name: string, values: f32[]): void { device.hostArray(name, values); }
export function hostEvent(name: string, value: f64): void { device.hostEvent(name, value); }
export function takeRequests(): string[] { return device.takeRequests(); }
```

### main.tsx (JS mode only)

```tsx
import { mount } from "@pocketjs/framework/solid";
import { connectDevice } from "../../platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts";
import App from "./app.tsx";
import * as host from "./app.ts";

mount(() => {
  connectDevice(host);
  return <App />;
});
```

AOT compiles `App` and its models, not `main.tsx`. In JS mode the host
installs `globalThis.m5` after boot:

| Member | Behaviour |
| --- | --- |
| `m5.sync(number, text, array, event)` | Calls the callback of each changed value's kind with `(name, value)`, then `event(name, value)` per queued event; returns the call count |
| `m5.request(text)` | Passes one request to the program's handler; `true` when a handler took it |
| `m5.mode` | `"js"` |

`connectDevice` registers an `onFrame` callback that calls `m5.sync` with the
root exports and passes every string from `takeRequests()` to `m5.request`.
Field writes made through model functions are settled in the same frame, so
views that read them update in that frame. A request made by a handler during
dispatch is delivered at the next frame's callback.

## AOT mode

AOT mode (Rust library, CMake build, both targets) is in progress on
`feat/app-host`; this section will describe it when it lands.

## Validation

`tests/aot-smoke` builds the host with a transfer-only display. See its
README for commands and results. No M5Stack hardware was used: these are
firmware builds, not device validation.
