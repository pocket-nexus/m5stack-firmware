// The Tab5 factory demo model: the startup animation, the launcher's live
// labels and switches, the panel windows with their open and close motion,
// the toasts and the sounds. Geometry is the factory's 1280x720 raster; the
// views run at 640x360 logical pixels, so positions are halved on the way
// out. Hardware goes through ./device only.
import { createSignal } from "solid-js";
import {
  abs,
  clamp,
  copy,
  f32,
  fixed,
  frames,
  idiv,
  imod,
  len,
  max,
  push,
  removeAt,
  round,
  codePoints,
  fromCodePoint,
  type f64,
  type Deg,
  type i32,
  type Px,
} from "@pocketjs/framework/solid/std";
import {
  accelX,
  accelY,
  accelZ,
  busVoltage,
  cameraCapturing,
  captureLeft,
  captureRight,
  chargeEnabled,
  cpuTemperature,
  ext5vEnabled,
  extAntennaEnabled,
  gyroX,
  gyroY,
  gyroZ,
  headphoneConnected,
  hostAttached,
  i2cAddresses,
  i2cScans,
  musicPlaying,
  panelIc,
  playNote,
  playNotes,
  playStartupSound,
  pointerDown,
  pointerX,
  pointerY,
  quickChargeEnabled,
  recordTestState,
  requestPower,
  requestSdScan,
  rs485Chunks,
  rs485Text,
  rtcDay,
  rtcHour,
  rtcMinute,
  rtcMonth,
  rtcSecond,
  rtcYear,
  sdDirectories,
  sdList,
  sdScans,
  sendRs485,
  setAudioCapture,
  setBacklight,
  setCameraWanted,
  setChargeEnabled,
  setExt5vEnabled,
  setExtAntennaEnabled,
  setGpioLevel,
  setGpioTest,
  setI2cScanBus,
  setMusicWanted,
  setQuickChargeEnabled,
  setRs485Open,
  setRtc,
  setSpeakerVolume,
  setUsbA5vEnabled,
  shuntCurrent,
  simulateFrame,
  simulateSecond,
  speakerVolume,
  startRecordTest,
  usbA5vEnabled,
  usbAConnected,
  usbCConnected,
  backlight,
} from "./device";
import * as device from "./device";
import {
  retarget,
  settled,
  spring,
  stepSpring,
  stepTween,
  teleport,
  tween,
  tweenDone,
  tweenJump,
  tweenTo,
  type Spring,
  type Tween,
} from "./motion";
import { daysInMonth, monthCells, pad2, type DayCell } from "./calendar";

const HZ: i32 = 30;
const DT: f32 = 0.033333335;
/** One logical pixel; multiplying an f32 by it gives a Px. */
const PX: Px = 1;
/** Factory raster pixels to logical pixels. */
const HALF: f32 = 0.5;

// ---- windows ------------------------------------------------------------------
// Index of each window; the keyframes are the factory's (center offset x, y,
// width, height on the 1280x720 screen).
const WINDOW_COUNT: i32 = 15;
const WIN_RTC: i32 = 0;
const WIN_POWER_OFF: i32 = 1;
const WIN_SLEEP_TOUCH: i32 = 2;
const WIN_SLEEP_SHAKE: i32 = 3;
const WIN_SLEEP_RTC: i32 = 4;
const WIN_CAMERA: i32 = 5;
const WIN_MIC: i32 = 6;
const WIN_HEADPHONE: i32 = 7;
const WIN_SD: i32 = 8;
const WIN_I2C: i32 = 9;
const WIN_EXT_PORT: i32 = 10;
const WIN_MBUS: i32 = 11;
const WIN_MUSIC: i32 = 12;
const WIN_COM: i32 = 13;
const WIN_WIFI: i32 = 14;
const CLOSED_X: i32[] = [436, 562, 562, 562, 562, 598, -154, -310, -46, -505, 413, 413, 225, 497, -309];
const CLOSED_Y: i32[] = [-219, -319, -245, -172, -98, 0, -318, 301, 300, 229, -115, -115, 279, 5, -187];
const CLOSED_W: i32[] = [75, 146, 146, 146, 146, 74, 75, 75, 75, 75, 75, 75, 75, 75, 60];
const CLOSED_H: i32[] = [75, 70, 70, 70, 70, 74, 75, 75, 75, 75, 75, 75, 75, 75, 60];
const OPEN_X: i32[] = [214, 123, 123, 123, 123, 141, -215, -289, -40, -356, -438, 183, 236, 60, 34];
const OPEN_Y: i32[] = [85, -200, -200, -163, -116, 0, -217, 130, 43, -12, 68, -15, 116, 0, -70];
const OPEN_W: i32[] = [800, 703, 703, 703, 703, 800, 670, 403, 566, 512, 273, 520, 413, 643, 602];
const OPEN_H: i32[] = [480, 291, 291, 291, 291, 480, 171, 208, 411, 356, 380, 547, 204, 435, 332];

const STATE_CLOSED: i32 = 0;
const STATE_OPENING: i32 = 1;
const STATE_OPENED: i32 = 2;
const STATE_CLOSING: i32 = 3;

interface WindowMotion {
  state: i32;
  x: Spring;
  y: Spring;
  w: Spring;
  h: Spring;
  opacity: Tween;
}

/** A window as the view draws it, in logical pixels. */
export interface WindowView {
  visible: boolean;
  /** Opening or opened: the window and its background take touches. */
  open: boolean;
  left: Px;
  top: Px;
  width: Px;
  height: Px;
  opacity: f32;
}

let motions: WindowMotion[] = [];
export const [windows, setWindows] = createSignal<WindowView[]>([]);

function closedWindow(kind: i32): WindowMotion {
  return {
    state: STATE_CLOSED,
    x: spring(f32(CLOSED_X[kind]), f32(0.4), f32(0.2)),
    y: spring(f32(CLOSED_Y[kind]), f32(0.4), f32(0.2)),
    w: spring(f32(CLOSED_W[kind]), f32(0.4), f32(0.2)),
    h: spring(f32(CLOSED_H[kind]), f32(0.4), f32(0.2)),
    opacity: tween(f32(0), f32(0)),
  };
}

function windowView(m: WindowMotion): WindowView {
  const width: f32 = max(m.w.value, f32(0));
  const height: f32 = max(m.h.value, f32(0));
  return {
    visible: m.state !== STATE_CLOSED,
    open: m.state === STATE_OPENING || m.state === STATE_OPENED,
    left: PX * ((f32(640) + m.x.value - width * HALF) * HALF),
    top: PX * ((f32(360) + m.y.value - height * HALF) * HALF),
    width: PX * (width * HALF),
    height: PX * (height * HALF),
    opacity: clamp(m.opacity.value, f32(0), f32(1)),
  };
}

