// POWER window (ui_function_power_window.cpp): three tabs.
//   Peripheral: USBA, P4USBC, Grove and M5BUS 5 V switches and the backlight
//               slider (1..100 %).
//   System:     L3B, LCD, AudioIC and AudioPA rails, the battery voltage and
//               the charge current (OFF, 180 mA, 650 mA).
//   M5BUS:      the 30 pins of the M5-Bus; tapping one of its 20 GPIOs opens
//               a popup that configures mode, level, drive and pull.
// Switching LCD, L3B or AudioIC off asks first: LCD off restarts the board
// 10 s later, L3B and AudioIC off leave only a Restart button.
import { f32, i32, idiv, len, push, type Color } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as dialog from "./dialog";

/** Rail indices, the board's names. */
export const L3B: i32 = 0;
export const LCD: i32 = 1;
export const AUDIO_IC: i32 = 2;
export const AUDIO_PA: i32 = 3;
export const USBA: i32 = 4;
export const P4USBC: i32 = 5;
export const GROVE: i32 = 6;
export const M5BUS: i32 = 7;
export const NAMES: string[] = ["L3B", "LCD", "AudioIC", "AudioPA", "USBA", "P4USBC", "Grove", "M5BUS"];

/** Dialog kinds of this window. */
export const DIALOG_WARNING: i32 = 1;
export const OWNER: i32 = 0;

let warning: i32 = -1;
/** Frames until the "now off" text after L3B/AudioIC went off (2000 ms). */
let restartCountdown: i32 = -1;

// ---- slider ---------------------------------------------------------------------

export const SLIDER_LEFT: i32 = 35;
export const SLIDER_WIDTH: i32 = 390;
export const SLIDER_TOP: i32 = 375;
let dragging: boolean = false;
let wasDown: boolean = false;

/** The knob's x on the screen for a value 1..100. */
export function knobX(value: i32): f32 {
  return f32(SLIDER_LEFT) + (f32(value - 1) * f32(SLIDER_WIDTH)) / f32(99);
}

/** One frame of the backlight slider on the Peripheral tab: a press on the
 *  slider (or its knob) moves the knob to the finger and drags it. */
export function stepSlider(tab: i32, x: i32, y: i32, down: boolean): void {
  if (down && !wasDown) {
    dragging = tab === 0 && x >= SLIDER_LEFT - 16 && x <= SLIDER_LEFT + SLIDER_WIDTH + 16 && y >= SLIDER_TOP - 8 && y <= SLIDER_TOP + 28;
  }
  if (!down) {
    dragging = false;
  }
  wasDown = down;
  if (dragging) {
    const t = (f32(x - SLIDER_LEFT) * f32(99)) / f32(SLIDER_WIDTH);
    let value = i32(t + f32(1.5));
    value = value < 1 ? 1 : value > 100 ? 100 : value;
    if (value !== device.backlight) {
      device.setBacklight(value);
    }
  }
}

// ---- rails ----------------------------------------------------------------------

function warn(rail: i32): void {
  warning = rail;
  restartCountdown = -1;
  if (rail === LCD) {
    dialog.show(OWNER, DIALOG_WARNING, "LCD Controller Power", "You are about to turn off the power rail that drives the LCD controller.\n\nThe display and touch input will stop working immediately. To keep this demo recoverable, the device will restart automatically 10 seconds after you confirm.", dialog.MEDIUM);
  } else if (rail === L3B) {
    dialog.show(OWNER, DIALOG_WARNING, "L3B Power Rail", "You are about to turn off L3B, the shared peripheral power rail.\n\nTouch, display support, audio and sensors may stop working after this rail is off. For a complete demo experience, restart is required after this operation.", dialog.MEDIUM);
  } else {
    dialog.show(OWNER, DIALOG_WARNING, "AudioIC Power", "You are about to turn off the audio codec power rail.\n\nAudio playback and recording will stop, and the audio pipeline may not recover cleanly without a restart. For a complete demo experience, restart is required after this operation.", dialog.MEDIUM);
  }
  dialog.button("Cancel", "#F3F4F6", "#111827", false);
  dialog.button("Confirm", "#DC2626", "#FFFFFF", false);
  dialog.button("Restart", "#D97706", "#FFFFFF", true);
}

export function toggle(rail: i32): void {
  const next = !device.rails[rail];
  if (!next && (rail === LCD || rail === L3B || rail === AUDIO_IC)) {
    warn(rail);
    return;
  }
  device.setRail(NAMES[rail], rail, next);
}

