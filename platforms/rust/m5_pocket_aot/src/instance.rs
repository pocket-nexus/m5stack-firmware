//! The `m5_pocket/aot.h` functions, generic over the per-app [`DeviceApp`].

use crate::{bridge, input::FrameInput, sys, sys::log, DeviceApp};
use alloc::{boxed::Box, format};
use core::ffi::c_void;
use microts::{
    runtime::{AotApp, AotGuest, AppState},
    Ui,
};
use pocket_drivers::abi::{
    CDisplay, CInputSource, CRenderer, PocketDisplayOutputV1, PocketInputSourceV1, PocketRendererV1,
};
use pocket_raster::RasterRenderer;
use pocket_runtime::{
    GuestEnvironment, GuestFrame, Profile, RendererId, Runtime, RuntimeConfig, RuntimeError,
};

const STATUS_OK: i32 = 0;
const STATUS_INVALID: i32 = -2;
const STATUS_FAILED: i32 = -3;

/// `m5_pocket_aot_config`.
#[repr(C)]
pub struct AotConfig {
    pub struct_size: usize,
    pub display: *const PocketDisplayOutputV1,
    pub display_context: *mut c_void,
    pub renderer: *const PocketRendererV1,
    pub renderer_context: *mut c_void,
    pub inputs: *const *const PocketInputSourceV1,
    pub input_contexts: *const *mut c_void,
    pub input_count: usize,
    pub heap_limit: usize,
    pub frame_buffer_budget: usize,
}

/// The exported host profile and the generated tick rate.
pub struct AppFacts {
    pub profile: Option<Profile>,
    pub profile_hash: [u8; 32],
    pub host_abi: u32,
    pub tick_hz: u32,
}

/// The guest-side app: the per-app glue plus the bridge and input state.
pub struct Hosted<D> {
    app: D,
    input: FrameInput,
}

impl<D: DeviceApp> AotApp for Hosted<D> {
    fn mount(ui: Ui, environment: &GuestEnvironment<'_>) -> Result<Self, RuntimeError> {
        Ok(Self {
            app: D::mount(ui, environment)?,
            input: FrameInput::default(),
        })
    }

    fn frame(&mut self, frame: &GuestFrame<'_>) -> Result<(), RuntimeError> {
        // Board values and events into device.ts, before input dispatch.
        sys::enter(sys::stage::BRIDGE);
        let mut changed = false;
        let app = &mut self.app;
        bridge::for_each_value(|value| changed |= app.value(value));
        bridge::for_each_event(|event| changed |= app.event(event));
        if changed {
            app.invalidate();
        }
        sys::enter(sys::stage::TRANSLATE);
        let input = self.input.translate(frame, app.ui_mut());
        sys::enter(sys::stage::APP_INPUT);
        app.frame(&input);
        // Requests the frame queued, in order.
        sys::enter(sys::stage::REQUESTS);
        if let Some(requests) = app.requests() {
            for request in requests.drain(..) {
                bridge::request(&request);
            }
        }
        sys::enter(sys::stage::DRAW);
        Ok(())
    }
}

struct Instance<D> {
    runtime: Runtime,
    app: AppState<Hosted<D>>,
    cadence_hz: u32,
    failed: bool,
}

fn fail<T>(what: &str, error: impl core::fmt::Debug) -> Option<T> {
    log(&format!("m5_pocket_aot: {what}: {error:?}"));
    None
}

unsafe fn build<D: DeviceApp>(config: &AotConfig, facts: AppFacts) -> Option<Instance<D>> {
    let Some(profile) = facts.profile else {
        return fail("profile", "no exported profile");
    };
    // The application's logical viewport is the profile's first logical size.
    // `profile.viewport` is the physical panel (720x1280 on the Tab5, whose
    // application is 640x360 at raster density 2); the runtime does not admit
    // it as a logical viewport.
    let Some(&viewport) = profile.logical_viewports.first() else {
        return fail("profile", "no logical viewport");
    };
    let mut runtime = match Runtime::new(RuntimeConfig {
        profile_hash: facts.profile_hash,
        host_abi: facts.host_abi,
        heap_limit: config.heap_limit,
        frame_buffer_budget: config.frame_buffer_budget,
        profile,
    }) {
        Ok(runtime) => runtime,
        Err(error) => return fail("Runtime::new", error),
    };
    let (guest, app) = AotGuest::<Hosted<D>>::new();
    if let Err(error) = runtime.set_guest(Box::new(guest)) {
        return fail("set_guest", error);
    }
    for index in 0..config.input_count {
        let (table, context) =
            unsafe { (*config.inputs.add(index), *config.input_contexts.add(index)) };
        let source = match unsafe { CInputSource::new(table, context) } {
            Ok(source) => source,
            Err(error) => return fail("input source", error),
        };
        if let Err(error) = runtime.add_input_source(Box::new(source)) {
            return fail("add_input_source", error);
        }
    }
    let software = match RasterRenderer::new(1) {
        Ok(renderer) => renderer,
        Err(error) => return fail("RasterRenderer", error),
    };
    if let Err(error) = runtime.set_software_renderer(software, RasterRenderer::new) {
        return fail("set_software_renderer", error);
    }
    let mut renderer = RendererId::SOFTWARE;
    if !config.renderer.is_null() {
        let device = match unsafe { CRenderer::new(config.renderer, config.renderer_context) } {
            Ok(device) => device,
            Err(error) => return fail("renderer", error),
        };
        renderer = match runtime.add_renderer(device) {
            Ok(id) => id,
            Err(error) => return fail("add_renderer", error),
        };
    }
    let display = match unsafe { CDisplay::new(config.display, config.display_context) } {
        Ok(display) => display,
        Err(error) => return fail("display", error),
    };
    if let Err(error) = runtime.add_display(display, renderer) {
        return fail("add_display", error);
    }
    if let Err(error) =
        runtime.boot_aot_deferred(facts.profile_hash, viewport, false, facts.tick_hz)
    {
        return fail("boot_aot_deferred", error);
    }
    if let Err(error) = runtime.present() {
        return fail("present", error);
    }
    let cadence_hz = runtime.timing().map_or(0, |timing| timing.cadence_hz);
    Some(Instance {
        runtime,
        app,
        cadence_hz,
        failed: false,
    })
}

