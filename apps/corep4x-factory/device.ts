// The device data bridge of the CoreP4X factory demo (docs/APP-HOST.md):
// every value the board provides and every request the application makes of
// the board. The board publishes values under these field names
// (m5_pocket_bridge_set_*); in JS mode main.tsx delivers them through the
// host* functions, in AOT mode the Rust host writes device_<name>_mut().
// Requests are strings "<name> <arg>..." drained after each frame.
import { clear, copy, i32, len, push, type f32, type f64 } from "@pocketjs/framework/solid/std";

// ---- board -> app -----------------------------------------------------------

/** True once the board published a value. */
export let hostAttached: boolean = false;

/** M5PM1 and M5IOE1 ("PYB") answered at boot. */
export let pm1Ready: boolean = true;
export let pybReady: boolean = true;
/** NVS corep4_prefs/boot_sound: the startup prompt sound at boot. */
export let bootSound: boolean = true;
/** "V<version>.<YYMMDD>-<HHMMSS>" of the running image. */
export let firmwareVersion: string = "V0.1.2.261124-000000";

/** Power rails as the board last applied them: L3B, LCD, AudioIC, AudioPA,
 *  USBA, P4USBC, Grove, M5BUS (published as rail<Name>). */
export let rails: boolean[] = [true, true, true, true, true, true, true, true];
/** LCD backlight 1..100 %. */
export let backlight: i32 = 100;
/** Charge current: 0 off, 1 180 mA, 2 650 mA. */
export let charge: i32 = 1;
/** PM1 VBAT filtered over 14 samples of 250 ms (mV), 0 until it exists. */
export let vbatFiltered: i32 = 0;
/** Input levels of GPIO 0..54 (0/1), sampled every 250 ms on the M5BUS tab. */
export let gpioLevels: i32[] = [];
/** The result of the last gpioConfig request ("" or an esp_err_t name) and
 *  the count of results. */
export let gpioConfigError: string = "";
export let gpioConfigs: i32 = 0;

/** RX8130CE date and time (month 1..12), published every second; the
 *  image's build time when the RTC does not answer (rtcReady false). */
export let rtcReady: boolean = true;
export let rtcYear: i32 = 2026;
export let rtcMonth: i32 = 11;
export let rtcDay: i32 = 24;
export let rtcHour: i32 = 9;
export let rtcMinute: i32 = 41;
export let rtcSecond: i32 = 0;
/** An SNTP sync over Ethernet succeeded during this TIMER visit. */
export let ntpReady: boolean = false;

/** Addresses that answered all three passes of the last I2C scan, and the
 *  scan results (0 done, -1 bus not ready) not yet taken. */
export let i2cAddresses: i32[] = [];
export let i2cResults: i32[] = [];

/** microSD: mounted and listable; the last listing ("<D|F><size>\t<name>"
 *  lines) and its count; an opendir failure text; capacity text; the sum
 *  of the listed file sizes. */
export let sdReady: boolean = false;
export let sdListing: string = "";
export let sdListed: i32 = 0;
export let sdPathError: string = "";
export let sdStatus: string = "SD: Not Ready";
export let sdFolderSize: string = "0 B";
/** SD speed test: progress text and the count of finished tests. */
export let sdTestText: string = "";
export let sdTests: i32 = 0;

/** TOUCH canvas: cells visited (0..25), whether the pill sits on dark
 *  pixels, the texture handle of the drawing; the controller
 *  status lines; the global touch indicator (NVS corep4_prefs/touch_ind). */
export let touchVisited: i32 = 0;
export let touchPillDark: boolean = false;
export let touchTexture: i32 = -1;
export let touchStatus: string = "Panel: CST3XX --  Handle:ready\nChip ID: 0xCACA220E\nFW Version: v1.1.1 (0x01010001)\nResolution: 480x480";
export let touchIndicator: boolean = false;

/** IMU window: 0 starting, 1 calibrating, 2 ready, 3 failed; the error
 *  name; the texture handle of the rendering; finger markers (count and
 *  top-left corners in canvas pixels). */
export let imuState: i32 = 0;
export let imuError: string = "";
export let imuTexture: i32 = -1;
export let imuMarkers: i32 = 0;
export let imuMarker0X: i32 = 0;
export let imuMarker0Y: i32 = 0;
export let imuMarker1X: i32 = 0;
export let imuMarker1Y: i32 = 0;

/** REC: 0 idle, 1 recording, 2 recorded, 3 playing; a recording or
 *  playback is being stopped; a worker task runs; the board's view mode
 *  (Spectrum) after it forced Waveform; dB labels; the texture handle. */
