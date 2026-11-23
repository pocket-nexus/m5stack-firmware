//! AOT glue of the AtomS3 factory app: mount, board values into device.ts,
//! the request queue out. The value and event lists mirror the hostNumber,
//! hostText, hostArray and hostEvent switches of apps/atoms3-factory/device.ts.
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

/// Decoded artwork kept past a frame: one 128x128 RGB565 image.
const TEXTURE_BUDGET: usize = 128 * 128 * 2;

type App = generated::AppApp<generated::AppModel, M5Host>;

pub struct Factory(Box<App>);

impl DeviceApp for Factory {
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
        load_images_in_place(&mut ui);
        ui.core_mut().set_texture_budget(TEXTURE_BUDGET);
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
        let ssid = match value.name {
            "wifi1" => Some(0),
            "wifi2" => Some(1),
            "wifi3" => Some(2),
            "wifi4" => Some(3),
            "wifi5" => Some(4),
            _ => None,
        };
        if let Some(index) = ssid {
            let ssids = model.device_wifiSsids_mut();
            if let Some(slot) = ssids.get_mut(index) {
                *slot = String::from(value.text());
            }
            return true;
        }
        match value.name {
            "imu" => *model.device_imuSamples_mut() += 1,
            "adc" => *model.device_adcSamples_mut() += 1,
            _ => {}
        }
        device_fields!(model, value, {
            "attached" => device_attached_mut,
            "irAddress" => device_irAddress_mut,
            "imu" => device_imu_mut,
            "wifiRssi" => device_wifiRssi_mut,
            "i2cAddresses" => device_i2cAddresses_mut,
            "adc" => device_adc_mut,
        })
    }

    fn event(&mut self, event: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        let byte = event.number() as i32;
        match event.name {
            "wifiDone" => {
                *model.device_wifiCount_mut() = byte;
                *model.device_wifiDone_mut() += 1;
            }
            "i2cDone" => *model.device_i2cDone_mut() += 1,
            "uartRx" => model.device_uartRx_mut().push(byte),
            "uartTx" => model.device_uartTx_mut().push(byte),
            "usbRx" => model.device_usbRx_mut().push(byte),
            _ => return false,
        }
        true
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(Factory, profile, TICK_HZ);