/// `m5_pocket_aot_create`.
///
/// # Safety
/// `config` is a valid `m5_pocket_aot_config` whose vtables and contexts stay
/// live until [`destroy`].
pub unsafe fn create<D: DeviceApp>(config: *const AotConfig, facts: AppFacts) -> *mut c_void {
    if config.is_null() {
        return core::ptr::null_mut();
    }
    let config = unsafe { &*config };
    if config.struct_size < core::mem::size_of::<AotConfig>()
        || config.display.is_null()
        || (config.input_count != 0 && (config.inputs.is_null() || config.input_contexts.is_null()))
    {
        return core::ptr::null_mut();
    }
    match unsafe { build::<D>(config, facts) } {
        Some(instance) => Box::into_raw(Box::new(instance)).cast(),
        None => core::ptr::null_mut(),
    }
}

/// `m5_pocket_aot_cadence_hz`.
///
/// # Safety
/// `app` came from [`create`] for the same `D`.
pub unsafe fn cadence_hz<D: DeviceApp>(app: *const c_void) -> u32 {
    if app.is_null() {
        return 0;
    }
    unsafe { &*app.cast::<Instance<D>>() }.cadence_hz
}

/// `m5_pocket_aot_tick`: one runtime tick. The first failure is logged.
///
/// # Safety
/// `app` came from [`create`] for the same `D`; calls are not concurrent.
pub unsafe fn tick<D: DeviceApp>(app: *mut c_void) -> i32 {
    if app.is_null() {
        return STATUS_INVALID;
    }
    let instance = unsafe { &mut *app.cast::<Instance<D>>() };
    match instance.runtime.tick() {
        Ok(()) => STATUS_OK,
        Err(error) => {
            if !instance.failed {
                instance.failed = true;
                log(&format!("m5_pocket_aot: tick: {error:?}"));
            }
            STATUS_FAILED
        }
    }
}

/// `m5_pocket_aot_destroy`: completes presentation, shuts the app down and
/// releases the runtime.
///
/// # Safety
/// `app` came from [`create`] for the same `D` and is not used afterwards.
pub unsafe fn destroy<D: DeviceApp>(app: *mut c_void) {
    if app.is_null() {
        return;
    }
    let mut instance = unsafe { Box::from_raw(app.cast::<Instance<D>>()) };
    let _ = instance.runtime.shutdown();
    drop(instance);
}

/// Run `f` on the mounted app's `Ui`, between frames.
///
/// # Safety
/// `app` came from [`create`] for the same `D`; no frame is running.
unsafe fn with_ui<D: DeviceApp, T>(app: *mut c_void, f: impl FnOnce(&mut Ui) -> T) -> Option<T> {
    if app.is_null() {
        return None;
    }
    let instance = unsafe { &mut *app.cast::<Instance<D>>() };
    let mut state = instance.app.try_borrow_mut().ok()?;
    let hosted = state.as_mut()?;
    Some(f(hosted.app.ui_mut()))
}

/// `m5_pocket_aot_upload_texture`: a handle, or -1.
///
/// # Safety
/// As [`with_ui`]; `pixels` holds `length` readable bytes.
pub unsafe fn upload_texture<D: DeviceApp>(
    app: *mut c_void,
    pixels: *const u8,
    length: usize,
    width: u32,
    height: u32,
    psm: u32,
) -> i32 {
    if pixels.is_null() {
        return -1;
    }
    let bytes = unsafe { core::slice::from_raw_parts(pixels, length) };
    unsafe {
        with_ui::<D, _>(app, |ui| {
            ui.core_mut().upload_texture(bytes, width, height, psm)
        })
    }
    .unwrap_or(-1)
}

/// `m5_pocket_aot_free_texture`.
///
/// # Safety
/// As [`with_ui`].
pub unsafe fn free_texture<D: DeviceApp>(app: *mut c_void, handle: i32) {
    unsafe { with_ui::<D, _>(app, |ui| ui.core_mut().free_texture(handle)) };
}

/// `m5_pocket_aot_register_image`: a POCKET_STATUS_* code.
///
/// # Safety
/// As [`with_ui`]; `name` holds `length` bytes of UTF-8.
pub unsafe fn register_image<D: DeviceApp>(
    app: *mut c_void,
    name: *const u8,
    length: usize,
    handle: i32,
) -> i32 {
    if name.is_null() {
        return STATUS_INVALID;
    }
    let Ok(name) = core::str::from_utf8(unsafe { core::slice::from_raw_parts(name, length) })
    else {
        return STATUS_INVALID;
    };
    match unsafe { with_ui::<D, _>(app, |ui| ui.register_image(name, handle)) } {
        Some(()) => STATUS_OK,
        None => STATUS_FAILED,
    }
}
