# Application host (m5_pocket_app)

**`platforms/esp-idf/m5_pocket_app` runs the one application of a program in
the mode the Kconfig choice selects, with the board's display and inputs, and
carries board data to the application and requests back.** The program keeps
board bring-up; it passes handles and callbacks to `m5_pocket_app_start`.

| Mode | Kconfig | Application | Runtime |
| --- | --- | --- | --- |
| JS (default) | `CONFIG_M5_POCKET_APP_MODE_JS` | bundle (`.pkb`) in the QuickJS guest | `pocket_runtime` archive + `pocket_guest_quickjs` |
| AOT | `CONFIG_M5_POCKET_APP_MODE_AOT` | MicroTS-compiled Rust static library | the application library drives `pocket_runtime::Runtime` |

## Files

| Path | Content |
| --- | --- |
| `platforms/esp-idf/m5_pocket_app/components.cmake` | Top-level include: resolves the mode, adds the PocketJS component directories |
| `platforms/esp-idf/m5_pocket_app/project_include.cmake` | `m5_pocket_app()` CMake helper |
| `platforms/esp-idf/m5_pocket_app/include/m5_pocket/app.h` | Host C API |
| `platforms/esp-idf/m5_pocket_app/include/m5_pocket/bridge.h` | Device bridge C API |
| `platforms/esp-idf/m5_pocket_app/include/m5_pocket/aot.h` | C ABI of the AOT library |
| `platforms/esp-idf/m5_pocket_app/js/m5-bridge.ts` | JS-mode bridge for `main.tsx` |
| `platforms/rust/m5_pocket_aot/` | Shared AOT crate: runtime, adapters, bridge, allocator, panic handler |
| `platforms/rust/m5_pocket_aot_build/` | `build.rs` support of per-app crates |
| `tests/aot-smoke/aot/` | Per-app AOT crate template |
| `tools/pocket` | Pocket CLI of the submodule, run through `tools/pocket.ts`, which applies the app's `pocket.fonts.json` ([FONTS.md](FONTS.md)) |
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

### Compiler warnings

**IDF already passes `-Wall -Wextra -Werror`, then relaxes them** with
`-Wno-unused-parameter`, `-Wno-sign-compare`, `-Wno-enum-conversion` and
`-Wno-error=` for unused functions, variables, set-but-unused variables,
deprecated declarations and the `-Wextra` group. CMake drops a repeated
`-Wall`/`-Wextra`/`-Werror` from a component's options, so
`target_compile_options(... -Wall -Wextra -Werror)` does not restore them.
`m5_strict_warnings(<target>)` (defined by `m5_pocket_app`'s
`project_include.cmake`, available to every component of a program that uses
it) appends, after IDF's options: `-Wunused-parameter -Wsign-compare
-Wenum-conversion` (C) and `-Werror=` for `unused-function`,
`unused-variable`, `unused-but-set-variable`, `deprecated-declarations` and
`extra`. `m5_pocket_app` and `tests/aot-smoke/main` use it; board components
call it after `idf_component_register`:

```cmake
idf_component_register(SRCS ... REQUIRES ...)
m5_strict_warnings(${COMPONENT_LIB})
```

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
(`pocket_render_esp_ppa`) at the profile's raster density, with a glyph mask
budget of `ppa_mask_budget` bytes per renderer (default half the display
buffer bytes); otherwise the software rasterizer draws.
`display.ppa_lanes = 2` creates two PPA devices and two renderers behind
`pocketjs_esp32p4_ppa_lanes`: every strip is split at a multiple of
`ppa_row_alignment` raster rows (default `dpi.row_alignment` for the DPI
display, else 1); the app task renders the upper rows and a helper task pinned
to `ppa_helper_core` (default 1) the lower rows, at the same time. Keep each
half of a strip a multiple of the 64-byte DMA cache line, or that half renders
in software. `ppa_lanes` 0 or 1 is the single renderer. For Tab5 (two 48-row
× 1280-px RGB565 strips, 640×360 logical at raster density 2 turned onto the
720×1280 panel):

```c
config.display.kind = M5_POCKET_DISPLAY_ESP_DPI;
config.display.dpi = (pocket_display_esp_dpi_config){ /* framebuffer, 720, 1280,
    .width = 1280, .height = 720, .turned = true, .scale = 1,
    .strip_rows = 48, .row_alignment = 2,
    .frame_buffer_budget = 2 * 48 * 1280 * 2, ... */ };
