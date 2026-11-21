// Device bridge state (docs/APP-HOST.md). The board publishes the
// "board → app" fields by name through m5_pocket_bridge_*; the application
// asks the board for work only through request(). Views and models reach
// hardware only through this module.
//
// Until the board publishes `uptimeMs`, `bridged` stays false and the
// application advances `uptimeMs` and the wall clock itself (desktop and
// preview builds).
import { clear, copy, f64, floor, i32, len, push, removeAt, type f32 } from "@pocketjs/framework/solid/std";

// ----------------------------- board → app ---------------------------------

/** True once the board publishes `uptimeMs`. */
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

/** Touch contact in logical pixels (466 × 466), published by the host. */
export let pointerDown: boolean = false;
export let pointerX: i32 = 0;
export let pointerY: i32 = 0;

/** Raw key state: KEYA (yellow, left) and KEYB (blue, right). */
export let keyA: boolean = false;
export let keyB: boolean = false;

/** BMI270 in the factory axis convention: acceleration in g, rotation in °/s. */
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

/** Persistent settings, published by the board at boot. */
export let brightness: i32 = 80;
export let volume: i32 = 80;
export let buttonSfx: boolean = true;
export let buttonVibrate: boolean = true;
export let wifiEnabled: boolean = false;
/** Boots on which the home-gesture guide was shown (the factory shows it 5 times). */
export let guideLaunchCount: i32 = 5;

/** Persistent alarms (at most 16), as parallel arrays. */
export let alarmHours: i32[] = [];
export let alarmMinutes: i32[] = [];
export let alarmEnabled: boolean[] = [];

/** Badge slots 0..5 that hold an image, and the active slot (-1: none). */
export let badgeSlots: boolean[] = [false, false, false, false, false, false];
export let badgeActive: i32 = -1;
/** Badge editor access point name and status text. */
export let apSsid: string = "M5StopWatch-0000";
export let apMessage: string = "";

// ----------------------------- app → board ---------------------------------

export let requests: string[] = [];
/** The alarm ringing pattern runs (trigger screen). */
export let alarmRinging: boolean = false;

export function request(command: string): void {
  push(requests, command);
}

export function playTone(midi: i32, seconds: f64): void {
  request(`tone ${midi} ${seconds}`);
}

export function vibrate(ms: i32, strength: i32): void {
  request(`vibrate ${ms} ${strength}`);
}

/** Level requests; `save` stores the value in NVS. */
export function setBrightness(value: i32, save: boolean): void {
  brightness = value;
  request(`brightness ${value} ${save ? 1 : 0}`);
}

export function setVolume(value: i32, save: boolean): void {
  volume = value;
  request(`volume ${value} ${save ? 1 : 0}`);
}

export function setButtonSfx(on: boolean, save: boolean): void {
  buttonSfx = on;
  request(`button_sfx ${on ? 1 : 0} ${save ? 1 : 0}`);
}

export function setButtonVibrate(on: boolean, save: boolean): void {
  buttonVibrate = on;
  request(`button_vibrate ${on ? 1 : 0} ${save ? 1 : 0}`);
}

export function setWifiEnabled(on: boolean): void {
  wifiEnabled = on;
  request(`wifi ${on ? 1 : 0}`);
}

export function setRtc(y: i32, mo: i32, d: i32, h: i32, mi: i32, s: i32): void {
  request(`rtc ${y} ${mo} ${d} ${h} ${mi} ${s}`);
  if (!bridged) {
    year = y;
    month = mo;
    day = d;
    hour = h;
    minute = mi;
    second = s;
  }
}

export function countGuide(): void {
  guideLaunchCount = guideLaunchCount + 1;
  request("guide_count");
}

export function playBootSfx(): void {
  request("boot_sfx");
}

export function setAlarmRinging(on: boolean): void {
  alarmRinging = on;
  request(`alarm_ring ${on ? 1 : 0}`);
}

export function setMic(on: boolean): void {
  request(`mic ${on ? 1 : 0}`);
}

export function requestBadge(slot: i32): void {
  request(`badge_slot ${slot}`);
}

export function setBadgeEditing(on: boolean): void {
  request(`badge_edit ${on ? 1 : 0}`);
}