export let recState: i32 = 0;
export let recStopping: boolean = false;
export let recPlayStopping: boolean = false;
export let recBusy: boolean = false;
export let recPlayBusy: boolean = false;
export let recSpectrum: boolean = false;
export let recDb0: string = "-inf";
export let recDb1: string = "-inf";
export let recDb2: string = "-inf";
export let recTexture: i32 = -1;

/** MUSIC: 0 ready, 1 preparing, 2 playing, 3 finished, 4 paused, 5 other
 *  audio, 6 not ready; position and length (ms); the 24 bar targets. */
export let musicState: i32 = 0;
export let musicElapsedMs: i32 = 0;
export let musicDurationMs: i32 = 45000;
export let musicBars: i32[] = [];
/** ES8311 output volume 0..100 (80 at boot). */
export let volume: i32 = 80;

/** Ethernet: 0 disconnected, 1 link up, 2 got an IP; address and gateway.
 *  PING: a test runs; the four rows ("<dot tone><text tone><text>" lines,
 *  tones 0 muted, 1 success, 2 warning, 3 error); the summary. */
export let ethState: i32 = 0;
export let ethIp: string = "";
export let ethGateway: string = "";
export let pingRunning: boolean = false;
export let pingRows: string = "";
export let pingSummary: string = "";
export let pingSummaryTone: i32 = 0;

/** IR: the RMT channels are set up, set-up or a send runs; the last result
 *  and its tone (0 muted, 1 success, 2 warning, 3 error); received frames
 *  and the newest one; the command after an Auto+1 send. */
export let irReady: boolean = false;
export let irInitializing: boolean = false;
export let irSending: boolean = false;
export let irResult: string = "Auto initializing IR TX/RX...";
export let irResultTone: i32 = 2;
export let irFrames: i32 = 0;
export let irAddress: i32 = 0;
export let irCommand: i32 = 0;
export let irRepeat: boolean = false;
export let irNext: i32[] = [];

/** The touch contact in logical pixels, published by the app host. */
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;
export let pointerDown: boolean = false;

// ---- app -> board -----------------------------------------------------------

export let requests: string[] = [];

export function request(command: string): void {
  push(requests, command);
}

export function setRail(name: string, index: i32, on: boolean): void {
  request(`rail ${name} ${on ? 1 : 0}`);
  if (!hostAttached) {
    rails[index] = on;
  }
}

export function setBacklight(percent: i32): void {
  backlight = percent;
  request(`backlight ${percent}`);
}

export function setCharge(value: i32): void {
  request(`charge ${value}`);
  if (!hostAttached) {
    charge = value;
  }
}

export function setVolume(value: i32): void {
  volume = value;
  request(`volume ${value}`);
}

export function setTouchIndicator(on: boolean): void {
  touchIndicator = on;
  request(`touchIndicator ${on ? 1 : 0}`);
}

export function setBootSound(on: boolean): void {
  bootSound = on;
  request(`bootSound ${on ? 1 : 0}`);
}

// ---- board -> app events ----------------------------------------------------

/** PM1 shutdown result codes, queued for the OFF window. */
export let powerOffResults: i32[] = [];
/** Deep sleep failures (esp_err_t names). */
export let sleepErrors: string[] = [];

// ---- host entry points (JS mode) -------------------------------------------

