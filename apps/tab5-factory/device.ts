// The device data bridge of the Tab5 factory demo (docs/APP-HOST.md): every
// value the board provides and every request the application makes of the
// board. The board publishes values under these field names
// (m5_pocket_bridge_set_*); in JS mode main.tsx delivers them through the
// host* functions, in AOT mode the Rust host writes device_<name>_mut().
// Requests are strings "<name> <arg>..." drained after each frame.
//
// Until a board publishes (`hostAttached` false) the application model calls
// the simulate* functions below, which stand in for the board.
import {
  clear,
  copy,
  f32,
  i32,
  idiv,
  imod,
  len,
  push,
  removeAt,
  type f64,
} from "@pocketjs/framework/solid/std";

// ---- board -> app -----------------------------------------------------------

/** True once the board published a value. */
export let hostAttached: boolean = false;

/** RX8130CE wall-clock time; month 1..12. */
export let rtcYear: i32 = 2025;
export let rtcMonth: i32 = 6;
export let rtcDay: i32 = 18;
export let rtcHour: i32 = 9;
export let rtcMinute: i32 = 41;
export let rtcSecond: i32 = 0;

/** INA226 on the battery path: volts and amperes, positive while charging. */
export let busVoltage: f32 = 7.72;
export let shuntCurrent: f32 = 0.24;
/** On-chip temperature sensor, degrees Celsius. */
export let cpuTemperature: i32 = 41;

/** BMI270 in the factory HAL's axes and units (g, tens of dps). */
export let accelX: f32 = 0;
export let accelY: f32 = 0;
export let accelZ: f32 = -1;
export let gyroX: f32 = 0;
export let gyroY: f32 = 0;
export let gyroZ: f32 = 0;

/** IO expander and USB host inputs. */
export let usbCConnected: boolean = true;
export let usbAConnected: boolean = false;
export let headphoneConnected: boolean = false;

/** The LCD controller: ILI9881C, ST7123 or ST7121. */
export let panelIc: string = "ST7123";

/** The last scan of the microSD root: names separated by "/", a 0/1 per
 *  name for directories, and the count of finished scans. */
export let sdList: string = "";
export let sdDirectories: i32[] = [];
export let sdScans: i32 = 0;

/** Addresses that answered the last I2C scan, and the count of scans. */
export let i2cAddresses: i32[] = [];
export let i2cScans: i32 = 0;

/** Waveform columns of the selected capture, oldest first: the peak
 *  magnitude of each column, 0..100. */
export let captureLeft: i32[] = [];
export let captureRight: i32[] = [];

/** Record test: 0 idle, 1 recording, 2 playing back. */
export let recordTestState: i32 = 0;
export let musicPlaying: boolean = false;
export let cameraCapturing: boolean = false;

/** The newest RS485 text chunk and the count of chunks received. */
export let rs485Text: string = "";
export let rs485Chunks: i32 = 0;

/** The touch contact in logical pixels (published by m5_pocket_app). */
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;
export let pointerDown: boolean = false;

// ---- app -> board -------------------------------------------------------------

export let requests: string[] = [];

/** Levels the application last requested, read back like the factory HAL's
 *  getters. */
export let backlight: i32 = 100;
export let speakerVolume: i32 = 60;
export let chargeEnabled: boolean = true;
export let quickChargeEnabled: boolean = true;
export let ext5vEnabled: boolean = true;
export let usbA5vEnabled: boolean = true;
export let extAntennaEnabled: boolean = false;
/** Requested board activity, read by the simulation. */
export let i2cScanBus: i32 = 0;
export let audioCapture: i32 = 0;
export let musicWanted: boolean = false;
export let cameraWanted: boolean = false;
let rtcSetYear: i32 = 0;
let rtcSetMonth: i32 = 0;
let rtcSetDay: i32 = 0;
let rtcSetHour: i32 = 0;
let rtcSetMinute: i32 = 0;
let rtcSetSecond: i32 = 0;
let rtcSetPending: boolean = false;
let recordTestPending: boolean = false;
let sdScanPending: boolean = false;

export function request(command: string): void {
  push(requests, command);
}

function flag(on: boolean): string {
  return on ? "1" : "0";
}

/** "backlight <0..100>" */
export function setBacklight(percent: i32): void {
  backlight = percent;
  request(`backlight ${percent}`);
}

/** "volume <0..100>" */
export function setSpeakerVolume(percent: i32): void {
  speakerVolume = percent;
  request(`volume ${percent}`);
}

/** "switch <name> <0|1>": charge, quickCharge, ext5v, usbA5v, extAntenna. */
export function setChargeEnabled(on: boolean): void {
  chargeEnabled = on;
  request(`switch charge ${flag(on)}`);
}

