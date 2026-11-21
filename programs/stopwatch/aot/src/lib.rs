//! AOT glue of the StopWatch app: mount, board values into device.ts, the
//! request queue out, and the badge picture the board decodes. Everything
//! else comes from m5_pocket_aot. Derived from tests/aot-smoke/aot.
#![no_std]

extern crate alloc;

use alloc::{boxed::Box, string::String, vec::Vec};
use core::ffi::c_void;
use m5_pocket_aot::{bridge::Value, device_fields, DeviceApp, M5Host};
use microts::{Input, Ui};
use pocket_runtime::{GuestEnvironment, RuntimeError};

#[allow(dead_code)]
mod profile {
    include!(concat!(env!("OUT_DIR"), "/pocket_profile.rs"));
}
include!(concat!(env!("POCKETJS_AOT_DIR"), "/include.rs"));
include!(concat!(env!("OUT_DIR"), "/m5_fonts.rs"));
include!(concat!(env!("OUT_DIR"), "/stopwatch_images.rs"));

type App = generated::AppApp<generated::AppModel, M5Host>;

/// `stopwatch_badge_image` in programs/stopwatch/main/badge.h.
#[repr(C)]
struct BadgeImage {
    pixels: *mut u16,
    canvas_w: u32,
    canvas_h: u32,
    image_w: u32,
    image_h: u32,
}

extern "C" {
    fn stopwatch_badge_take_image(out: *mut BadgeImage) -> bool;
    fn heap_caps_free(ptr: *mut c_void);
}

/// The badge view binds this image name; the bundle holds a 1 x 1 stand-in.
const BADGE_IMAGE: &str = "images/badge_slot.png";
/// PocketJS PSM_5650: 16-bit pixels with red in the low bits.
const PSM_5650: u32 = 0;

pub struct Stopwatch {
    app: Box<App>,
    badge_texture: i32,
}

impl Stopwatch {
    /// Uploads a newly decoded badge picture (the JS-mode program does the
    /// same through pocket/textures.h) and tells the model its handle.
    fn take_badge(&mut self) {
        let mut image = BadgeImage {
            pixels: core::ptr::null_mut(),
            canvas_w: 0,
            canvas_h: 0,
            image_w: 0,
            image_h: 0,
        };
        // SAFETY: the C function fills `image`; a non-null buffer of
        // canvas_w * canvas_h u16 pixels is ours to free with heap_caps_free.
        if !unsafe { stopwatch_badge_take_image(&mut image) } {
            return;
        }
        let ui = self.app.ui_mut();
        if self.badge_texture >= 0 {
            ui.core_mut().free_texture(self.badge_texture);
            self.badge_texture = -1;
        }
        if !image.pixels.is_null() {
            let length = (image.canvas_w * image.canvas_h * 2) as usize;
            // SAFETY: see above; the bytes stay valid until heap_caps_free.
            let bytes = unsafe { core::slice::from_raw_parts(image.pixels as *const u8, length) };
            let handle = ui
                .core_mut()
                .upload_texture(bytes, image.canvas_w, image.canvas_h, PSM_5650);
            // SAFETY: the buffer came from heap_caps_malloc and is not used again.
            unsafe { heap_caps_free(image.pixels as *mut c_void) };
            if handle >= 0 {
                ui.register_image(BADGE_IMAGE, handle);
                self.badge_texture = handle;
                *self.app.model.device_badgeCanvas_mut() = image.canvas_w as i32;
            }
        }
        *self.app.model.device_badgeTexture_mut() = self.badge_texture;
        self.app.invalidate();
    }
}

impl DeviceApp for Stopwatch {
    fn mount(mut ui: Ui, environment: &GuestEnvironment<'_>) -> Result<Self, RuntimeError> {
        if environment.tick_hz != TICK_HZ
            || !ui.load_styles(include_bytes!(concat!(
                env!("POCKETJS_AOT_DIR"),
                "/styles.bin"
            )))
        {
            return Err(RuntimeError::InvalidConfiguration);
        }
        load_fonts_in_place(&mut ui);
        // Pictures stay in flash in their images.json formats (build.rs).
        load_images_in_place(&mut ui);
        let mut app = Box::new(App::new(
            M5Host(ui),
            generated::AppProps {},
            generated::AppModel::default(),
        ));
        let first = app.ui().initial_focus();
        app.ui_mut().set_focus(first);
        Ok(Self {
            app,
            badge_texture: -1,
        })
    }

    fn frame(&mut self, input: &Input) {
        self.take_badge();
        self.app.frame(input);
    }

    fn invalidate(&mut self) {
        self.app.invalidate();
    }

    fn ui_mut(&mut self) -> &mut Ui {
        self.app.ui_mut()
    }

    /// The AOT counterpart of hostNumber/hostText/hostArray in device.ts.
    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.app.model;
        if value.name == "uptimeMs" {
            *model.device_uptimeMs_mut() = value.number();
            *model.device_bridged_mut() = true;
            return true;
        }
        device_fields!(model, value, {
            "year" => device_year_mut,
            "month" => device_month_mut,
            "day" => device_day_mut,
            "weekday" => device_weekday_mut,
            "hour" => device_hour_mut,
            "minute" => device_minute_mut,
            "second" => device_second_mut,
            "batteryLevel" => device_batteryLevel_mut,
            "charging" => device_charging_mut,
            "pointerDown" => device_pointerDown_mut,
            "pointerX" => device_pointerX_mut,
            "pointerY" => device_pointerY_mut,
            "keyA" => device_keyA_mut,
            "keyB" => device_keyB_mut,
            "accelX" => device_accelX_mut,
            "accelY" => device_accelY_mut,
            "accelZ" => device_accelZ_mut,
            "gyroX" => device_gyroX_mut,
            "gyroY" => device_gyroY_mut,
            "gyroZ" => device_gyroZ_mut,
            "spectrum" => device_spectrum_mut,
            "peakHz" => device_peakHz_mut,
            "brightness" => device_brightness_mut,
            "volume" => device_volume_mut,
            "buttonSfx" => device_buttonSfx_mut,
            "buttonVibrate" => device_buttonVibrate_mut,
            "wifiEnabled" => device_wifiEnabled_mut,
            "guideLaunchCount" => device_guideLaunchCount_mut,
            "alarmHours" => device_alarmHours_mut,
            "alarmMinutes" => device_alarmMinutes_mut,
            "alarmEnabled" => device_alarmEnabled_mut,
            "badgeSlots" => device_badgeSlots_mut,
            "badgeActive" => device_badgeActive_mut,
            "apSsid" => device_apSsid_mut,
            "apMessage" => device_apMessage_mut,
            "badgeCanvas" => device_badgeCanvas_mut,
        })
    }

    fn event(&mut self, event: &Value<'_>) -> bool {
        match event.name {
            "badgeClosed" => {
                *self.app.model.device_badgeClosed_mut() += 1;
                true
            }
            _ => false,
        }
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.app.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(Stopwatch, profile, TICK_HZ);