function publishWindows(): void {
  const views: WindowView[] = [];
  for (let i: i32 = 0; i < WINDOW_COUNT; i++) {
    push(views, windowView(motions[i]));
  }
  setWindows(views);
}

function windowState(kind: i32): i32 {
  return motions[kind].state;
}

/** Window::open: from the closed keyframe (teleported, transparent) to the
 *  opened one; the opacity jumps to full. */
function openWindow(kind: i32): void {
  const m: WindowMotion = closedWindow(kind);
  motions[kind] = {
    state: STATE_OPENING,
    x: retarget(m.x, f32(OPEN_X[kind])),
    y: retarget(m.y, f32(OPEN_Y[kind])),
    w: retarget(m.w, f32(OPEN_W[kind])),
    h: retarget(m.h, f32(OPEN_H[kind])),
    opacity: tweenJump(m.opacity, f32(1)),
  };
  windowOpened(kind);
}

/** Window::close: back to the closed keyframe, fading out over 0.8 s. */
function closeWindow(kind: i32): void {
  const m: WindowMotion = motions[kind];
  if (m.state === STATE_CLOSED || m.state === STATE_CLOSING) {
    return;
  }
  motions[kind] = {
    state: STATE_CLOSING,
    x: retarget(m.x, f32(CLOSED_X[kind])),
    y: retarget(m.y, f32(CLOSED_Y[kind])),
    w: retarget(m.w, f32(CLOSED_W[kind])),
    h: retarget(m.h, f32(CLOSED_H[kind])),
    opacity: tweenTo(m.opacity, f32(0), f32(0.8)),
  };
  windowClosed(kind);
}

function stepWindows(): void {
  let moving: boolean = false;
  for (let i: i32 = 0; i < WINDOW_COUNT; i++) {
    const m: WindowMotion = motions[i];
    if (m.state === STATE_CLOSED || m.state === STATE_OPENED) {
      continue;
    }
    moving = true;
    const next: WindowMotion = {
      state: m.state,
      x: stepSpring(m.x, DT),
      y: stepSpring(m.y, DT),
      w: stepSpring(m.w, DT),
      h: stepSpring(m.h, DT),
      opacity: stepTween(m.opacity, DT),
    };
    if (settled(next.x) && settled(next.y) && settled(next.w) && settled(next.h) && tweenDone(next.opacity)) {
      next.state = m.state === STATE_OPENING ? STATE_OPENED : STATE_CLOSED;
    }
    motions[i] = next;
  }
  if (moving) {
    publishWindows();
  }
}

// ---- sounds ---------------------------------------------------------------------

const PROGRESSION: i32[] = [60, 67, 69, 64, 65, 60, 65, 67];
const SCALE: i32[] = [60, 62, 64, 65, 67, 69, 71];
let progressionCalls: i32 = 0;
let randomSeed: i32 = 2025;

/** audio::play_next_tone_progression, two octaves up. */
function nextTone(): void {
  playNote(PROGRESSION[imod(progressionCalls, 8)] + 24);
  progressionCalls = progressionCalls + 1;
}

/** audio::play_random_tone: a C major note, two octaves up. */
function randomTone(): void {
  randomSeed = imod(randomSeed * 1103 + 12345, 65536);
  playNote(SCALE[imod(idiv(randomSeed, 16), 7)] + 24);
}

// ---- toasts ---------------------------------------------------------------------

interface ToastMotion {
  id: i32;
  text: string;
  state: i32;
  depth: i32;
  y: Spring;
  w: Spring;
  /** Frames the toast has been fully open. */
  shown: i32;
}

export interface ToastView {
  id: i32;
  text: string;
  left: Px;
  top: Px;
  width: Px;
}

let toastMotions: ToastMotion[] = [];
let toastQueue: string[] = [];
let nextToastId: i32 = 1;
export const [toasts, setToasts] = createSignal<ToastView[]>([]);

/** ui::pop_a_toast: at most five waiting; one starts per frame. */
function popToast(text: string): void {
  if (len(toastQueue) < 5) {
    push(toastQueue, text);
  }
}

function stepToasts(): void {
  let changed: boolean = false;
  if (len(toastQueue) > 0) {
    const text: string = toastQueue[0];
    removeAt(toastQueue, 0);
    // The new toast pushes the open ones down and narrows them.
    for (let s: i32 = 0; s < len(toastMotions); s++) {
      const open: ToastMotion = toastMotions[s];
      if (open.state === STATE_OPENING || open.state === STATE_OPENED) {
        const depth: i32 = open.depth + 1;
        toastMotions[s] = {
          id: open.id,
          text: open.text,
          state: open.state,
          depth: depth,
          y: retarget(open.y, f32(16 + depth * (12 - depth * 2))),
          w: retarget(open.w, f32(640 - depth * 40)),
          shown: open.shown,
        };
      }
    }
    if (len(toastMotions) >= 3) {
      removeAt(toastMotions, 0);
    }
    push(toastMotions, {
      id: nextToastId,
      text: text,
      state: STATE_OPENING,
      depth: 0,
      y: retarget(spring(f32(-100), f32(0.4), f32(0.3)), f32(16)),
      w: retarget(spring(f32(320), f32(0.4), f32(0.3)), f32(640)),
      shown: 0,
    });
    nextToastId = nextToastId + 1;
    changed = true;
  }
  let i: i32 = 0;
  while (i < len(toastMotions)) {
    const t: ToastMotion = toastMotions[i];
    const y: Spring = stepSpring(t.y, DT);
    const w: Spring = stepSpring(t.w, DT);
    let state: i32 = t.state;
    let shown: i32 = t.shown;
    if (settled(y) && settled(w)) {
      if (state === STATE_OPENING) {
        state = STATE_OPENED;
      } else if (state === STATE_CLOSING) {
        state = STATE_CLOSED;
      }
    }
    if (state === STATE_OPENED) {
      shown = shown + 1;
    }
    if (!settled(t.y) || !settled(t.w) || state !== t.state) {
      changed = true;
    }
    if (state === STATE_CLOSED) {
      removeAt(toastMotions, i);
      continue;
    }
    // 1600 ms on screen, then back up.
    if (state === STATE_OPENED && shown > idiv(1600 * HZ, 1000)) {
      state = STATE_CLOSING;
      toastMotions[i] = { id: t.id, text: t.text, state: state, depth: t.depth, y: retarget(y, f32(-100)), w: retarget(w, f32(320)), shown: shown };
      changed = true;
    } else {
      toastMotions[i] = { id: t.id, text: t.text, state: state, depth: t.depth, y: y, w: w, shown: shown };
    }
    i++;
  }
  if (changed) {
    const views: ToastView[] = [];
    for (let k: i32 = 0; k < len(toastMotions); k++) {
      const shownToast: ToastMotion = toastMotions[k];
      push(views, {
        id: shownToast.id,
        text: shownToast.text,
        left: PX * ((f32(364) - shownToast.w.value * HALF) * HALF),
        top: PX * (shownToast.y.value * HALF),
        width: PX * (shownToast.w.value * HALF),
      });
    }
    setToasts(views);
  }
}

