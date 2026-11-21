// Settings app (factory app_setup): the scrolling menu, brightness and
// volume sliders, button sound/vibration switches, Wi-Fi switch, time and
// date rollers, firmware version and the ten-tap "About" blue screen.
import { clamp, f64, idiv, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as roller from "./roller";

export const MENU: i32 = 0;
export const BRIGHTNESS: i32 = 1;
export const VOLUME: i32 = 2;
export const BUTTON: i32 = 3;
export const WIFI: i32 = 4;
export const TIME: i32 = 5;
export const DATE: i32 = 6;
export const ABOUT: i32 = 7;

export const VERSION: string = "V0.5";

export let screen: i32 = 0;
export let scroll: i32 = 0;
let scrollOrigin: i32 = 0;
let dragging: boolean = false;
export let suppressTap: boolean = false;
let versionTaps: i32 = 0;
/** Slider value of the brightness or volume page. */
export let level: i32 = 0;
let sliding: boolean = false;
/** Date page stage: 0 year and month, 1 day. */
export let dateStage: i32 = 0;
/** About page progress in percent. */
export let progress: i32 = 0;
let burstLeft: i32 = 0;
let nextStep = 0.0;
let seed: i32 = 12345;

/** Menu content: rows end at 1042 + 119; the panel pads 72 px below. */
const CONTENT: i32 = 1042 + 119 + 21 + 5 + 72;

export function open(): void {
  screen = MENU;
  scroll = 0;
  versionTaps = 0;
  roller.disableAll();
}

function random(low: i32, high: i32): i32 {
  seed = (seed * 75 + 74) % 65537;
  return low + (seed % (high - low + 1));
}

export function openItem(item: i32): void {
  if (suppressTap) {
    return;
  }
  if (item === BRIGHTNESS) {
    level = device.brightness;
  } else if (item === VOLUME) {
    level = device.volume;
  } else if (item === TIME) {
    // Rollers 110 px wide at centre offsets -120, 0, 120; -13.
    roller.setup(0, 24, true, 58, 138, 110, device.hour);
    roller.setup(1, 60, true, 178, 138, 110, device.minute);
    roller.setup(2, 60, true, 298, 138, 110, device.second);
  } else if (item === DATE) {
    dateStage = 0;
    roller.setup(0, 100, false, 73, 143, 170, clamp(device.year - 2000, 0, 99));
    roller.setup(1, 12, true, 273, 143, 110, device.month - 1);
  }
  screen = item;
}

export function tapVersion(): void {
  if (suppressTap) {
    return;
  }
  versionTaps = versionTaps + 1;
  if (versionTaps >= 10) {
    versionTaps = 0;
    screen = ABOUT;
    progress = 0;
    burstLeft = 0;
    nextStep = device.uptimeMs + f64(random(700, 2600));
  }
}

export function daysInMonth(year: i32, month: i32): i32 {
  if (month === 2) {
    return (year % 4 === 0 && year % 100 !== 0) || year % 400 === 0 ? 29 : 28;
  }
  return month === 4 || month === 6 || month === 9 || month === 11 ? 30 : 31;
}

/** OK button of every sub-page. */
export function confirm(): void {
  if (screen === BRIGHTNESS) {
    device.setBrightness(level, true);
  } else if (screen === VOLUME) {
    device.setVolume(level, true);
  } else if (screen === BUTTON) {
    device.setButtonSfx(device.buttonSfx, true);
    device.setButtonVibrate(device.buttonVibrate, true);
  } else if (screen === TIME) {
    device.setRtc(device.year, device.month, device.day, roller.value(0), roller.value(1), roller.value(2));
  } else if (screen === DATE) {
    if (dateStage === 0) {
      dateStage = 1;
      const year = 2000 + roller.value(0);
      const month = roller.value(1) + 1;
      const days = daysInMonth(year, month);
      roller.disableAll();
      roller.setup(0, 100, false, -1000, 143, 170, year - 2000);
      roller.setup(1, 12, true, -1000, 143, 110, month - 1);
      roller.disable(0);
      roller.disable(1);
      roller.setup(2, days, false, 163, 143, 140, clamp(device.day, 1, days) - 1);
      return;
    }
    device.setRtc(2000 + roller.value(0), roller.value(1) + 1, roller.value(2) + 1, device.hour, device.minute, device.second);
  }
  roller.disableAll();
  screen = MENU;
  scroll = 0;
}

export function toggleSfx(): void {
  device.setButtonSfx(!device.buttonSfx, false);
}

export function toggleVibrate(): void {
  device.setButtonVibrate(!device.buttonVibrate, false);
}

export function toggleWifi(): void {
  device.setWifiEnabled(!device.wifiEnabled);
}

/** Summary line of the time and date pages. */
export function two(value: i32): string {
  return value < 10 ? `0${value}` : `${value}`;
}

export function summary(): string {
  if (screen === TIME) {
    return `${two(roller.value(0))}:${two(roller.value(1))}:${two(roller.value(2))}`;
  }
  if (dateStage === 0) {
    return `${2000 + roller.value(0)}-${two(roller.value(1) + 1)}`;
  }
  return `${2000 + roller.value(0)}-${two(roller.value(1) + 1)}-${two(roller.value(2) + 1)}`;
}

export function wifiStatus(): string {
  if (!device.wifiEnabled) {
    return "Off";
  }
  return device.apMessage === "" ? "Wi-Fi error" : device.apMessage;
}

/** One frame; returns true when the app should close. */
export function update(): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    roller.disableAll();
    return true;
  }
  if (screen === MENU) {
    updateMenu();
  } else if (screen === BRIGHTNESS || screen === VOLUME) {
    updateSlider();
  } else if (screen === TIME || screen === DATE) {
    roller.update();
  } else if (screen === ABOUT) {
    updateAbout();
  }
  return false;
}

function updateMenu(): void {
  if (input.pointerPressed) {
    suppressTap = false;
    dragging = false;
    scrollOrigin = scroll;
  }
  if (input.pointerDown && input.moved) {
    dragging = true;
    suppressTap = true;
    scroll = clamp(scrollOrigin - (input.pointerY - input.startY), 0, CONTENT - 466);
  }
  if (input.pointerReleased) {
    dragging = false;
  }
}

function updateSlider(): void {
  // Slider 374 × 24 centred at (233, 225); the touch band is 80 px tall.
  if (input.pointerPressed) {
    sliding = input.pointerY >= 185 && input.pointerY <= 265;
  }
  if (input.pointerDown && sliding) {
    const low = screen === BRIGHTNESS ? 10 : 0;
    const step = screen === BRIGHTNESS ? 1 : 5;
    const raw = low + round(f64(clamp(input.pointerX - 46, 0, 374)) / 374.0 * f64(100 - low));
    level = clamp(low + idiv(raw - low + idiv(step, 2), step) * step, low, 100);
    if (screen === BRIGHTNESS) {
      device.setBrightness(level, false);
    } else {
      device.setVolume(level, false);
    }
  }
  if (input.pointerReleased) {
    sliding = false;
  }
}

function updateAbout(): void {
  if (device.uptimeMs < nextStep) {
    return;
  }
  if (burstLeft <= 0) {
    burstLeft = random(1, 5);
  }
  progress = progress + random(1, 8);
  burstLeft = burstLeft - 1;
  nextStep = device.uptimeMs + f64(burstLeft > 0 ? random(60, 180) : random(700, 2600));
}