export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "hostAttached":
      hostAttached = value !== 0;
      break;
    case "pointerX":
      pointerX = i32(value);
      break;
    case "pointerY":
      pointerY = i32(value);
      break;
    case "pointerDown":
      pointerDown = value !== 0;
      break;
    case "pm1Ready":
      pm1Ready = value !== 0;
      break;
    case "pybReady":
      pybReady = value !== 0;
      break;
    case "bootSound":
      bootSound = value !== 0;
      break;
    case "backlight":
      backlight = i32(value);
      break;
    case "charge":
      charge = i32(value);
      break;
    case "vbatFiltered":
      vbatFiltered = i32(value);
      break;
    case "rtcReady":
      rtcReady = value !== 0;
      break;
    case "rtcYear":
      rtcYear = i32(value);
      break;
    case "rtcMonth":
      rtcMonth = i32(value);
      break;
    case "rtcDay":
      rtcDay = i32(value);
      break;
    case "rtcHour":
      rtcHour = i32(value);
      break;
    case "rtcMinute":
      rtcMinute = i32(value);
      break;
    case "rtcSecond":
      rtcSecond = i32(value);
      break;
    case "ntpReady":
      ntpReady = value !== 0;
      break;
    case "touchVisited":
      touchVisited = i32(value);
      break;
    case "touchPillDark":
      touchPillDark = value !== 0;
      break;
    case "touchTexture":
      touchTexture = i32(value);
      break;
    case "touchIndicator":
      touchIndicator = value !== 0;
      break;
    case "imuState":
      imuState = i32(value);
      break;
    case "imuTexture":
      imuTexture = i32(value);
      break;
    case "imuMarkers":
      imuMarkers = i32(value);
      break;
    case "imuMarker0X":
      imuMarker0X = i32(value);
      break;
    case "imuMarker0Y":
      imuMarker0Y = i32(value);
      break;
    case "imuMarker1X":
      imuMarker1X = i32(value);
      break;
    case "imuMarker1Y":
      imuMarker1Y = i32(value);
      break;
    case "recState":
      recState = i32(value);
      break;
    case "recStopping":
      recStopping = value !== 0;
      break;
    case "recPlayStopping":
      recPlayStopping = value !== 0;
      break;
    case "recBusy":
      recBusy = value !== 0;
      break;
    case "recPlayBusy":
      recPlayBusy = value !== 0;
      break;
    case "recSpectrum":
      recSpectrum = value !== 0;
      break;
    case "recTexture":
      recTexture = i32(value);
      break;
    case "musicState":
      musicState = i32(value);
      break;
    case "musicElapsedMs":
      musicElapsedMs = i32(value);
      break;
    case "musicDurationMs":
      musicDurationMs = i32(value);
      break;
    case "volume":
      volume = i32(value);
      break;
    case "ethState":
      ethState = i32(value);
      break;
    case "pingRunning":
      pingRunning = value !== 0;
      break;
    case "pingSummaryTone":
      pingSummaryTone = i32(value);
      break;
    case "irReady":
      irReady = value !== 0;
      break;
    case "irInitializing":
      irInitializing = value !== 0;
      break;
    case "irSending":
      irSending = value !== 0;
      break;
    case "irResultTone":
      irResultTone = i32(value);
      break;
    case "irFrames":
      irFrames = i32(value);
      break;
    case "irAddress":
      irAddress = i32(value);
      break;
    case "irCommand":
      irCommand = i32(value);
      break;
    case "irRepeat":
      irRepeat = value !== 0;
      break;
    case "sdReady":
      sdReady = value !== 0;
      break;
    case "railL3B":
      rails[0] = value !== 0;
      break;
    case "railLCD":
      rails[1] = value !== 0;
      break;
    case "railAudioIC":
      rails[2] = value !== 0;
      break;
    case "railAudioPA":
      rails[3] = value !== 0;
      break;
    case "railUSBA":
      rails[4] = value !== 0;
      break;
    case "railP4USBC":
      rails[5] = value !== 0;
      break;
    case "railGrove":
      rails[6] = value !== 0;
      break;
    case "railM5BUS":
      rails[7] = value !== 0;
      break;
  }
}

export function hostText(name: string, value: string): void {
  switch (name) {
    case "firmwareVersion":
      firmwareVersion = value;
      break;
    case "sleepError":
      push(sleepErrors, value);
      break;
    case "gpioConfigError":
      gpioConfigError = value;
      break;
    case "recDb0":
      recDb0 = value;
      break;
    case "recDb1":
      recDb1 = value;
      break;
    case "recDb2":
      recDb2 = value;
      break;
    case "ethIp":
      ethIp = value;
      break;
    case "ethGateway":
      ethGateway = value;
      break;
    case "pingRows":
      pingRows = value;
      break;
    case "pingSummary":
      pingSummary = value;
      break;
    case "irResult":
      irResult = value;
      break;
    case "touchStatus":
      touchStatus = value;
      break;
    case "imuError":
      imuError = value;
      break;
    case "sdListing":
      sdListing = value;
      break;
    case "sdPathError":
      sdPathError = value;
      break;
    case "sdStatus":
      sdStatus = value;
      break;
    case "sdFolderSize":
      sdFolderSize = value;
      break;
    case "sdTestText":
      sdTestText = value;
      break;
  }
}

export function hostArray(name: string, values: f32[]): void {
  switch (name) {
    case "musicBars":
      clear(musicBars);
      for (let i: i32 = 0; i < len(values); i++) {
        push(musicBars, i32(values[i]));
      }
      break;
    case "i2cAddresses":
      clear(i2cAddresses);
      for (let i: i32 = 0; i < len(values); i++) {
        push(i2cAddresses, i32(values[i]));
      }
      break;
    case "gpioLevels":
      clear(gpioLevels);
      for (let i: i32 = 0; i < len(values); i++) {
        push(gpioLevels, i32(values[i]));
      }
      break;
  }
}

export function hostEvent(name: string, value: f64): void {
  switch (name) {
    case "powerOff":
      push(powerOffResults, i32(value));
      break;
    case "gpioConfig":
      gpioConfigs = gpioConfigs + 1;
      break;
    case "i2cScan":
      push(i2cResults, i32(value));
      break;
    case "sdListed":
      sdListed = sdListed + 1;
      break;
    case "irNext":
      push(irNext, i32(value));
      break;
    case "sdTest":
      sdTests = sdTests + 1;
      break;
  }
}

export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}