export function dismissToast(id: i32): void {
  for (let i: i32 = 0; i < len(toastMotions); i++) {
    const t: ToastMotion = toastMotions[i];
    if (t.id === id && t.state !== STATE_CLOSING) {
      toastMotions[i] = { id: t.id, text: t.text, state: STATE_CLOSING, depth: t.depth, y: retarget(t.y, f32(-100)), w: retarget(t.w, f32(320)), shown: t.shown };
    }
  }
}

// ---- startup animation -----------------------------------------------------------

/** 0 startup animation, 1 launcher. */
export const [screen, setScreen] = createSignal<i32>(0);
export const [logoTabTop, setLogoTabTop] = createSignal<Px>(392);
export const [logoTabOpacity, setLogoTabOpacity] = createSignal<f32>(0);
export const [logo5Left, setLogo5Left] = createSignal<Px>(655);

let startupPhase: i32 = 0;
let startupFrames: i32 = 0;
let logoTabY: Spring = { value: 0, velocity: 0, target: 0, stiffness: 0, damping: 0 };
let logoTabFade: Tween = { value: 0, from: 0, target: 0, elapsed: 0, duration: 0 };
let logo5X: Spring = { value: 0, velocity: 0, target: 0, stiffness: 0, damping: 0 };
let startupSoundPlayed: boolean = false;

function stepStartup(): void {
  startupFrames = startupFrames + 1;
  if (startupPhase === 0) {
    // 400 ms, then the "Tab" logo rises and fades in.
    if (startupFrames > idiv(400 * HZ, 1000)) {
      startupPhase = 1;
      logoTabY = retarget(logoTabY, f32(309));
      logoTabFade = tweenTo(logoTabFade, f32(1), f32(0.6));
    }
    return;
  }
  if (startupPhase === 1) {
    logoTabY = stepSpring(logoTabY, DT);
    logoTabFade = stepTween(logoTabFade, DT);
    if (settled(logoTabY)) {
      startupPhase = 2;
      logo5X = retarget(logo5X, f32(82));
    }
  } else if (startupPhase === 2) {
    logoTabFade = stepTween(logoTabFade, DT);
    logo5X = stepSpring(logo5X, DT);
    if (!startupSoundPlayed && logo5X.value < f32(90)) {
      setSpeakerVolume(80);
      playStartupSound();
      startupSoundPlayed = true;
    }
    if (settled(logo5X)) {
      startupPhase = 3;
      startupFrames = 0;
    }
  } else if (startupPhase === 3) {
    // One second on the finished logo, then the launcher.
    if (startupFrames > HZ) {
      setSpeakerVolume(60);
      enterLauncher();
    }
    return;
  }
  // The logo images are 180x80 and 60x80, aligned to the top middle.
  setLogoTabTop(PX * (logoTabY.value * HALF));
  setLogoTabOpacity(logoTabFade.value);
  setLogo5Left(PX * ((f32(610) + logo5X.value) * HALF));
}

// ---- launcher labels ----------------------------------------------------------------

export const [timeText, setTimeText] = createSignal<string>("..");
export const [dateText, setDateText] = createSignal<string>("..");
export const [voltageText, setVoltageText] = createSignal<string>("..");
export const [currentText, setCurrentText] = createSignal<string>("..");
export const [charging, setCharging] = createSignal<boolean>(true);
export const [cpuText, setCpuText] = createSignal<string>("..");
export const [accelXText, setAccelXText] = createSignal<string>("..");
export const [accelYText, setAccelYText] = createSignal<string>("..");
export const [accelZText, setAccelZText] = createSignal<string>("..");
export const [brightness, setBrightness] = createSignal<i32>(100);
export const [volume, setVolume] = createSignal<i32>(60);
export const [brightnessShift, setBrightnessShift] = createSignal<Px>(0);
export const [volumeShift, setVolumeShift] = createSignal<Px>(0);
export const [panelName, setPanelName] = createSignal<string>("");
export const [dotShiftX, setDotShiftX] = createSignal<Px>(0);
export const [dotShiftY, setDotShiftY] = createSignal<Px>(0);
export const [dotScale, setDotScale] = createSignal<f32>(0.379);
export const [chargeOn, setChargeOn] = createSignal<boolean>(true);
export const [quickChargeOn, setQuickChargeOn] = createSignal<boolean>(true);
export const [ext5vOn, setExt5vOn] = createSignal<boolean>(true);
export const [usbA5vOn, setUsbA5vOn] = createSignal<boolean>(true);
export const [antennaExternal, setAntennaExternal] = createSignal<boolean>(false);
export const [usbCDetected, setUsbCDetected] = createSignal<boolean>(false);
export const [usbADetected, setUsbADetected] = createSignal<boolean>(false);
export const [headphoneDetected, setHeadphoneDetected] = createSignal<boolean>(false);

let launcherFrames: i32 = 0;
let brightnessBump: Tween = { value: 0, from: 0, target: 0, elapsed: 0, duration: 0 };
let volumeBump: Tween = { value: 0, from: 0, target: 0, elapsed: 0, duration: 0 };
let dotX: Tween = { value: 0, from: 0, target: 0, elapsed: 0, duration: 0 };
let dotY: Tween = { value: 0, from: 0, target: 0, elapsed: 0, duration: 0 };
let dotSize: Tween = { value: 0, from: 0, target: 0, elapsed: 0, duration: 0 };
let lastUsbA: boolean = false;
let lastHeadphone: boolean = false;

