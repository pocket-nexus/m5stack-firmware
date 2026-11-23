//! AOT glue of the StickS3 demos: mount, board values into device.ts, the
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

pub struct StickS3(Box<App>);

impl DeviceApp for StickS3 {
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
        // The demos draw only Views and text: no images to load.
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

    // The same names as the hostNumber/hostArray switches in device.ts.
    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        device_fields!(model, value, {
            "bridged" => device_bridged_mut,
            "uptimeMs" => device_uptimeMs_mut,
            "presentedMs" => device_presentedMs_mut,
            "keyFront" => device_keyFront_mut,
            "keySide" => device_keySide_mut,
            "frontPressMs" => device_frontPressMs_mut,
            "sidePressMs" => device_sidePressMs_mut,
            "batteryLevel" => device_batteryLevel_mut,
            "charging" => device_charging_mut,
            "imuOk" => device_imuOk_mut,
            "accelX" => device_accelX_mut,
            "accelY" => device_accelY_mut,
            "accelZ" => device_accelZ_mut,
            "gyroX" => device_gyroX_mut,
            "gyroY" => device_gyroY_mut,
            "gyroZ" => device_gyroZ_mut,
            "micOn" => device_micOn_mut,
            "micDb" => device_micDb_mut,
            "micPeakDb" => device_micPeakDb_mut,
            "seed" => device_seed_mut,
            "best" => device_best_mut,
        })
    }

    // device.ts hostEvent: a PMIC power-key click counts in powerClicks.
    fn event(&mut self, event: &Value<'_>) -> bool {
        match event.name {
            "powerKey" => {
                *self.0.model.device_powerClicks_mut() += 1;
                true
            }
            _ => false,
        }
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(StickS3, profile, TICK_HZ);