export function dialogButton(index: i32): void {
  if (index === 0) {
    dialog.close();
  } else if (index === 1) {
    if (warning === LCD) {
      // The board turns the LCD rail off and restarts 10 s later.
      device.request("lcdOff");
      dialog.close();
    } else {
      device.setRail(NAMES[warning], warning, false);
      dialog.setHidden(0, true);
      dialog.setHidden(1, true);
      dialog.setText("Applying the power change...");
      restartCountdown = 60;
    }
  } else {
    // esp_restart at once (the OFF window's restart dims the backlight
    // first).
    device.request("restart 1");
  }
}

/** One frame: the 2 s delay before the warning dialog offers Restart. */
export function step(): void {
  if (restartCountdown > 0) {
    restartCountdown = restartCountdown - 1;
    if (restartCountdown === 0 && dialog.open && dialog.owner === OWNER) {
      dialog.setText(
        warning === AUDIO_IC
          ? "AudioIC power is now off.\nRestart is required before audio playback or recording is used again."
          : "L3B power is now off.\nRestart is required to restore the full peripheral experience.",
      );
      dialog.setHidden(2, false);
    }
  }
}

export function close(): void {
  warning = -1;
  restartCountdown = -1;
  dragging = false;
  closePopup();
}

// ---- charge ---------------------------------------------------------------------

export const CHARGE_OFF: i32 = 0;
export const CHARGE_180: i32 = 1;
export const CHARGE_650: i32 = 2;

export function setCharge(value: i32): void {
  device.setCharge(value);
}

/** "%u.%02uV" of the filtered battery voltage, "-.--V" until it exists. */
export function vbatText(): string {
  const mv = device.vbatFiltered;
  if (mv <= 0) {
    return "-.--V";
  }
  const centivolts = idiv(mv + 5, 10);
  const fraction = centivolts - idiv(centivolts, 100) * 100;
  return `${idiv(centivolts, 100)}.${fraction < 10 ? "0" : ""}${fraction}V`;
}

// ---- M5BUS ----------------------------------------------------------------------

export const KIND_GPIO: i32 = 0;
export const KIND_VOLTAGE: i32 = 1;
export const KIND_GND: i32 = 2;
export const KIND_TEXT: i32 = 3;

export interface Pin {
  /** Bus pin number 1..30. */
  id: i32;
  left: i32;
  top: i32;
  kind: i32;
  gpio: i32;
  label: string;
  /** Two-line signal text sits 5 px higher. */
  twoLines: boolean;
  output: boolean;
  /** The level dot: 0 low, 1 high input, 2 high output. */
  dot: i32;
}

// m5_corep4_pinmap.h COREP4_M5BUS_P1..P30: the GPIO number, or -1 with the
// signal text.
const BUS_GPIO: i32[] = [-1, 17, -1, 20, -1, -1, 7, 21, 8, 22, 10, -1, 38, 37, 15, 14, 11, 9, 18, 16, 39, 12, 34, 23, -1, 19, -1, -1, -1, -1];
const BUS_TEXT: string[] = ["GND", "", "GND", "", "GND", "CHIP\nEN", "", "", "", "", "", "P4_3V3\nL3B", "", "", "", "", "", "", "", "", "", "", "", "", "HPWR", "", "HPWR", "5V\nINOUT", "HPWR", "VBAT"];
const BUS_KIND: i32[] = [2, 0, 2, 0, 2, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1];

/** Applied configuration per bus pin (index pin - 1): mode output, level,
 *  open drain, pull (0 none, 1 up, 2 down). Defaults: Input / OpenDrain /
 *  NoPull / low. */
let appliedOutput: boolean[] = [];
let appliedLevel: boolean[] = [];
let appliedOpenDrain: boolean[] = [];
let appliedPull: i32[] = [];

export function resetBus(): void {
  appliedOutput = [];
  appliedLevel = [];
  appliedOpenDrain = [];
  appliedPull = [];
  for (let i: i32 = 0; i < 30; i++) {
    push(appliedOutput, false);
    push(appliedLevel, false);
    push(appliedOpenDrain, true);
    push(appliedPull, 0);
  }
  closePopup();
}

function gpioLevel(gpio: i32): boolean {
  return gpio >= 0 && gpio < len(device.gpioLevels) && device.gpioLevels[gpio] !== 0;
}

/** The 30 pin cards: group g holds pins 10g+1..10g+10, odd pins in column
 *  L (x 29 + 152 g), even pins in column R (x 97 + 152 g), cards 53 px
 *  apart from y 109. */