function enterLauncher(): void {
  setScreen(1);
  launcherFrames = 0;
  lastUsbA = usbAConnected;
  lastHeadphone = headphoneConnected;
  setPanelName(`LCD IC: ${panelIc}`);
  setBrightness(backlight);
  setVolume(speakerVolume);
  updateClock();
  updatePowerMonitor();
  updateCpu();
  updateImu();
  updateSwitches();
  publishWindows();
}

function updateClock(): void {
  setTimeText(`${rtcHour}:${pad2(rtcMinute)}:${pad2(rtcSecond)}`);
  setDateText(`${rtcYear}/${rtcMonth}/${rtcDay}`);
}

function updatePowerMonitor(): void {
  setVoltageText(`${fixed(busVoltage, 2)}V`);
  setCurrentText(`${fixed(shuntCurrent, 2)}A`);
  setCharging(shuntCurrent >= f32(0));
}

function updateCpu(): void {
  setCpuText(`${cpuTemperature}`);
}

/** PanelImu::update: the dot follows the acceleration (50 px per g, at most
 *  50) and grows with motion, 22 px at rest to 58 px. */
function updateImu(): void {
  setAccelXText(`X:${fixed(accelX, 1)}`);
  setAccelYText(`Y:${fixed(accelY, 1)}`);
  setAccelZText(`Z:${fixed(accelZ, 1)}`);
  dotX = tweenTo(dotX, f32(clamp(round(accelX * f32(50)), -50, 50)), f32(0.1));
  dotY = tweenTo(dotY, f32(clamp(round(accelY * f32(50)), -50, 50)), f32(0.1));
  const gyro: f32 = max(max(abs(gyroX), abs(gyroY)), abs(gyroZ));
  const accel: f32 = max(max(abs(accelX), abs(accelY)), abs(accelZ));
  const score: f32 = max(gyro, accel) * f32(10);
  let size: f32 = f32(22);
  if (score >= f32(20)) {
    size = f32(58);
  } else if (score > f32(10)) {
    size = f32(22) + (score - f32(10)) * f32(36) / f32(10);
  }
  dotSize = tweenTo(dotSize, size, f32(0.3));
}

function updateSwitches(): void {
  setChargeOn(chargeEnabled);
  setQuickChargeOn(quickChargeEnabled);
  setExt5vOn(ext5vEnabled);
  setUsbA5vOn(usbA5vEnabled);
  setAntennaExternal(extAntennaEnabled);
  setUsbCDetected(usbCConnected);
  setUsbADetected(usbAConnected);
  setHeadphoneDetected(headphoneConnected);
  if (lastUsbA !== usbAConnected) {
    lastUsbA = usbAConnected;
    popToast(usbAConnected ? "USB-A connected" : "USB-A disconnected");
    plugSound(usbAConnected);
  }
  if (lastHeadphone !== headphoneConnected) {
    lastHeadphone = headphoneConnected;
    popToast(headphoneConnected ? "Headphone connected" : "Headphone disconnected");
    plugSound(headphoneConnected);
  }
}

function plugSound(pluggedIn: boolean): void {
  playNotes(pluggedIn ? [84, 88] : [88, 84], 100);
}

function stepLabels(): void {
  if (!tweenDone(brightnessBump)) {
    brightnessBump = stepTween(brightnessBump, DT);
    setBrightnessShift(PX * (brightnessBump.value * HALF));
  }
  if (!tweenDone(volumeBump)) {
    volumeBump = stepTween(volumeBump, DT);
    setVolumeShift(PX * (volumeBump.value * HALF));
  }
  if (!tweenDone(dotX) || !tweenDone(dotY) || !tweenDone(dotSize)) {
    dotX = stepTween(dotX, DT);
    dotY = stepTween(dotY, DT);
    dotSize = stepTween(dotSize, DT);
    setDotShiftX(PX * (dotX.value * HALF));
    setDotShiftY(PX * (dotY.value * HALF));
    setDotScale(dotSize.value / f32(58));
  }
}

// ---- launcher buttons ------------------------------------------------------------------

export function brightnessUp(): void {
  brightnessBump = tweenTo(tweenJump(brightnessBump, f32(-8)), f32(0), f32(0.2));
  if (brightness() >= 100) {
    playNote(96);
    return;
  }
  playNote(88);
  const target: i32 = clamp(brightness() + 20, 0, 100);
  setBacklight(target);
  setBrightness(target);
}

export function brightnessDown(): void {
  brightnessBump = tweenTo(tweenJump(brightnessBump, f32(8)), f32(0), f32(0.2));
  if (brightness() <= 20) {
    playNote(76);
    return;
  }
  playNote(84);
  const target: i32 = clamp(brightness() - 20, 20, 100);
  setBacklight(target);
  setBrightness(target);
}

export function volumeUp(): void {
  volumeBump = tweenTo(tweenJump(volumeBump, f32(-8)), f32(0), f32(0.2));
  if (volume() >= 100) {
    playNote(96);
    return;
  }
  const target: i32 = clamp(volume() + 20, 0, 100);
  setSpeakerVolume(target);
  setVolume(target);
  playNote(88);
}

export function volumeDown(): void {
  volumeBump = tweenTo(tweenJump(volumeBump, f32(8)), f32(0), f32(0.2));
  if (volume() <= 0) {
    return;
  }
  playNote(84);
  const target: i32 = clamp(volume() - 20, 0, 100);
  setSpeakerVolume(target);
  setVolume(target);
}

export function toggleCharge(): void {
  randomTone();
  setChargeEnabled(!chargeEnabled);
  popToast(chargeEnabled ? "Battery charging enabled" : "Battery charging disabled");
  updateSwitches();
}

export function toggleQuickCharge(): void {
  randomTone();
  setQuickChargeEnabled(!quickChargeEnabled);
  popToast(quickChargeEnabled ? "QC charging enabled" : "QC charging disabled");
  updateSwitches();
}

export function toggleExt5v(): void {
  randomTone();
  setExt5vEnabled(!ext5vEnabled);
  popToast(ext5vEnabled ? " EXT 5V enabled" : "EXT 5V disabled");
  updateSwitches();
}

export function toggleUsbA5v(): void {
  randomTone();
  setUsbA5vEnabled(!usbA5vEnabled);
  popToast(usbA5vEnabled ? "USB-A 5V enabled" : "USB-A 5V disabled");
  updateSwitches();
}

export function toggleAntenna(): void {
  randomTone();
  setExtAntennaEnabled(!extAntennaEnabled);
  popToast(extAntennaEnabled ? "Switched to external antenna" : "Switched to internal antenna");
  updateSwitches();
  if (windowState(WIN_WIFI) === STATE_CLOSED) {
    openWindow(WIN_WIFI);
  }
}