/** Stores the alarm list: alarm_clear, one `alarm h m enabled` each, alarm_save. */
function saveAlarms(): void {
  request("alarm_clear");
  for (let i = 0; i < len(alarmHours); i++) {
    request(`alarm ${alarmHours[i]} ${alarmMinutes[i]} ${alarmEnabled[i] ? 1 : 0}`);
  }
  request("alarm_save");
}

export function addAlarm(h: i32, m: i32): void {
  push(alarmHours, h);
  push(alarmMinutes, m);
  push(alarmEnabled, true);
  saveAlarms();
}

export function removeAlarm(index: i32): void {
  removeAt(alarmHours, index);
  removeAt(alarmMinutes, index);
  removeAt(alarmEnabled, index);
  saveAlarms();
}

export function setAlarmEnabled(index: i32, enabled: boolean): void {
  alarmEnabled[index] = enabled;
  saveAlarms();
}

// ----------------------------- host entry points ---------------------------

export function hostNumber(name: string, value: f64): void {
  switch (name) {
    case "uptimeMs":
      uptimeMs = value;
      bridged = true;
      break;
    case "year":
      year = i32(value);
      break;
    case "month":
      month = i32(value);
      break;
    case "day":
      day = i32(value);
      break;
    case "weekday":
      weekday = i32(value);
      break;
    case "hour":
      hour = i32(value);
      break;
    case "minute":
      minute = i32(value);
      break;
    case "second":
      second = i32(value);
      break;
    case "batteryLevel":
      batteryLevel = i32(value);
      break;
    case "charging":
      charging = value !== 0.0;
      break;
    case "pointerDown":
      pointerDown = value !== 0.0;
      break;
    case "pointerX":
      pointerX = i32(value);
      break;
    case "pointerY":
      pointerY = i32(value);
      break;
    case "keyA":
      keyA = value !== 0.0;
      break;
    case "keyB":
      keyB = value !== 0.0;
      break;
    case "accelX":
      accelX = value;
      break;
    case "accelY":
      accelY = value;
      break;
    case "accelZ":
      accelZ = value;
      break;
    case "gyroX":
      gyroX = value;
      break;
    case "gyroY":
      gyroY = value;
      break;
    case "gyroZ":
      gyroZ = value;
      break;
    case "peakHz":
      peakHz = value;
      break;
    case "brightness":
      brightness = i32(value);
      break;
    case "volume":
      volume = i32(value);
      break;
    case "buttonSfx":
      buttonSfx = value !== 0.0;
      break;
    case "buttonVibrate":
      buttonVibrate = value !== 0.0;
      break;
    case "wifiEnabled":
      wifiEnabled = value !== 0.0;
      break;
    case "guideLaunchCount":
      guideLaunchCount = i32(value);
      break;
    case "badgeActive":
      badgeActive = i32(value);
      break;
    default:
      break;
  }
}

export function hostText(name: string, value: string): void {
  switch (name) {
    case "apSsid":
      apSsid = value;
      break;
    case "apMessage":
      apMessage = value;
      break;
    default:
      break;
  }
}

export function hostArray(name: string, values: f32[]): void {
  switch (name) {
    case "spectrum":
      for (let i = 0; i < len(values) && i < 20; i++) {
        spectrum[i] = f64(values[i]);
      }
      break;
    case "alarmHours":
      clear(alarmHours);
      for (const v of values) {
        push(alarmHours, i32(v));
      }
      break;
    case "alarmMinutes":
      clear(alarmMinutes);
      for (const v of values) {
        push(alarmMinutes, i32(v));
      }
      break;
    case "alarmEnabled":
      clear(alarmEnabled);
      for (const v of values) {
        push(alarmEnabled, v !== 0.0);
      }
      break;
    case "badgeSlots":
      for (let i = 0; i < len(values) && i < 6; i++) {
        badgeSlots[i] = values[i] !== 0.0;
      }
      break;
    default:
      break;
  }
}

export function hostEvent(name: string, value: f64): void {
  hostNumber(name, value);
}

export function takeRequests(): string[] {
  const taken = copy(requests);
  clear(requests);
  return taken;
}

// ----------------------------- simulation ----------------------------------

function daysIn(y: i32, mo: i32): i32 {
  if (mo === 2) {
    return (y % 4 === 0 && y % 100 !== 0) || y % 400 === 0 ? 29 : 28;
  }
  return mo === 4 || mo === 6 || mo === 9 || mo === 11 ? 30 : 31;
}

/** Without a board: advance the simulated clock by one frame. */
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
