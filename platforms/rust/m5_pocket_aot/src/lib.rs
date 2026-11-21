//! AOT host of `m5_pocket_app` (platforms/esp-idf/m5_pocket_app).
//!
//! A board's per-app crate compiles the MicroTS output of its application,
//! implements [`DeviceApp`] for the generated `AppApp` and calls
//! [`export_app!`]. This crate supplies everything else the static library
//! needs on ESP-IDF:
//!
//! - the `m5_pocket/aot.h` C ABI ([`create`], [`tick`], [`cadence_hz`],
//!   [`destroy`]), which boots a `pocket_runtime::Runtime` with
//!   `boot_aot_deferred` over the C display, renderer and input vtables that
//!   `m5_pocket_app` created (adapted by `pocket_drivers::abi`);
//! - the device bridge between `m5_pocket/bridge.h` and the generated
//!   `device_<name>_mut()` accessors ([`bridge`], [`Field`],
//!   [`device_fields!`]);
//! - the translation of the runtime's guest frame into `microts::Input`,
//!   including touch presses ([`input`]);
//! - on the device (`target_os = "none"`): the global allocator over
//!   `pocket_host_alloc`, the panic handler over `pocket_host_panic`.
#![no_std]

extern crate alloc;

// ESP32-P4: link the PPA renderer factory (pocket_render_esp_ppa_create, ...)
// that the C pocket_render_esp_ppa component calls, as the JS-mode runtime
// archive does. A dependency nothing names would not be linked.
#[cfg(target_arch = "riscv32")]
pub use pocket_render_esp_ppa::cabi as ppa;

pub mod bridge;
mod fields;
mod host;
pub mod input;
mod instance;
mod sys;

pub use fields::Field;
pub use host::M5Host;
pub use instance::{
    cadence_hz, create, destroy, free_texture, register_image, tick, upload_texture, AotConfig,
    AppFacts, Hosted,
};
pub use sys::log;

use alloc::{rc::Rc, string::String, vec::Vec};
use microts::{Input, Ui};
use pocket_runtime::{GuestEnvironment, RuntimeError};