export function showWifiInfo(): void {
  randomTone();
  if (windowState(WIN_WIFI) === STATE_CLOSED) {
    openWindow(WIN_WIFI);
  }
}

/** A launcher panel button: the next progression tone and its window. */
export function openPanel(kind: i32): void {
  nextTone();
  openWindow(kind);
}

/** The tap outside a window that closes it, or its close button. */
export function dismissWindow(kind: i32): void {
  if (windowState(kind) === STATE_OPENING || windowState(kind) === STATE_OPENED) {
    if (kind === WIN_CAMERA) {
      closeCamera();
      return;
    }
    closeWindow(kind);
  }
}

// ---- window contents --------------------------------------------------------------------

/** Window::onOpen per window. */
function windowOpened(kind: i32): void {
  if (kind === WIN_RTC) {
    setRtcReady(false);
  } else if (kind === WIN_POWER_OFF) {
    powerOffCountdown = 15;
    powerOffFrames = 0;
    setCountdown(15);
  } else if (kind === WIN_CAMERA) {
    cameraStarted = false;
    cameraClosing = false;
    setCameraMessage("Opening Camera ...");
    setCameraVisible(false);
    setCameraFullscreen(false);
  } else if (kind === WIN_MIC) {
    setAudioCapture(1);
    recordFrames = 0;
  } else if (kind === WIN_HEADPHONE) {
    setAudioCapture(2);
    recordFrames = 0;
  } else if (kind === WIN_SD) {
    sdScanned = false;
    setSdMessage("Scanning SD Card ...");
    setSdError(false);
    setSdEntries([]);
  } else if (kind === WIN_I2C) {
    setI2cInternal(true);
    setI2cScanBus(1);
    setI2cFound([]);
    i2cSeen = i2cScans;
  } else if (kind === WIN_EXT_PORT) {
    setGpioReady(false);
    setExtPortScroll(0);
    setMbusScroll(0);
    openWindow(WIN_MBUS);
  } else if (kind === WIN_MUSIC) {
    setAudioCapture(3);
  } else if (kind === WIN_COM) {
    setRs485Open(true);
    rs485Seen = rs485Chunks;
    rs485Log = "";
    setComLines([]);
  }
}

/** Window::onClose per window. */
function windowClosed(kind: i32): void {
  if (kind === WIN_RTC) {
    nextTone();
    setDropdown(0);
  } else if (kind === WIN_POWER_OFF || kind === WIN_SLEEP_TOUCH || kind === WIN_SLEEP_SHAKE) {
    nextTone();
  } else if (kind === WIN_CAMERA || kind === WIN_SD || kind === WIN_HEADPHONE || kind === WIN_MIC) {
    nextTone();
    if (kind === WIN_MIC || kind === WIN_HEADPHONE) {
      setAudioCapture(0);
    }
  } else if (kind === WIN_I2C) {
    nextTone();
    setI2cScanBus(0);
  } else if (kind === WIN_EXT_PORT) {
    nextTone();
    setGpioReady(false);
    setGpioTest(false, []);
    closeWindow(WIN_MBUS);
  } else if (kind === WIN_MUSIC) {
    setMusicWanted(false);
    setAudioCapture(0);
  } else if (kind === WIN_COM) {
    nextTone();
    setRs485Open(false);
  }
}

// RTC setting

export const [rtcReady, setRtcReady] = createSignal<boolean>(false);
export const [calendarYear, setCalendarYear] = createSignal<i32>(2025);
export const [calendarMonth, setCalendarMonth] = createSignal<i32>(1);
export const [calendarCells, setCalendarCells] = createSignal<DayCell[]>([]);
export const [selectedYear, setSelectedYear] = createSignal<i32>(2025);
export const [selectedMonth, setSelectedMonth] = createSignal<i32>(1);
export const [selectedDay, setSelectedDay] = createSignal<i32>(1);
export const [rollerHour, setRollerHour] = createSignal<i32>(0);
export const [rollerMinute, setRollerMinute] = createSignal<i32>(0);
export const [rollerSecond, setRollerSecond] = createSignal<i32>(0);
/** 0 closed, 1 year list, 2 month list. */
export const [dropdown, setDropdown] = createSignal<i32>(0);
export const [yearOptions, setYearOptions] = createSignal<i32[]>([]);

export interface MonthOption {
  month: i32;
  name: string;
}

export const MONTH_NAMES: string[] = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
export const [monthOptions, setMonthOptions] = createSignal<MonthOption[]>([]);

/** The month dropdown's text. */
export const [monthName, setMonthName] = createSignal<string>("January");

function showMonth(year: i32, month: i32): void {
  setMonthName(MONTH_NAMES[month - 1]);
  setCalendarYear(year);
  setCalendarMonth(month);
  setCalendarCells(monthCells(year, month));
}

/** RtcSettingWindow::onUpdate: the calendar and rollers appear once the
 *  window has opened, set to the current time. */
function prepareRtc(): void {
  showMonth(rtcYear, rtcMonth);
  setSelectedYear(rtcYear);
  setSelectedMonth(rtcMonth);
  setSelectedDay(rtcDay);
  setRollerHour(rtcHour);
  setRollerMinute(rtcMinute);
  setRollerSecond(rtcSecond);
  const years: i32[] = [];
  for (let y: i32 = rtcYear + 10; y > rtcYear - 16; y--) {
    push(years, y);
  }
  setYearOptions(years);
  const months: MonthOption[] = [];
  for (let m: i32 = 1; m <= 12; m++) {
    push(months, { month: m, name: MONTH_NAMES[m - 1] });
  }
  setMonthOptions(months);
  setRtcReady(true);
}

export function pickDay(cell: i32): void {
  const cells: DayCell[] = calendarCells();
  if (cell < 0 || cell >= len(cells)) {
    return;
  }
  const picked: DayCell = cells[cell];
  let year: i32 = calendarYear();
  let month: i32 = calendarMonth();
  if (!picked.current) {
    if (cell < 7) {
      month = month - 1;
    } else {
      month = month + 1;
    }
    if (month < 1) {
      month = 12;
      year = year - 1;
    } else if (month > 12) {
      month = 1;
      year = year + 1;
    }
  }
  setSelectedYear(year);
  setSelectedMonth(month);
  setSelectedDay(picked.day);
}

export function toggleDropdown(which: i32): void {
  setDropdown(dropdown() === which ? 0 : which);
}

export function pickYear(year: i32): void {
  setDropdown(0);
  showMonth(year, calendarMonth());
}

