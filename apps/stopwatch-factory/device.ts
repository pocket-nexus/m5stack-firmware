// Device data bridge state (see the repository's app host documentation).
//
// The host writes the "host → app" fields before every frame and reads the
// "app → host" requests after it. Views and models reach hardware only
// through this module. Until a host bridge is attached (`bridged` stays
// false) the application advances `uptimeMs` and the wall clock itself and
// the requests have no effect.
import { floor, type f64, type i32 } from "@pocketjs/framework/solid/std";

// ----------------------------- host → app ---------------------------------

/** True once a host bridge writes these fields every frame. */
export let bridged: boolean = false;
/** Monotonic milliseconds since boot. */
export let uptimeMs: f64 = 0.0;

/** RTC wall clock (local time). month 1..12, weekday 0 = Sunday. */
export let year: i32 = 2026;
export let month: i32 = 1;
export let day: i32 = 1;
export let weekday: i32 = 4;
export let hour: i32 = 10;
export let minute: i32 = 8;
export let second: i32 = 0;

/** Battery level 0..100 and external power (PM1). */
export let batteryLevel: i32 = 80;
export let charging: boolean = false;

/** Raw touch contact in logical pixels (466 × 466). */
export let pointerDown: boolean = false;
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;

/** Raw key state: KEYA (yellow, left) and KEYB (blue, right). */
export let keyA: boolean = false;
export let keyB: boolean = false;

/** BMI270, factory axis convention: acceleration in g, rotation in °/s. */
export let accelX: f64 = 0.0;
export let accelY: f64 = 0.0;
export let accelZ: f64 = 1.0;
export let gyroX: f64 = 0.0;
export let gyroY: f64 = 0.0;
export let gyroZ: f64 = 0.0;

/** Microphone spectrum: 20 normalised bands (0..1) and the peak frequency. */
export let spectrum: f64[] = [
  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
];
export let peakHz: f64 = 0.0;
/** Host sets this while the microphone capture runs. */
export let micActive: boolean = false;

/** Persistent settings, loaded by the host before the first frame. */
export let brightness: i32 = 80;
export let volume: i32 = 80;
export let buttonSfx: boolean = true;
export let buttonVibrate: boolean = true;
export let wifiEnabled: boolean = false;
/** Boots counted for the home-gesture guide (the factory shows it 5 times). */
export let guideLaunchCount: i32 = 0;

/** Persistent alarms (at most 16), as parallel arrays. */
export let alarmHours: i32[] = [];
export let alarmMinutes: i32[] = [];
export let alarmEnabled: boolean[] = [];

/** Badge slots 0..5 that hold an image, and the active slot (-1: none). */
export let badgeSlots: boolean[] = [false, false, false, false, false, false];
export let badgeActive: i32 = -1;
/** Wi-Fi access point state for the badge editor. */
export let apSsid: string = "M5StopWatch-0000";
export let apMessage: string = "";

// ----------------------------- app → host ---------------------------------
// A request is a value plus a serial the app increments; the host acts when
// the serial differs from the last one it handled.

/** Apply `brightness` / `volume` / button settings; save when `saveSettings`. */
export let settingsSerial: i32 = 0;
export let saveSettings: boolean = false;

/** Play a tone: MIDI note and duration in seconds. */
export let toneMidi: i32 = 0;
export let toneSeconds: f64 = 0.0;
export let toneSerial: i32 = 0;

/** Vibrate the motor for `vibrateMs` at `vibrateStrength` (1..100). */
export let vibrateMs: i32 = 0;
export let vibrateStrength: i32 = 0;
export let vibrateSerial: i32 = 0;

/** Alarm sound loop on/off (trigger screen). */
export let alarmRinging: boolean = false;

/** Write the RTC from the fields below. */
export let rtcYear: i32 = 2026;
export let rtcMonth: i32 = 1;
export let rtcDay: i32 = 1;
export let rtcHour: i32 = 0;
export let rtcMinute: i32 = 0;
export let rtcSecond: i32 = 0;
export let rtcSerial: i32 = 0;

/** Save the alarm arrays. */
export let alarmsSerial: i32 = 0;
/** Count one guide display (increments `guideLaunchCount` persistently). */
export let guideSerial: i32 = 0;
/** Microphone capture for the FFT page. */
export let micRequested: boolean = false;
/** Select the badge slot in `badgeRequestSlot`. */
export let badgeRequestSlot: i32 = -1;
export let badgeSerial: i32 = 0;
/** Badge editor: start the access point and HTTP server while true. */
export let badgeEditing: boolean = false;
/** Play the boot sound. */
export let bootSfxSerial: i32 = 0;

// ----------------------------- helpers ------------------------------------

export function playTone(midi: i32, seconds: f64): void {
  toneMidi = midi;
  toneSeconds = seconds;
  toneSerial = toneSerial + 1;
}

export function vibrate(ms: i32, strength: i32): void {
  vibrateMs = ms;
  vibrateStrength = strength;
  vibrateSerial = vibrateSerial + 1;
}

export function applySettings(save: boolean): void {
  saveSettings = save;
  settingsSerial = settingsSerial + 1;
}

export function setRtc(y: i32, mo: i32, d: i32, h: i32, mi: i32, s: i32): void {
  rtcYear = y;
  rtcMonth = mo;
  rtcDay = d;
  rtcHour = h;
  rtcMinute = mi;
  rtcSecond = s;
  rtcSerial = rtcSerial + 1;
  if (!bridged) {
    year = y;
    month = mo;
    day = d;
    hour = h;
    minute = mi;
    second = s;
  }
}

function daysIn(y: i32, mo: i32): i32 {
  if (mo === 2) {
    return (y % 4 === 0 && y % 100 !== 0) || y % 400 === 0 ? 29 : 28;
  }
  return mo === 4 || mo === 6 || mo === 9 || mo === 11 ? 30 : 31;
}

/** Without a bridge: advance the simulated clock by one frame. */
export function simulate(frameMs: f64): void {
  if (bridged) {
    return;
  }
  const before = uptimeMs;
  uptimeMs = uptimeMs + frameMs;
  if (floor(uptimeMs / 1000.0) === floor(before / 1000.0)) {
    return;
  }
  second = second + 1;
  if (second < 60) {
    return;
  }
  second = 0;
  minute = minute + 1;
  if (minute < 60) {
    return;
  }
  minute = 0;
  hour = hour + 1;
  if (hour < 24) {
    return;
  }
  hour = 0;
  weekday = (weekday + 1) % 7;
  day = day + 1;
  if (day <= daysIn(year, month)) {
    return;
  }
  day = 1;
  month = month + 1;
  if (month <= 12) {
    return;
  }
  month = 1;
  year = year + 1;
}

export function countGuide(): void {
  guideSerial = guideSerial + 1;
}

export function playBootSfx(): void {
  bootSfxSerial = bootSfxSerial + 1;
}
