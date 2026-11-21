# m5_pocket_aot

The Rust half of `m5_pocket_app` in AOT mode (`no_std` + `alloc`). A board's
per-app crate depends on it, implements `DeviceApp` for its generated
`AppApp` and calls `export_app!`; this crate provides:

- `create`/`tick`/`cadence_hz`/`destroy`: the `m5_pocket/aot.h` C ABI. They
  build a `pocket_runtime::Runtime` from the exported profile, register an
  `AotGuest`, the software rasterizer (`pocket_raster::RasterRenderer`), and
  the C display, renderer and input sources m5_pocket_app created, adapted by
  `pocket_drivers::abi::{CDisplay, CRenderer, CInputSource}`; then
  `boot_aot_deferred` and `present`.
- `Hosted<D>`: the `AotApp` that, every frame, writes changed bridge values and
  events into `device.ts` (then `invalidate`), translates the guest frame into
  `microts::Input` (`input::FrameInput`, touch presses included), runs the
  frame and passes the queued request strings to the program.
- `bridge`, `Field`, `device_fields!`: typed access to `m5_pocket/bridge.h`.
- `M5Host`: a `microts::Host` that admits every input channel.
- `load_font_in_place`: font atlases read from flash instead of heap copies.
- On the device (`target_os = "none"`): the global allocator over
  `pocket_host_alloc`/`realloc`/`free` and the panic handler over
  `pocket_host_panic` (`pocket_runtime/src/hooks.c`).
- On ESP32-P4: the `pocket_render_esp_ppa` C factory (`cabi` feature), which
  the C PPA renderer component calls.

`../m5_pocket_aot_build` is the per-app crate's `build.rs` support. The
template per-app crate is `tests/aot-smoke/aot`; see
[docs/APP-HOST.md](../../../docs/APP-HOST.md).
