//! AOT glue of the CoreP4X factory demo: mount, board values into device.ts,
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

type App = generated::AppApp<generated::AppModel, M5Host>;

pub struct CoreP4X(Box<App>);

const RAILS: [&str; 8] = [
    "railL3B",
    "railLCD",
    "railAudioIC",
    "railAudioPA",
    "railUSBA",
    "railP4USBC",
    "railGrove",
    "railM5BUS",
];

impl DeviceApp for CoreP4X {
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
        // The pictures in their images.json formats, copied into the heap
        // (PSRAM) at mount.
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

    /// The hostNumber, hostText and hostArray cases of device.ts, in order.
    fn value(&mut self, value: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        // rail<Name> fill device.rails in the order L3B, LCD, AudioIC,
        // AudioPA, USBA, P4USBC, Grove, M5BUS.
        if let Some(index) = RAILS.iter().position(|name| *name == value.name) {
            let rails = model.device_rails_mut();
            if let Some(rail) = rails.get_mut(index) {
                *rail = value.boolean();
            }
            return true;
        }
        if value.name == "sleepError" {
            model.device_sleepErrors_mut().push(String::from(value.text()));
            return true;
        }
        device_fields!(model, value, {
            "hostAttached" => device_hostAttached_mut,
            "pointerX" => device_pointerX_mut,
            "pointerY" => device_pointerY_mut,
            "pointerDown" => device_pointerDown_mut,
            "pm1Ready" => device_pm1Ready_mut,
            "pybReady" => device_pybReady_mut,
            "bootSound" => device_bootSound_mut,
            "backlight" => device_backlight_mut,
            "charge" => device_charge_mut,
            "vbatFiltered" => device_vbatFiltered_mut,
            "rtcReady" => device_rtcReady_mut,
            "rtcYear" => device_rtcYear_mut,
            "rtcMonth" => device_rtcMonth_mut,
            "rtcDay" => device_rtcDay_mut,
            "rtcHour" => device_rtcHour_mut,
            "rtcMinute" => device_rtcMinute_mut,
            "rtcSecond" => device_rtcSecond_mut,
            "ntpReady" => device_ntpReady_mut,
            "touchVisited" => device_touchVisited_mut,
            "touchPillDark" => device_touchPillDark_mut,
            "touchTexture" => device_touchTexture_mut,
            "touchIndicator" => device_touchIndicator_mut,
            "imuState" => device_imuState_mut,
            "imuTexture" => device_imuTexture_mut,
            "imuMarkers" => device_imuMarkers_mut,
            "imuMarker0X" => device_imuMarker0X_mut,
            "imuMarker0Y" => device_imuMarker0Y_mut,
            "imuMarker1X" => device_imuMarker1X_mut,
            "imuMarker1Y" => device_imuMarker1Y_mut,
            "recState" => device_recState_mut,
            "recStopping" => device_recStopping_mut,
            "recPlayStopping" => device_recPlayStopping_mut,
            "recBusy" => device_recBusy_mut,
            "recPlayBusy" => device_recPlayBusy_mut,
            "recSpectrum" => device_recSpectrum_mut,
            "recTexture" => device_recTexture_mut,
            "musicState" => device_musicState_mut,
            "musicElapsedMs" => device_musicElapsedMs_mut,
            "musicDurationMs" => device_musicDurationMs_mut,
            "volume" => device_volume_mut,
            "ethState" => device_ethState_mut,
            "pingRunning" => device_pingRunning_mut,
            "pingSummaryTone" => device_pingSummaryTone_mut,
            "irReady" => device_irReady_mut,
            "irInitializing" => device_irInitializing_mut,
            "irSending" => device_irSending_mut,
            "irResultTone" => device_irResultTone_mut,
            "irFrames" => device_irFrames_mut,
            "irAddress" => device_irAddress_mut,
            "irCommand" => device_irCommand_mut,
            "irRepeat" => device_irRepeat_mut,
            "sdReady" => device_sdReady_mut,
            "firmwareVersion" => device_firmwareVersion_mut,
            "gpioConfigError" => device_gpioConfigError_mut,
            "recDb0" => device_recDb0_mut,
            "recDb1" => device_recDb1_mut,
            "recDb2" => device_recDb2_mut,
            "ethIp" => device_ethIp_mut,
            "ethGateway" => device_ethGateway_mut,
            "pingRows" => device_pingRows_mut,
            "pingSummary" => device_pingSummary_mut,
            "irResult" => device_irResult_mut,
            "touchStatus" => device_touchStatus_mut,
            "imuError" => device_imuError_mut,
            "sdListing" => device_sdListing_mut,
            "sdPathError" => device_sdPathError_mut,
            "sdStatus" => device_sdStatus_mut,
            "sdFolderSize" => device_sdFolderSize_mut,
            "sdTestText" => device_sdTestText_mut,
            "musicBars" => device_musicBars_mut,
            "i2cAddresses" => device_i2cAddresses_mut,
            "gpioLevels" => device_gpioLevels_mut,
        })
    }

    /// The hostEvent cases of device.ts.
    fn event(&mut self, event: &Value<'_>) -> bool {
        let model = &mut self.0.model;
        match event.name {
            "powerOff" => model.device_powerOffResults_mut().push(event.number() as i32),
            "gpioConfig" => *model.device_gpioConfigs_mut() += 1,
            "i2cScan" => model.device_i2cResults_mut().push(event.number() as i32),
            "sdListed" => *model.device_sdListed_mut() += 1,
            "irNext" => model.device_irNext_mut().push(event.number() as i32),
            "sdTest" => *model.device_sdTests_mut() += 1,
            _ => return false,
        }
        true
    }

    fn requests(&mut self) -> Option<&mut Vec<String>> {
        Some(self.0.model.device_requests_mut())
    }
}

m5_pocket_aot::export_app!(CoreP4X, profile, TICK_HZ);