export function pickMonth(month: i32): void {
  setDropdown(0);
  showMonth(calendarYear(), month);
}

export function applyRtc(): void {
  const year: i32 = selectedYear();
  const month: i32 = selectedMonth();
  const day: i32 = clamp(selectedDay(), 1, daysInMonth(year, month));
  popToast(`Set RTC time to ${year}/${month}/${day} ${rollerHour()}:${pad2(rollerMinute())}:${pad2(rollerSecond())}`);
  setRtc(year, month, day, rollerHour(), rollerMinute(), rollerSecond());
}

// Power windows

export const [countdown, setCountdown] = createSignal<i32>(15);
let powerOffCountdown: i32 = 15;
let powerOffFrames: i32 = 0;

export function cancelPower(kind: i32): void {
  closeWindow(kind);
}

export function powerOff(): void {
  requestPower(1);
}

export function sleepTouch(): void {
  requestPower(2);
  closeWindow(WIN_SLEEP_TOUCH);
}

export function sleepShake(): void {
  requestPower(3);
}

export function sleepRtc(): void {
  requestPower(4);
}

// Camera

export const [cameraMessage, setCameraMessage] = createSignal<string>("Opening Camera ...");
export const [cameraVisible, setCameraVisible] = createSignal<boolean>(false);
export const [cameraFullscreen, setCameraFullscreen] = createSignal<boolean>(false);
let cameraStarted: boolean = false;
let cameraClosing: boolean = false;

function closeCamera(): void {
  if (!cameraStarted) {
    closeWindow(WIN_CAMERA);
    return;
  }
  if (!cameraClosing) {
    cameraClosing = true;
    setCameraVisible(false);
    setCameraMessage("Closing Camera ...");
    setCameraWanted(false);
  }
}

export function toggleCameraSize(): void {
  setCameraFullscreen(!cameraFullscreen());
}

function stepCamera(): void {
  if (cameraClosing) {
    if (cameraCapturing) {
      return;
    }
    cameraClosing = false;
    cameraStarted = false;
    setCameraMessage("Camera Closed");
    closeWindow(WIN_CAMERA);
  }
  if (windowState(WIN_CAMERA) !== STATE_OPENED) {
    return;
  }
  if (!cameraStarted) {
    setCameraWanted(true);
    cameraStarted = true;
    setCameraVisible(true);
  }
}

// Record test, headphone and music windows

/** 0 idle, 1 recording, 2 playing (the HAL's MicTestState). */
export const [recordState, setRecordState] = createSignal<i32>(0);
/** The spinner arc's start angle: one turn a second. */
export const [spinnerAngle, setSpinnerAngle] = createSignal<Deg>(0);
const DEGREE: Deg = 1;
export const [musicOn, setMusicOn] = createSignal<boolean>(false);
export interface ChartColumn {
  id: i32;
  height: Px;
}

export const [chartA, setChartA] = createSignal<ChartColumn[]>([]);
export const [chartB, setChartB] = createSignal<ChartColumn[]>([]);
let recordFrames: i32 = 0;

export function startDualMicTest(): void {
  startRecordTest(1);
  setRecordState(1);
}

export function startHeadphoneTest(): void {
  startRecordTest(2);
  setRecordState(1);
}

export function toggleMusic(): void {
  setMusicWanted(!musicOn());
  setMusicOn(!musicOn());
}

/** Waveform columns: a level of 100 fills the 60 px chart. */
function columns(levels: i32[]): ChartColumn[] {
  const out: ChartColumn[] = [];
  const count: i32 = len(levels);
  for (let i: i32 = 0; i < 60; i++) {
    const source: i32 = count - 60 + i;
    const level: i32 = source >= 0 && source < count ? levels[source] : 0;
    push(out, { id: i, height: PX * (f32(max(clamp(level, 0, 100), 1)) * f32(0.6)) });
  }
  return out;
}

function stepAudioWindows(): void {
  const mic: boolean = windowState(WIN_MIC) !== STATE_CLOSED;
  const headphone: boolean = windowState(WIN_HEADPHONE) !== STATE_CLOSED;
  const music: boolean = windowState(WIN_MUSIC) !== STATE_CLOSED;
  if (!mic && !headphone && !music) {
    return;
  }
  if (recordState() !== 0 || musicOn()) {
    setSpinnerAngle(DEGREE * f32(imod(frameCount * 12, 360)));
  }
  setChartA(columns(captureLeft));
  setChartB(columns(captureRight));
  recordFrames = recordFrames + 1;
  if (recordFrames >= idiv(200 * HZ, 1000)) {
    recordFrames = 0;
    setRecordState(recordTestState);
    setMusicOn(musicPlaying);
    setHeadphoneDetected(headphoneConnected);
  }
}

// SD card

export interface SdEntry {
  id: i32;
  name: string;
  directory: boolean;
}

export const [sdMessage, setSdMessage] = createSignal<string>("");
export const [sdError, setSdError] = createSignal<boolean>(false);
export const [sdEntries, setSdEntries] = createSignal<SdEntry[]>([]);
let sdScanned: boolean = false;
let sdSeen: i32 = 0;

function stepSd(): void {
  if (windowState(WIN_SD) !== STATE_OPENED) {
    return;
  }
  if (!sdScanned) {
    sdScanned = true;
    sdSeen = sdScans;
    requestSdScan();
    return;
  }
  if (sdSeen === sdScans || sdMessage() === "") {
    return;
  }
  sdSeen = sdScans;
  // sdList: names separated by "/" (a character FAT names cannot hold).
  const entries: SdEntry[] = [];
  const characters: i32[] = codePoints(sdList);
  let name: string = "";
  for (let i: i32 = 0; i <= len(characters); i++) {
    if (i === len(characters) || characters[i] === 47) {
      if (name !== "" && len(entries) < 8) {
        const index: i32 = len(entries);
        push(entries, { id: index, name: name, directory: index < len(sdDirectories) && sdDirectories[index] !== 0 });
      }
      name = "";
    } else {
      name = `${name}${fromCodePoint(characters[i])}`;
    }
  }
  if (len(entries) === 0) {
    setSdMessage("No files found on SD Card.");
    return;
  }
  setSdEntries(entries);
  setSdMessage("");
}

// I2C scan

export interface I2cLabel {
  address: i32;
  text: string;
  left: Px;
  top: Px;
}

export const [i2cInternal, setI2cInternal] = createSignal<boolean>(true);
export const [i2cFound, setI2cFound] = createSignal<I2cLabel[]>([]);
let i2cSeen: i32 = 0;
const HEX: string[] = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"];

