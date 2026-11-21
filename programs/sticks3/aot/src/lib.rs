//! AOT glue of the StickS3 factory app: mount, board values into device.ts,
//! the request queue out. Everything else comes from m5_pocket_aot.
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
include!(concat!(env!("OUT_DIR"), "/m5_fonts.rs"));
include!(concat!(env!("OUT_DIR"), "/m5_images.rs"));

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
        // RGB565 as images.json declares, not the generated RGBA8888
        // load_images: half the heap.
        load_images_at_density(&mut ui, m5_pocket_aot::ImageStorage::Heap);
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

    // The same names as the hostNumber/hostText switches in device.ts.
    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        device_fields!(model, value, {
            "batteryLevel" => device_batteryLevel_mut,
            "charging" => device_charging_mut,
            "wifiConfigured" => device_wifiConfigured_mut,
            "wifiStatus" => device_wifiStatus_mut,
            "cloudStatus" => device_cloudStatus_mut,
            "apStations" => device_apStations_mut,
            "portalClients" => device_portalClients_mut,
            "accessCode" => device_accessCode_mut,
            "nickName" => device_nickName_mut,
            "apSsid" => device_apSsid_mut,
            "firmwareVersion" => device_firmwareVersion_mut,
        })
    }

    // The StickS3 startup posts no board events (device.ts hostEvent).
    fn event(&mut self, _event: &Value<'_>) -> bool {
        false
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(StickS3, profile, TICK_HZ);
