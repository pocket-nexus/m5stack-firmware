//! AOT glue of the StopWatch app: mount, board values into device.ts, the
//! request queue out. The badge picture the board decodes arrives through
//! m5_pocket_app_set_texture (main/services.c). Everything else comes from
//! m5_pocket_aot. Derived from tests/aot-smoke/aot.
#![no_std]

extern crate alloc;

use alloc::{boxed::Box, string::String, vec::Vec};
use m5_pocket_aot::{bridge::Value, device_fields, DeviceApp, M5Host};
use microts::{Input, Ui};
use pocket_runtime::{GuestEnvironment, RuntimeError};

#[allow(dead_code)]
mod profile {
    include!(concat!(env!("OUT_DIR"), "/pocket_profile.rs"));
}
include!(concat!(env!("POCKETJS_AOT_DIR"), "/include.rs"));

type App = generated::AppApp<generated::AppModel, M5Host>;

pub struct Stopwatch {
    app: Box<App>,
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
        // Pictures stay in flash in their images.json formats (7.2 MB of
        // IMG entries with the big-number tiles and glyph images); a drawn
        // texture is decoded into the heap and released beyond a 2 MiB
        // budget when the previous frame did not draw it.
        load_images_in_place(&mut ui);
        ui.core_mut().set_texture_budget(2 * 1024 * 1024);
        let mut app = Box::new(App::new(
            M5Host(ui),
            generated::AppProps {},
            generated::AppModel::default(),
        ));
        let first = app.ui().initial_focus();
        app.ui_mut().set_focus(first);
        Ok(Self { app })
    }

    fn frame(&mut self, input: &Input) {
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
            "badgeTexture" => device_badgeTexture_mut,
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