export function pins(): Pin[] {
  const out: Pin[] = [];
  for (let i: i32 = 0; i < 30; i++) {
    const group = idiv(i, 10);
    const inGroup = i - group * 10;
    const column = inGroup - idiv(inGroup, 2) * 2;
    const row = idiv(inGroup, 2);
    const gpio = BUS_GPIO[i];
    const kind = BUS_KIND[i];
    const draftHere = popupPin === i + 1;
    const output = kind === KIND_GPIO && (draftHere ? draftOutput : appliedOutput[i]);
    const level = draftHere ? draftLevel : appliedLevel[i];
    let dot: i32 = 0;
    if (kind === KIND_GPIO) {
      if (output) {
        dot = level ? 2 : 0;
      } else {
        dot = gpioLevel(gpio) ? 1 : 0;
      }
    }
    push(out, {
      id: i + 1,
      left: 29 + group * 152 + column * 68,
      top: 109 + row * 53,
      kind: kind,
      gpio: gpio,
      label: kind === KIND_GPIO ? `GP-${gpio < 10 ? "0" : ""}${gpio}` : BUS_TEXT[i],
      twoLines: kind === KIND_TEXT || BUS_TEXT[i] === "P4_3V3\nL3B" || BUS_TEXT[i] === "5V\nINOUT",
      output: kind === KIND_GPIO && appliedOutput[i],
      dot: dot,
    });
  }
  return out;
}

// ---- GPIO popup ------------------------------------------------------------------

/** The pin whose popup is open (1..30), 0 for none. */
export let popupPin: i32 = 0;
export let draftOutput: boolean = false;
export let draftLevel: boolean = false;
export let draftOpenDrain: boolean = true;
export let draftPull: i32 = 0;
export let popupError: string = "";
let applying: boolean = false;

export function openPopup(pin: i32): void {
  if (BUS_KIND[pin - 1] !== KIND_GPIO) {
    return;
  }
  popupPin = pin;
  draftOutput = appliedOutput[pin - 1];
  draftLevel = appliedLevel[pin - 1];
  draftOpenDrain = appliedOpenDrain[pin - 1];
  draftPull = appliedPull[pin - 1];
  popupError = "";
  applying = false;
}

export function closePopup(): void {
  popupPin = 0;
  applying = false;
}

export function popupTitle(): string {
  return popupPin > 0 ? `P${popupPin}  GPIO${BUS_GPIO[popupPin - 1]}` : "";
}

export function popupStatus(): string {
  if (popupError !== "") {
    return popupError;
  }
  const pull = draftPull === 1 ? "PullUp" : draftPull === 2 ? "PullDown" : "NoPull";
  return `${draftOutput ? "Output" : "Input"} / ${draftOutput ? (draftLevel ? "High" : "Low") : "Level ignored"} / ${draftOpenDrain ? "OpenDrain" : "PushPull"} / ${pull}`;
}

export function setDraftOutput(output: boolean): void {
  draftOutput = output;
  popupError = "";
}
export function setDraftLevel(level: boolean): void {
  if (draftOutput) {
    draftLevel = level;
    popupError = "";
  }
}
export function setDraftOpenDrain(openDrain: boolean): void {
  draftOpenDrain = openDrain;
  popupError = "";
}
export function setDraftPull(pull: i32): void {
  draftPull = pull;
  popupError = "";
}

export function apply(): void {
  if (popupPin === 0 || applying) {
    return;
  }
  applying = true;
  device.request(`gpioConfig ${BUS_GPIO[popupPin - 1]} ${draftOutput ? 1 : 0} ${draftLevel ? 1 : 0} ${draftOpenDrain ? 1 : 0} ${draftPull}`);
}

/** The board's answer to gpioConfig: "" on success, else an esp_err_t name. */
export function applied(error: string): void {
  if (popupPin === 0 || !applying) {
    return;
  }
  applying = false;
  if (error === "") {
    const i = popupPin - 1;
    appliedOutput[i] = draftOutput;
    appliedLevel[i] = draftLevel;
    appliedOpenDrain[i] = draftOpenDrain;
    appliedPull[i] = draftPull;
    closePopup();
  } else {
    popupError = `GPIO config failed: ${error}.`;
  }
}

export function pinBg(kind: i32, output: boolean): Color {
  return kind === KIND_GPIO ? (output ? "#FEF3C7" : "#DCFCE7") : kind === KIND_VOLTAGE ? "#FEE2E2" : kind === KIND_GND ? "#4B5563" : "#F3F4F6";
}

export function count(): i32 {
  return len(appliedOutput);
}
