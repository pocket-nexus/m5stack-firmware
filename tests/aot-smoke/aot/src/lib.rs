//! AOT glue of the aot-smoke app: mount, board values into device.ts, the
//! request queue out. Everything else comes from m5_pocket_aot.
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

pub struct Smoke(Box<App>);

impl DeviceApp for Smoke {
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
        load_images(&mut ui);
        let mut app = Box::new(App::new(
            M5Host(ui),
            generated::AppProps {},
            generated::AppModel::default(),
        ));
        let first = app.ui().initial_focus();
        app.ui_mut().set_focus(first);
        Ok(Self(app))
    }

    fn frame(&mut self, input: &Input) {
        self.0.frame(input);
    }

    fn invalidate(&mut self) {
        self.0.invalidate();
    }

    fn ui_mut(&mut self) -> &mut Ui {
        self.0.ui_mut()
    }

    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        device_fields!(model, value, {
            "battery" => device_battery_mut,
            "charging" => device_charging_mut,
            "clock" => device_clock_mut,
            "spectrum" => device_spectrum_mut,
            "pointerX" => device_pointerX_mut,
            "pointerY" => device_pointerY_mut,
            "pointerDown" => device_pointerDown_mut,
        })
    }

    fn event(&mut self, event: &Value<'_>) -> bool {
        match event.name {
            "alarm" => {
                *self.0.model.device_alarms_mut() += event.number() as i32;
                true
            }
            _ => false,
        }
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(Smoke, profile, TICK_HZ);