export function setQuickChargeEnabled(on: boolean): void {
  quickChargeEnabled = on;
  request(`switch quickCharge ${flag(on)}`);
}

export function setExt5vEnabled(on: boolean): void {
  ext5vEnabled = on;
  request(`switch ext5v ${flag(on)}`);
}

export function setUsbA5vEnabled(on: boolean): void {
  usbA5vEnabled = on;
  request(`switch usbA5v ${flag(on)}`);
}

export function setExtAntennaEnabled(on: boolean): void {
  extAntennaEnabled = on;
  request(`switch extAntenna ${flag(on)}`);
}

/** "tone <ms> <midi>...": notes in order. Nothing plays at volume 0, as in
 *  the factory audio helpers. */
export function playNotes(notes: i32[], length: i32): void {
  if (speakerVolume <= 0) {
    return;
  }
  let command: string = `tone ${length}`;
  for (let i: i32 = 0; i < len(notes); i++) {
    command = `${command} ${notes[i]}`;
  }
  request(command);
}

export function playNote(note: i32): void {
  if (speakerVolume > 0) {
    request(`tone 100 ${note}`);
  }
}

/** "sound startup": the startup sound effect. */
export function playStartupSound(): void {
  request("sound startup");
}

/** "rtc <year> <month 1..12> <day> <hour> <minute> <second>" */
export function setRtc(year: i32, month: i32, day: i32, hour: i32, minute: i32, second: i32): void {
  rtcSetYear = year;
  rtcSetMonth = month;
  rtcSetDay = day;
  rtcSetHour = hour;
  rtcSetMinute = minute;
  rtcSetSecond = second;
  rtcSetPending = true;
  request(`rtc ${year} ${month} ${day} ${hour} ${minute} ${second}`);
}

/** "power <action>": 1 power off, 2 sleep until touched, 3 power off until
 *  shaken, 4 power off for 10 seconds. */
export function requestPower(action: i32): void {
  request(`power ${action}`);
}

/** "i2cScan <bus>": scan every 300 ms, 1 internal, 2 Port A, 0 stop. */
export function setI2cScanBus(bus: i32): void {
  i2cScanBus = bus;
  request(`i2cScan ${bus}`);
}

/** "gpioTest <0|1> <pin>...": the pins become outputs driven low (1), or
 *  are driven low and released (0). */
export function setGpioTest(active: boolean, pins: i32[]): void {
  let command: string = `gpioTest ${flag(active)}`;
  for (let i: i32 = 0; i < len(pins); i++) {
    command = `${command} ${pins[i]}`;
  }
  request(command);
}

/** "gpio <pin> <0|1>" */
export function setGpioLevel(pin: i32, high: boolean): void {
  request(`gpio ${pin} ${flag(high)}`);
}

/** "capture <source>": waveform columns of 0 nothing, 1 both microphones,
 *  2 the headphone microphone, 3 the speaker loopback (AEC channel). */
export function setAudioCapture(source: i32): void {
  audioCapture = source;
  request(`capture ${source}`);
}

/** "recordTest <source>": 3 s recording played back; 1 dual microphone,
 *  2 headphone microphone. */
export function startRecordTest(source: i32): void {
  recordTestPending = true;
  request(`recordTest ${source}`);
}

/** "music <0|1>" */
export function setMusicWanted(on: boolean): void {
  musicWanted = on;
  request(`music ${flag(on)}`);
}

/** "rs485 <0|1>" opens the monitor; "rs485Send <text>" sends a line. */
export function setRs485Open(open: boolean): void {
  request(`rs485 ${flag(open)}`);
}

export function sendRs485(text: string): void {
  request(`rs485Send ${text}`);
}

/** "camera <0|1>" */
export function setCameraWanted(on: boolean): void {
  cameraWanted = on;
  request(`camera ${flag(on)}`);
}

/** "sdScan": mount the card, list its root, publish sdList and sdScan. */
export function requestSdScan(): void {
  sdScanPending = true;
  request("sdScan");
}

// ---- host entry points ------------------------------------------------------------