config.display.ppa = true;
config.display.ppa_lanes = 2;
```

`struct_size` of the embedded configs is set by the host. The display's buffer
bytes become the runtime's `frame_buffer_budget`.

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

**In AOT mode MicroTS compiles the application's views and models into Rust,
and the per-app crate links them with `platforms/rust/m5_pocket_aot` into the
static library `libm5_pocket_app_aot.a`.** `pocket_runtime` links that library
in place of its default runtime archive (`components.cmake` sets
`POCKET_RUNTIME_ARCHIVE` to it), so the firmware contains one Rust archive,
no QuickJS and no bundle. `pocket_runtime/src/hooks.c` keeps serving the
library: its allocator is `pocket_host_alloc`/`realloc`/`free`, its panic
handler `pocket_host_panic`, its log `pocket_host_log`.

The C side is the same in both modes: `m5_pocket_app_start` creates the same
display, renderer and input objects and passes their vtables to
`m5_pocket_aot_create` (`m5_pocket/aot.h`). The library adapts them with
`pocket_drivers::abi::{CDisplay, CRenderer, CInputSource}`, registers the
software rasterizer and an `AotGuest`, calls
`Runtime::boot_aot_deferred(profile hash, profile viewport, false, TICK_HZ)`
and presents the first frame.

**On ESP32-P4, AOT mode uses the same PPA renderers as JS mode**, one renderer
or two lanes: the library exports the `pocket_render_esp_ppa_*` factory the C
component calls (as the JS-mode runtime archive does) and registers the C
renderer through `CRenderer`. This path is validated by firmware builds only;
a board that sees a problem on hardware sets `display.ppa = false` for the
software rasterizer.

### Build steps

`m5_pocket_app()` in AOT mode:

1. `pocketjs_profile_header` exports `pocket_profile.h`, `pocket.host.json`
   and `pocket_profile.rs` to `<build>/esp-idf/main/pocketjs/<name>/profile`.
2. `pocket compile --config <host config> --aot --out <build>/.../m5_pocket_app/<name>/aot`
   generates `include.rs`, `app.rs`, `app_model.rs`, `styles.bin` and the
   font atlases; its depfile reruns it when a source changes. The step needs
   the MicroTS layout oracle `third_party/pocketjs/platforms/web/pocketjs.wasm`
   (ignored by the submodule); when it is missing the build runs
   `bun tools/wasm.ts` in the submodule first (wasm32 target of 1.98.1).
3. Cargo builds the per-app crate on every build (Cargo decides what is
   stale) with `POCKETJS_AOT_DIR`, `POCKETJS_PROFILE_RS` and
   `CARGO_TARGET_DIR=<build>/m5_pocket_aot/cargo`:
   `rustup run <M5_POCKET_AOT_TOOLCHAIN> cargo build --release --locked --target <triple>`.

   | IDF target | Rust target | Toolchain (default of `M5_POCKET_AOT_TOOLCHAIN`) |
   | --- | --- | --- |
   | esp32s3 | `xtensa-esp32s3-none-elf` | `esp` (Espressif, rustc 1.97 nightly), `-Zbuild-std=core,alloc` |
   | esp32p4 | `riscv32imafc-unknown-none-elf` | channel of `third_party/pocketjs/rust-toolchain.toml` (1.98.1) |

4. `aot_archive.cmake` copies a new archive to
   `<build>/m5_pocket_aot/libm5_pocket_app_aot.a` and, for ESP32-P4, removes
   the soft-float compiler-rt members (`pocket_runtime/prepare_archive.cmake`;
   36 objects). An unchanged archive is not copied, so a no-op build does not
   relink.

The submodule's compiler pins (`tools/build/toolchains/esp-idf/compiler-pins.json`:
P4 1.93.0) apply to its release packaging, not to these Cargo calls; no
override is needed. Both modes are configured per build directory:

```sh
idf.py -B build-aot -DSDKCONFIG=build-aot/sdkconfig \
    -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.aot" set-target esp32s3 build
```

### Per-app crate

Each program has a thin crate, by default `programs/<board>/aot/`
(`AOT_CRATE`). Copy `tests/aot-smoke/aot/` and adjust the paths and the field
list:

```toml
[lib]
name = "m5_pocket_app_aot"          # fixed: m5_pocket_app links lib<name>.a
crate-type = ["staticlib"]

[dependencies]
m5-pocket-aot = { path = "../../../platforms/rust/m5_pocket_aot" }
microts = { path = "../../../third_party/pocketjs/engine/microts-runtime", features = ["runtime"] }
pocket-runtime = { path = "../../../third_party/pocketjs/engine/runtime" }

[build-dependencies]
m5-pocket-aot-build = { path = "../../../platforms/rust/m5_pocket_aot_build" }

[profile.release]
opt-level = "s"
panic = "abort"
overflow-checks = false
```

`build.rs` calls `m5_pocket_aot_build::emit()`, which copies the exported
profile to `OUT_DIR/pocket_profile.rs` and writes `OUT_DIR/m5_fonts.rs`
(`load_fonts_in_place`, see below). `src/lib.rs`:

```rust
#![no_std]
extern crate alloc;
use alloc::{boxed::Box, string::String, vec::Vec};
use m5_pocket_aot::{bridge::Value, device_fields, DeviceApp, M5Host};
use microts::{Input, Ui};
use pocket_runtime::{GuestEnvironment, RuntimeError};

#[allow(dead_code)]
mod profile { include!(concat!(env!("OUT_DIR"), "/pocket_profile.rs")); }
include!(concat!(env!("POCKETJS_AOT_DIR"), "/include.rs"));
include!(concat!(env!("OUT_DIR"), "/m5_fonts.rs"));

type App = generated::AppApp<generated::AppModel, M5Host>;
pub struct Board(Box<App>);

