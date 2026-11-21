// AlarmClock app (factory app_alarm_clock): up to 16 daily alarms kept by
// the host, the list with enable switches and long-press deletion, the add
// screen with hour and minute rollers, and the trigger screen with the
// factory beep pattern (4 × 1760 Hz beeps 180 ms apart, then 720 ms).
import { clamp, f64, idiv, len, push, removeAt, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as roller from "./roller";

export const LIST: i32 = 0;
export const ADD: i32 = 1;

export const MAX_ALARMS: i32 = 16;
export let screen: i32 = 0;
/** List scroll in px. */
export let scroll: i32 = 0;
let scrollOrigin: i32 = 0;
let dragging: boolean = false;
/** Row index pressed for a long-press, -1 for none. */
let pressedRow: i32 = -1;
/** Row whose delete dialog is open, -1 for none. */
export let deleteRow: i32 = -1;
/** Suppresses the tap of a contact that scrolled or long-pressed. */
export let suppressTap: boolean = false;

/** Ringing alarm: index into the list, -1 when idle. */
export let ringing: i32 = -1;
export let ringHour: i32 = 0;
export let ringMinute: i32 = 0;
let ringStart = 0.0;
let nextBeep: i32 = 0;
let lastCheck = -10000.0;
/** Date key (yyyymmdd) of the last trigger per alarm. */
let lastTriggered: i32[] = [];

export function count(): i32 {
  return len(device.alarmHours);
}

export function open(): void {
  screen = LIST;
  scroll = 0;
  deleteRow = -1;
  roller.disableAll();
}

/** Row tops in list coordinates: title at 36, rows from 88 every 140 px. */
export function rowTop(index: i32): i32 {
  return 88 + index * 140;
}

function contentHeight(): i32 {
  return rowTop(count() + 1) + 72;
}

export function openAdd(): void {
  if (suppressTap) {
    return;
  }
  screen = ADD;
  // Rollers 160 px wide at centre offsets ±90, -13; 164 px tall.
  roller.setup(0, 24, true, 63, 138, 160, 7);
  roller.setup(1, 60, true, 243, 138, 160, 0);
}

export function confirmAdd(): void {
  if (count() < MAX_ALARMS) {
    device.addAlarm(roller.value(0), roller.value(1));
    push(lastTriggered, -1);
  }
  roller.disableAll();
  screen = LIST;
  scroll = 0;
}

export function toggle(index: i32): void {
  if (suppressTap || index < 0 || index >= count()) {
    return;
  }
  device.setAlarmEnabled(index, !device.alarmEnabled[index]);
  if (index < len(lastTriggered)) {
    lastTriggered[index] = -1;
  }
}

export function confirmDelete(): void {
  if (deleteRow >= 0 && deleteRow < count()) {
    device.removeAlarm(deleteRow);
    if (deleteRow < len(lastTriggered)) {
      removeAt(lastTriggered, deleteRow);
    }
  }
  deleteRow = -1;
}

export function cancelDelete(): void {
  deleteRow = -1;
}

export function dismiss(): void {
  ringing = -1;
  device.setAlarmRinging(false);
}

/** Checks the alarms once per second; runs on every page. */
export function check(): void {
  if (ringing >= 0) {
    ring();
    return;
  }
  if (device.uptimeMs - lastCheck < 1000.0) {
    return;
  }
  lastCheck = device.uptimeMs;
  const key = device.year * 10000 + device.month * 100 + device.day;
  while (len(lastTriggered) < count()) {
    push(lastTriggered, -1);
  }
  for (let i = 0; i < count(); i++) {
    if (
      device.alarmEnabled[i] &&
      device.alarmHours[i] === device.hour &&
      device.alarmMinutes[i] === device.minute &&
      lastTriggered[i] !== key
    ) {
      lastTriggered[i] = key;
      ringing = i;
      ringHour = device.hour;
      ringMinute = device.minute;
      ringStart = device.uptimeMs;
      nextBeep = 0;
      device.setAlarmRinging(true);
      return;
    }
  }
}

function ring(): void {
  // Beep n starts at 1260 ms per group of four plus 180 ms per beep.
  const start = f64(idiv(nextBeep, 4)) * 1260.0 + f64(nextBeep % 4) * 180.0;
  if (device.uptimeMs - ringStart >= start) {
    device.playTone(93, 0.07);
    device.vibrate(70, 90);
    nextBeep = nextBeep + 1;
  }
}

/** One frame of the list screen; returns true when the app should close. */
export function update(): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    roller.disableAll();
    return true;
  }
  if (screen === ADD) {
    roller.update();
    return false;
  }
  if (deleteRow >= 0) {
    return false;
  }
  if (input.pointerPressed) {
    suppressTap = false;
    dragging = false;
    scrollOrigin = scroll;
    pressedRow = -1;
    for (let i = 0; i < count(); i++) {
      const y = rowTop(i) - scroll;
      if (input.pointerX >= 46 && input.pointerX < 420 && input.pointerY >= y && input.pointerY < y + 119) {
        pressedRow = i;
      }
    }
  }
  if (input.pointerDown) {
    if (input.moved) {
      dragging = true;
      suppressTap = true;
      pressedRow = -1;
    }
    if (dragging) {
      const limit = contentHeight() > 466 ? contentHeight() - 466 : 0;
      scroll = clamp(scrollOrigin - (input.pointerY - input.startY), 0, limit);
    }
    if (pressedRow >= 0 && device.uptimeMs - input.startMs >= 400.0) {
      deleteRow = pressedRow;
      pressedRow = -1;
      suppressTap = true;
    }
  }
  if (input.pointerReleased) {
    dragging = false;
    pressedRow = -1;
  }
  return false;
}