/// The per-app glue around a generated `AppApp`.
pub trait DeviceApp: Sized + 'static {
    /// Load the style table, fonts and images into `ui` and construct the
    /// generated app with an [`M5Host`].
    fn mount(ui: Ui, environment: &GuestEnvironment<'_>) -> Result<Self, RuntimeError>;
    /// `AppApp::frame`.
    fn frame(&mut self, input: &Input);
    /// `AppApp::invalidate`: called after board values changed fields.
    fn invalidate(&mut self);
    /// `AppApp::ui_mut`.
    fn ui_mut(&mut self) -> &mut Ui;
    /// Write one board value into its `device.ts` field; false for a name
    /// the application does not declare. [`device_fields!`] writes the match.
    fn value(&mut self, value: &bridge::Value<'_>) -> bool;
    /// Handle one board event (a number); false when not handled.
    fn event(&mut self, event: &bridge::Value<'_>) -> bool {
        let _ = event;
        false
    }
    /// The `device.ts` request queue (`device_requests_mut()`), drained after
    /// every frame.
    fn requests(&mut self) -> Option<&mut Vec<String>>;
}

/// Load a baked font atlas in place: its coverage cells stay in flash
/// (`load_font_atlas_shared`) instead of the heap copy the generated
/// `load_fonts` makes.
pub fn load_font_in_place(ui: &mut Ui, bytes: &'static [u8]) -> bool {
    let storage: Rc<dyn AsRef<[u8]>> = Rc::new(bytes);
    ui.core_mut()
        .load_font_atlas_shared(storage, 0..bytes.len())
}

/// Where an image's pixels live after [`upload_image`].
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ImageStorage {
    /// Copied into the heap at mount (`upload_texture_flags`), as the
    /// generated `load_images` does: the pixel bytes are paid twice, in flash
    /// and in the heap.
    Heap,
    /// Read in place from flash (`upload_texture_from`): copied into the heap
    /// when a frame first draws the texture and released again under the
    /// core's texture budget (`ui.core_mut().set_texture_budget(bytes)`;
    /// zero, the default, keeps every drawn texture).
    Flash,
}

/// Upload one picture `m5_pocket_aot_build::emit_images` embedded and
/// register it under `name`, the name the generated view draws. `psm` and
/// `flags` follow the DrawList texture contract (`spec::psm`, `spec::img`).
#[allow(clippy::too_many_arguments)]
pub fn upload_image(
    ui: &mut Ui,
    name: &str,
    pixels: &'static [u8],
    width: u32,
    height: u32,
    psm: u32,
    flags: u8,
    storage: ImageStorage,
) {
    let id = match storage {
        ImageStorage::Heap => ui
            .core_mut()
            .upload_texture_flags(pixels, width, height, psm, flags),
        ImageStorage::Flash => {
            let bytes: Rc<dyn AsRef<[u8]>> = Rc::new(pixels);
            ui.core_mut()
                .upload_texture_from(bytes, 0..pixels.len(), width, height, psm, flags)
        }
    };
    assert!(id >= 0, "image upload failed");
    ui.register_image(name, id);
}

/// Define the `m5_pocket/aot.h` functions for one [`DeviceApp`].
///
/// `$profile` names the module (in scope at the call) that includes `OUT_DIR/pocket_profile.rs`;
/// `$tick_hz` is the generated `TICK_HZ`.
#[macro_export]
macro_rules! export_app {
    ($app:ty, $profile:ident, $tick_hz:expr) => {
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_create(
            config: *const $crate::AotConfig,
        ) -> *mut ::core::ffi::c_void {
            unsafe {
                $crate::create::<$app>(
                    config,
                    $crate::AppFacts {
                        profile: $profile::profile(),
                        profile_hash: $profile::PROFILE_HASH,
                        host_abi: $profile::HOST_ABI,
                        tick_hz: $tick_hz,
                    },
                )
            }
        }
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_cadence_hz(app: *const ::core::ffi::c_void) -> u32 {
            unsafe { $crate::cadence_hz::<$app>(app) }
        }
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_tick(app: *mut ::core::ffi::c_void) -> i32 {
            unsafe { $crate::tick::<$app>(app) }
        }
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_destroy(app: *mut ::core::ffi::c_void) {
            unsafe { $crate::destroy::<$app>(app) }
        }
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_upload_texture(
            app: *mut ::core::ffi::c_void,
            pixels: *const u8,
            length: usize,
            width: u32,
            height: u32,
            psm: u32,
        ) -> i32 {
            unsafe { $crate::upload_texture::<$app>(app, pixels, length, width, height, psm) }
        }
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_free_texture(
            app: *mut ::core::ffi::c_void,
            handle: i32,
        ) {
            unsafe { $crate::free_texture::<$app>(app, handle) }
        }
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn m5_pocket_aot_register_image(
            app: *mut ::core::ffi::c_void,
            name: *const u8,
            length: usize,
            handle: i32,
        ) -> i32 {
            unsafe { $crate::register_image::<$app>(app, name, length, handle) }
        }
    };
}

/// Match a board value's name against `device.ts` fields and assign it
/// through the generated accessors; evaluates to whether a field took it.
///
/// ```ignore
/// fn value(&mut self, value: &Value<'_>) -> bool {
///     let model = &mut self.0.model;
///     m5_pocket_aot::device_fields!(model, value, {
///         "battery" => device_battery_mut,
///         "clock" => device_clock_mut,
///     })
/// }
/// ```
#[macro_export]
macro_rules! device_fields {
    ($model:expr, $value:expr, { $($name:literal => $accessor:ident),* $(,)? }) => {
        match $value.name {
            $($name => {
                $crate::Field::assign($model.$accessor(), $value);
                true
            })*
            _ => false,
        }
    };
}