export function scanPortA(): void {
  setI2cInternal(false);
  playNote(84);
  setI2cFound([]);
  setI2cScanBus(ext5vEnabled ? 2 : 0);
}

export function scanInternal(): void {
  setI2cInternal(true);
  playNote(87);
  setI2cFound([]);
  setI2cScanBus(1);
}

export function togglePortAPower(): void {
  setExt5vEnabled(!ext5vEnabled);
  playNote(ext5vEnabled ? 87 : 84);
  popToast(ext5vEnabled ? " EXT 5V enabled" : "EXT 5V disabled");
  updateSwitches();
  if (!i2cInternal()) {
    setI2cScanBus(ext5vEnabled ? 2 : 0);
    setI2cFound([]);
  }
}

function stepI2c(): void {
  if (windowState(WIN_I2C) === STATE_CLOSED || windowState(WIN_I2C) === STATE_CLOSING) {
    return;
  }
  if (i2cSeen === i2cScans) {
    return;
  }
  i2cSeen = i2cScans;
  if (!i2cInternal() && !ext5vEnabled) {
    setI2cFound([]);
    return;
  }
  // The address grid of the chart picture: 27 px per column and row.
  const labels: I2cLabel[] = [];
  for (let i: i32 = 0; i < len(i2cAddresses); i++) {
    const address: i32 = i2cAddresses[i];
    const row: i32 = idiv(address, 16);
    const column: i32 = imod(address, 16);
    push(labels, {
      address: address,
      text: `${HEX[row]}${HEX[column]}`,
      left: PX * (f32(78 + column * 27) * HALF - f32(7)),
      top: PX * (f32(136 + row * 27) * HALF - f32(6)),
    });
  }
  setI2cFound(labels);
}

// GPIO test

export interface GpioRow {
  pin: i32;
  high: boolean;
  left: Px;
  top: Px;
}

const EXT_PORT_PINS: i32[] = [49, 50, 0, 1, 54, 53];
const MBUS_PINS: i32[] = [18, 19, 5, 38, 7, 3, 2, 47, 16, 17, 45, 52, 37, 6, 4, 48, 35, 51];
export const [gpioReady, setGpioReady] = createSignal<boolean>(false);
export const [extPortRows, setExtPortRows] = createSignal<GpioRow[]>([]);
export const [mbusRows, setMbusRows] = createSignal<GpioRow[]>([]);
export const [extPortScroll, setExtPortScroll] = createSignal<Px>(0);
export const [mbusScroll, setMbusScroll] = createSignal<Px>(0);

/** ExtPortWindow and MbusWindow: once open, every pin is an output driven
 *  low; ExtPort rows sit at 55 + 81 i, MBUS rows in two columns of nine. */
function prepareGpio(): void {
  const all: i32[] = copy(EXT_PORT_PINS);
  for (let i: i32 = 0; i < len(MBUS_PINS); i++) {
    push(all, MBUS_PINS[i]);
  }
  setGpioTest(true, all);
  const ext: GpioRow[] = [];
  for (let i: i32 = 0; i < len(EXT_PORT_PINS); i++) {
    push(ext, { pin: EXT_PORT_PINS[i], high: false, left: PX * (f32(273 - 230) * HALF * HALF), top: PX * (f32(55 + 81 * i) * HALF) });
  }
  const mbus: GpioRow[] = [];
  for (let i: i32 = 0; i < len(MBUS_PINS); i++) {
    const center: i32 = i > 8 ? 260 - 124 : 260 + 124;
    push(mbus, { pin: MBUS_PINS[i], high: false, left: PX * (f32(center - 115) * HALF), top: PX * (f32(55 + 81 * imod(i, 9)) * HALF) });
  }
  setExtPortRows(ext);
  setMbusRows(mbus);
  setGpioReady(true);
}

function toggleRow(rows: GpioRow[], pin: i32): GpioRow[] {
  const next: GpioRow[] = copy(rows);
  for (let i: i32 = 0; i < len(next); i++) {
    if (next[i].pin === pin) {
      next[i].high = !next[i].high;
      playNote(next[i].high ? 87 : 84);
      setGpioLevel(pin, next[i].high);
    }
  }
  return next;
}

export function toggleExtPortPin(pin: i32): void {
  setExtPortRows(toggleRow(extPortRows(), pin));
}

export function toggleMbusPin(pin: i32): void {
  setMbusRows(toggleRow(mbusRows(), pin));
}

// RS485 monitor

export interface ComLine {
  id: i32;
  text: string;
}

export const [comLines, setComLines] = createSignal<ComLine[]>([]);
let rs485Seen: i32 = 0;
let rs485Log: string = "";
let comLineId: i32 = 0;

function appendCom(text: string): void {
  const lines: ComLine[] = copy(comLines());
  push(lines, { id: comLineId, text: text });
  comLineId = comLineId + 1;
  while (len(lines) > 15) {
    removeAt(lines, 0);
  }
  setComLines(lines);
}

export function sendHello(): void {
  nextTone();
  appendCom("<<< Hello M5Stack!");
  sendRs485("Hello M5Stack!");
}

function stepCom(): void {
  if (windowState(WIN_COM) !== STATE_OPENED || rs485Seen === rs485Chunks) {
    return;
  }
  rs485Seen = rs485Chunks;
  if (rs485Text !== rs485Log) {
    rs485Log = rs485Text;
    appendCom(rs485Text);
  }
}

// ---- host entry points ----------------------------------------------------------------
// main.tsx reaches the device bridge through these root exports.

export function hostNumber(name: string, value: f64): void {
  device.hostNumber(name, value);
}

export function hostText(name: string, value: string): void {
  device.hostText(name, value);
}

export function hostArray(name: string, values: f32[]): void {
  device.hostArray(name, values);
}

export function hostEvent(name: string, value: f64): void {
  device.hostEvent(name, value);
}

export function takeRequests(): string[] {
  return device.takeRequests();
}

// ---- drags ---------------------------------------------------------------------------
// Rollers and the GPIO windows follow a finger from the host's raw pointer.

const DRAG_NONE: i32 = 0;
const DRAG_HOUR: i32 = 1;
const DRAG_MINUTE: i32 = 2;
const DRAG_SECOND: i32 = 3;
const DRAG_EXT_PORT: i32 = 4;
const DRAG_MBUS: i32 = 5;
let dragTarget: i32 = DRAG_NONE;
let dragStartY: f32 = 0;
let dragStartValue: i32 = 0;
let dragStartScroll: f32 = 0;
let pointerWasDown: boolean = false;