export function hostNumber(name: string, value: f64): void {
  hostAttached = true;
  switch (name) {
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
    case "busVoltage":
      busVoltage = f32(value);
      break;
    case "shuntCurrent":
      shuntCurrent = f32(value);
      break;
    case "cpuTemperature":
      cpuTemperature = i32(value);
      break;
    case "accelX":
      accelX = f32(value);
      break;
    case "accelY":
      accelY = f32(value);
      break;
    case "accelZ":
      accelZ = f32(value);
      break;
    case "gyroX":
      gyroX = f32(value);
      break;
    case "gyroY":
      gyroY = f32(value);
      break;
    case "gyroZ":
      gyroZ = f32(value);
      break;
    case "usbCConnected":
      usbCConnected = value !== 0;
      break;
    case "usbAConnected":
      usbAConnected = value !== 0;
      break;
    case "headphoneConnected":
      headphoneConnected = value !== 0;
      break;
    case "recordTestState":
      recordTestState = i32(value);
      break;
    case "musicPlaying":
      musicPlaying = value !== 0;
      break;
    case "cameraCapturing":
      cameraCapturing = value !== 0;
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
  }
}

export function hostText(name: string, value: string): void {
  hostAttached = true;
  switch (name) {
    case "panelIc":
      panelIc = value;
      break;
    case "sdList":
      sdList = value;
      break;
    case "rs485Text":
      rs485Text = value;
      break;
  }
}

function integers(values: f32[]): i32[] {
  const out: i32[] = [];
  for (let i: i32 = 0; i < len(values); i++) {
    push(out, i32(values[i]));
  }
  return out;
}

export function hostArray(name: string, values: f32[]): void {
  hostAttached = true;
  switch (name) {
    case "sdDirectories":
      sdDirectories = integers(values);
      break;
    case "i2cAddresses":
      i2cAddresses = integers(values);
      break;
    case "captureLeft":
      captureLeft = integers(values);
      break;
    case "captureRight":
      captureRight = integers(values);
      break;
  }
}

export function hostEvent(name: string, value: f64): void {
  hostAttached = true;
  switch (name) {
    case "sdScan":
      sdScans = sdScans + 1;
      break;
    case "i2cScan":
      i2cScans = i2cScans + 1;
      break;
    case "rs485":
      rs485Chunks = rs485Chunks + i32(value);
      break;
  }
}

export function takeRequests(): string[] {
  const taken: string[] = copy(requests);
  clear(requests);
  return taken;
}

// ---- simulation ---------------------------------------------------------------
// Without a board the application calls these to stand in for it: a ticking
// clock, a quiet microphone and the Tab5's internal I2C devices.

let simulationSeed: i32 = 12345;
let recordTestFrames: i32 = 0;
let i2cScanFrames: i32 = 0;

function simulatedNoise(range: i32): i32 {
  simulationSeed = imod(simulationSeed * 1103 + 12345, 65536);
  return imod(idiv(simulationSeed, 7), range);
}

/** Advances the wall clock by one second. */
export function simulateSecond(): void {
  rtcSecond = rtcSecond + 1;
  if (rtcSecond < 60) {
    return;
  }
  rtcSecond = 0;
  rtcMinute = rtcMinute + 1;
  if (rtcMinute < 60) {
    return;
  }
  rtcMinute = 0;
  rtcHour = imod(rtcHour + 1, 24);
}

/** One frame of the simulated board at `hz` frames per second. */
export function simulateFrame(hz: i32): void {
  clear(requests);
  if (rtcSetPending) {
    rtcSetPending = false;
    rtcYear = rtcSetYear;
    rtcMonth = rtcSetMonth;
    rtcDay = rtcSetDay;
    rtcHour = rtcSetHour;
    rtcMinute = rtcSetMinute;
    rtcSecond = rtcSetSecond;
  }
  if (recordTestPending) {
    recordTestPending = false;
    recordTestFrames = 6 * hz;
  }
  if (recordTestFrames > 0) {
    recordTestFrames = recordTestFrames - 1;
    recordTestState = recordTestFrames > 3 * hz ? 1 : recordTestFrames > 0 ? 2 : 0;
  }
  musicPlaying = musicWanted;
  cameraCapturing = cameraWanted;
  if (sdScanPending) {
    sdScanPending = false;
    sdList = "";
    sdScans = sdScans + 1;
  }
  if (i2cScanBus !== 0) {
    i2cScanFrames = i2cScanFrames + 1;
    if (i2cScanFrames >= idiv(hz * 3, 10)) {
      i2cScanFrames = 0;
      i2cAddresses = i2cScanBus === 1 ? [0x10, 0x32, 0x40, 0x41, 0x43, 0x44, 0x55, 0x68] : [];
      i2cScans = i2cScans + 1;
    }
  }
  if (audioCapture !== 0) {
    const level: i32 = musicPlaying || recordTestState === 2 ? 45 : 6;
    push(captureLeft, simulatedNoise(level) + 1);
    push(captureRight, simulatedNoise(level) + 1);
    while (len(captureLeft) > 60) {
      removeAt(captureLeft, 0);
    }
    while (len(captureRight) > 60) {
      removeAt(captureRight, 0);
    }
  }
}
