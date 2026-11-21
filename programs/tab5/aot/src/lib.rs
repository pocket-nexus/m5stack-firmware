//! AOT glue of the Tab5 factory demo: mount, board values into device.ts,
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

pub struct Tab5(Box<App>);

impl DeviceApp for Tab5 {
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
        // The @2x pictures in their images.json formats, not the generated
        // 1x RGBA8888 load_images.
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

    /// The hostNumber, hostText and hostArray cases of device.ts, in order.
    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        device_fields!(model, value, {
            "hostAttached" => device_hostAttached_mut,
            "rtcYear" => device_rtcYear_mut,
            "rtcMonth" => device_rtcMonth_mut,
            "rtcDay" => device_rtcDay_mut,
            "rtcHour" => device_rtcHour_mut,
            "rtcMinute" => device_rtcMinute_mut,
            "rtcSecond" => device_rtcSecond_mut,
            "busVoltage" => device_busVoltage_mut,
            "shuntCurrent" => device_shuntCurrent_mut,
            "cpuTemperature" => device_cpuTemperature_mut,
            "accelX" => device_accelX_mut,
            "accelY" => device_accelY_mut,
            "accelZ" => device_accelZ_mut,
            "gyroX" => device_gyroX_mut,
            "gyroY" => device_gyroY_mut,
            "gyroZ" => device_gyroZ_mut,
            "usbCConnected" => device_usbCConnected_mut,
            "usbAConnected" => device_usbAConnected_mut,
            "headphoneConnected" => device_headphoneConnected_mut,
            "recordTestState" => device_recordTestState_mut,
            "musicPlaying" => device_musicPlaying_mut,
            "cameraCapturing" => device_cameraCapturing_mut,
            "pointerX" => device_pointerX_mut,
            "pointerY" => device_pointerY_mut,
            "pointerDown" => device_pointerDown_mut,
            "panelIc" => device_panelIc_mut,
            "sdList" => device_sdList_mut,
            "rs485Text" => device_rs485Text_mut,
            "sdDirectories" => device_sdDirectories_mut,
            "i2cAddresses" => device_i2cAddresses_mut,
            "captureLeft" => device_captureLeft_mut,
            "captureRight" => device_captureRight_mut,
        })
    }

    /// The hostEvent cases of device.ts.
    fn event(&mut self, event: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        match event.name {
            "sdScan" => {
                *model.device_sdScans_mut() += 1;
                true
            }
            "i2cScan" => {
                *model.device_i2cScans_mut() += 1;
                true
            }
            "rs485" => {
                *model.device_rs485Chunks_mut() += event.number() as i32;
                true
            }
            _ => false,
        }
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(Tab5, profile, TICK_HZ);