function inside(x: f32, y: f32, left: f32, top: f32, width: f32, height: f32): boolean {
  return x >= left && x < left + width && y >= top && y < top + height;
}

/** The opened window rectangle of `kind` in logical pixels. */
function windowLeft(kind: i32): f32 {
  return f32(640 + OPEN_X[kind] - idiv(OPEN_W[kind], 2)) * HALF;
}

function windowTop(kind: i32): f32 {
  return f32(360 + OPEN_Y[kind] - idiv(OPEN_H[kind], 2)) * HALF;
}

function stepDrag(): void {
  if (pointerDown && !pointerWasDown) {
    dragTarget = DRAG_NONE;
    dragStartY = f32(pointerY);
    if (windowState(WIN_RTC) === STATE_OPENED && rtcReady()) {
      // Rollers 150x44 at (-290, 185), (-110, 185), (70, 185) of the window center.
      const top: f32 = windowTop(WIN_RTC) + f32(240 + 185 - 22) * HALF;
      const left: f32 = windowLeft(WIN_RTC);
      if (inside(f32(pointerX), f32(pointerY), left + f32(400 - 290 - 75) * HALF, top, f32(75), f32(22))) {
        dragTarget = DRAG_HOUR;
        dragStartValue = rollerHour();
      } else if (inside(f32(pointerX), f32(pointerY), left + f32(400 - 110 - 75) * HALF, top, f32(75), f32(22))) {
        dragTarget = DRAG_MINUTE;
        dragStartValue = rollerMinute();
      } else if (inside(f32(pointerX), f32(pointerY), left + f32(400 + 70 - 75) * HALF, top, f32(75), f32(22))) {
        dragTarget = DRAG_SECOND;
        dragStartValue = rollerSecond();
      }
    }
    if (windowState(WIN_MBUS) === STATE_OPENED && inside(f32(pointerX), f32(pointerY), windowLeft(WIN_MBUS), windowTop(WIN_MBUS), f32(260), f32(273.5))) {
      dragTarget = DRAG_MBUS;
      dragStartScroll = f32(mbusScroll());
    } else if (windowState(WIN_EXT_PORT) === STATE_OPENED && inside(f32(pointerX), f32(pointerY), windowLeft(WIN_EXT_PORT), windowTop(WIN_EXT_PORT), f32(136.5), f32(190))) {
      dragTarget = DRAG_EXT_PORT;
      dragStartScroll = f32(extPortScroll());
    }
  } else if (pointerDown && dragTarget !== DRAG_NONE) {
    const dy: f32 = f32(pointerY) - dragStartY;
    // One roller row is 22 logical pixels; dragging up shows later values.
    const steps: i32 = round(-dy / f32(22));
    if (dragTarget === DRAG_HOUR) {
      setRollerHour(imod(imod(dragStartValue + steps, 24) + 24, 24));
    } else if (dragTarget === DRAG_MINUTE) {
      setRollerMinute(imod(imod(dragStartValue + steps, 60) + 60, 60));
    } else if (dragTarget === DRAG_SECOND) {
      setRollerSecond(imod(imod(dragStartValue + steps, 60) + 60, 60));
    } else if (dragTarget === DRAG_EXT_PORT) {
      // Content: rows to 55 + 81 * 5 + 62, plus 24 of bottom padding.
      setExtPortScroll(PX * clamp(dragStartScroll + dy, f32(380 - 546) * HALF, f32(0)));
    } else if (dragTarget === DRAG_MBUS) {
      setMbusScroll(PX * clamp(dragStartScroll + dy, f32(547 - 789) * HALF, f32(0)));
    }
  } else if (!pointerDown) {
    dragTarget = DRAG_NONE;
  }
  pointerWasDown = pointerDown;
}

// ---- the frame loop ------------------------------------------------------------------------

let frameCount: i32 = 0;

function stepLauncher(): void {
  launcherFrames = launcherFrames + 1;
  // The factory panels poll their HAL values at these periods.
  if (imod(launcherFrames, HZ) === 0) {
    updateClock();
    updateCpu();
  }
  if (imod(launcherFrames, 3) === 0) {
    updatePowerMonitor();
    updateImu();
  }
  if (imod(launcherFrames, 6) === 0) {
    updateSwitches();
  }
  stepLabels();
  stepWindows();
  stepToasts();
  if (windowState(WIN_RTC) === STATE_OPENED && !rtcReady()) {
    prepareRtc();
  }
  if (windowState(WIN_POWER_OFF) === STATE_OPENING || windowState(WIN_POWER_OFF) === STATE_OPENED) {
    powerOffFrames = powerOffFrames + 1;
    if (powerOffFrames >= HZ) {
      powerOffFrames = 0;
      powerOffCountdown = powerOffCountdown - 1;
      if (powerOffCountdown < 0) {
        powerOff();
      }
      setCountdown(powerOffCountdown);
    }
  }
  if (windowState(WIN_EXT_PORT) === STATE_OPENED && !gpioReady()) {
    prepareGpio();
  }
  stepCamera();
  stepAudioWindows();
  stepSd();
  stepI2c();
  stepCom();
  stepDrag();
}

function step(): void {
  frameCount = frameCount + 1;
  if (!hostAttached) {
    simulateFrame(HZ);
    if (imod(frameCount, HZ) === 0) {
      simulateSecond();
    }
  }
  if (screen() === 0) {
    stepStartup();
  } else {
    stepLauncher();
  }
}

/** Mount: the window table, then one model step per frame. */
export async function start(): Promise<void> {
  logoTabY = spring(f32(785), f32(0.6), f32(0.2));
  logoTabFade = tween(f32(0), f32(0.6));
  logo5X = spring(f32(700), f32(0.4), f32(0.2));
  brightnessBump = tween(f32(0), f32(0.2));
  volumeBump = tween(f32(0), f32(0.2));
  dotX = tween(f32(0), f32(0.1));
  dotY = tween(f32(0), f32(0.1));
  dotSize = tween(f32(22), f32(0.3));
  for (let i: i32 = 0; i < WINDOW_COUNT; i++) {
    push(motions, closedWindow(i));
  }
  publishWindows();
  setBrightness(backlight);
  setVolume(speakerVolume);
  // Bounded loop: over two years of frames at 30 Hz, one step each.
  for (let i: i32 = 0; i < 2147483647; i++) {
    await frames(1);
    step();
  }
}