impl DeviceApp for Board {
    fn mount(mut ui: Ui, environment: &GuestEnvironment<'_>) -> Result<Self, RuntimeError> {
        if environment.tick_hz != TICK_HZ
            || !ui.load_styles(include_bytes!(concat!(env!("POCKETJS_AOT_DIR"), "/styles.bin")))
        {
            return Err(RuntimeError::InvalidConfiguration);
        }
        load_fonts_in_place(&mut ui);
        load_images(&mut ui);
        let mut app = Box::new(App::new(M5Host(ui), generated::AppProps {}, generated::AppModel::default()));
        let first = app.ui().initial_focus();
        app.ui_mut().set_focus(first);
        Ok(Self(app))
    }
    fn frame(&mut self, input: &Input) { self.0.frame(input); }
    fn invalidate(&mut self) { self.0.invalidate(); }
    fn ui_mut(&mut self) -> &mut Ui { self.0.ui_mut() }
    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        device_fields!(model, value, {
            "battery" => device_battery_mut,
            "charging" => device_charging_mut,
            "clock" => device_clock_mut,
        })
    }
    fn event(&mut self, event: &Value<'_>) -> bool {
        match event.name {
            "alarm" => { *self.0.model.device_alarms_mut() += event.number() as i32; true }
            _ => false,
        }
    }
    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(Board, profile, TICK_HZ);
```

The generated model exposes each exported `device.ts` field as
`device_<field>()` and `device_<field>_mut()`, with the field name as written
(`pointerX` → `device_pointerX_mut`). `device_fields!` assigns through the
`Field` trait: integers and floats (saturating `as`), `bool` (non-zero),
`String`, and `Vec` of numbers. The match is the AOT counterpart of the
`hostNumber`/`hostText`/`hostArray` switches in `device.ts`; keep both lists
equal. A model function exported by `app.ts` is not callable from Rust unless
a view uses it, which is why the AOT host writes fields directly.

Every frame `Hosted<D>` (in `m5_pocket_aot`) takes the changed bridge values
and events, calls `value`/`event`, calls `invalidate` when any field changed,
translates the guest frame into `microts::Input`, runs `frame`, then drains
`requests()` into the program's request handler.

### Input in AOT mode

`microts::Input` carries buttons, button edges, one press target, two relative
axes and a motion estimate. The host maps:

- buttons, axes 0/1 and motion directly (motion records become
  `microts::motion::MotionState`);
- a touch contact into a press, as the guest framework treats a tap: at
  touch-down the contact's hit node is walked up to its nearest focusable
  ancestor, which is armed and shows `active:`; lifting the contact over the
  same target sends a CIRCLE press with that target, lifting elsewhere
  cancels. The synthetic CIRCLE edge also reaches an
  `ActionHandler button={BTN.CIRCLE}`.

There are no gesture, drag or scroll events in AOT mode. Apps implement
swipes and drags in model code from the `pointerX`, `pointerY` and
`pointerDown` fields, which the host publishes from the touch callback in
both modes. The runtime's touch wire clamps coordinates to 0–1023, so a
logical viewport wider than 1024 pixels cannot be touched beyond x = 1023.

### Memory

- **No PSRAM is required.** `pocket_host_alloc` takes PSRAM first and
  internal RAM otherwise; display strip buffers are the program's (their
  bytes are the runtime's `frame_buffer_budget`); `heap_limit` bounds the
  runtime. The AOT build never links QuickJS.
- **Static RAM of the library is small**: the whole AOT smoke firmware for
  esp32s3 without PSRAM uses 12,036 B `.data` and 6,824 B `.bss` (IDF
  included). Generated styles, fonts and code are flash.
- **Fonts stay in flash** with `load_fonts_in_place` (`load_font_atlas_shared`).
  The generated `load_fonts` would copy each atlas into the heap (the smoke
  app's three atlases are 76 KB). `load_images` still uploads image pixels
  to the heap.
- **The app task's stack** defaults to 64 KiB (`CONFIG_M5_POCKET_APP_TASK_STACK`).
  The smoke app used at most 19,400 B of it under QEMU (see below); a larger
  view tree needs more for layout. `m5_pocket_app_get_stats` reports
  `stack_free_min`.

Measured under QEMU (esp32s3, no PSRAM, AOT smoke, 240×135, two 16-row
strips of 7,680 B each): internal heap free 377,872 B after the display
buffers were allocated, 273,956 B while running, minimum 270,744 B. The host,
the runtime, the app and the 64 KiB task stack together took at most
107,128 B of internal RAM. QEMU validates boot, the frame loop, input,
bridge and requests; it does not validate timing or a real panel.

## Validation

`tests/aot-smoke` builds the host with a transfer-only display in both modes
for esp32s3 and esp32p4; its README has the commands and results. The AOT
esp32s3 build (no PSRAM) also ran under Espressif's QEMU and passed. The JS
build does not reach `app_main` under QEMU: the USB-Serial-JTAG connection
monitor, which the JS build links through VFS, waits for a BBPLL calibration
that QEMU does not emulate. QEMU has no ESP32-P4 machine. No M5Stack hardware
was used: none of this is device validation.
