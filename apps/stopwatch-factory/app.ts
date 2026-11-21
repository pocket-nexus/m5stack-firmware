// Root model of the StopWatch factory application. It owns every signal the
// views render; the state modules beside it hold the per-app logic in plain
// fields, and `frame` publishes their results once per frame.
import { createSignal } from "solid-js";
import { cos, f32, f64, frames, idiv, len, push, round, sin, type Color, type Deg, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as launcher from "./launcher";
import * as motion from "./motion";
import * as statusbar from "./statusbar";
import * as stopwatch from "./stopwatch";
import * as watch from "./watch";
import * as alarms from "./alarms";
import * as badge from "./badge";
import * as fft from "./fft";
import * as fluid from "./fluid";
import * as imu from "./imu";
import * as roller from "./roller";
import * as setup from "./setup";
import * as wheel from "./wheel";
import type { Sector } from "./wheel";

export type Page =
  | "boot"
  | "launcher"
  | "guide"
  | "fluid"
  | "alarm"
  | "watch"
  | "stopwatch"
  | "badge"
  | "imu"
  | "fft"
  | "wheel"
  | "setup";

export interface LapRow {
  id: i32;
  title: string;
  time: string;
  y: i32;
}

export interface AlarmRow {
  id: i32;
  time: string;
  enabled: boolean;
  y: i32;
}

export interface Bar {
  id: i32;
  angle: Deg;
  length: i32;
  level: i32;
}

export interface Dot {
  id: i32;
  x: i32;
  y: i32;
  level: i32;
}

export interface Tick {
  id: i32;
  x: i32;
  y: i32;
  angle: Deg;
  hour: boolean;
}

/** Launcher order: the factory installs these apps after the launcher. */
const APP_PAGES: Page[] = ["fluid", "alarm", "watch", "stopwatch", "badge", "imu", "fft", "wheel", "setup"];
const APP_TITLES: string[] = [
  "FluidBox",
  "AlarmClock",
  "WatchFace",
  "Stopwatch",
  "Badge",
  "IMU",
  "Audio.FFT",
  "LuckyWheel",
  "Settings",
];
const WEEKDAYS: string[] = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];

// Watch face themes (factory number_flow.cpp and simple.cpp).
const FLOW_BACKGROUND: Color[] = ["#000000", "#EEEEEE", "#C2EFEB", "#EEE0CB", "#B1CC74", "#8A89C0", "#F26419", "#F2D7EE", "#048A81", "#9DB4C0"];
const FLOW_PANEL: Color[] = ["#1A1919", "#E4E3E3", "#B1E1DD", "#D9CAB2", "#A4BD6B", "#8180B4", "#F6722C", "#E9C9E5", "#05968C", "#92A8B3"];
const FLOW_TIME: Color[] = ["#FFFFFF", "#484848", "#566E6C", "#6A5E4D", "#4B5E20", "#DFDEFF", "#FFEFE6", "#83627E", "#B1FFFA", "#ECF8FF"];
const FLOW_DATE: Color[] = ["#A0A0A0", "#525252", "#6A938F", "#948063", "#668323", "#CBCBFF", "#FFD3BC", "#A4789D", "#70CFC9", "#DCF3FF"];
// The simple face uses the same background, time and date colours; its
// second dot takes the date colour.

// --------------------------------- shell -----------------------------------

export const [page, setPage] = createSignal<Page>("boot");
export const [bridged, setBridged] = createSignal<boolean>(false);
export const [clockText, setClockText] = createSignal<string>("OO:OO");
export const [clock0, setClock0] = createSignal<string>("O");
export const [clock1, setClock1] = createSignal<string>("O");
export const [clock2, setClock2] = createSignal<string>(":");
export const [clock3, setClock3] = createSignal<string>("O");
export const [clock4, setClock4] = createSignal<string>("O");

// Status bar.
export const [statusShown, setStatusShown] = createSignal<boolean>(false);
export const [statusY, setStatusY] = createSignal<i32>(-85);
export const [batteryLevel, setBatteryLevel] = createSignal<i32>(80);
export const [batteryText, setBatteryText] = createSignal<string>("80%");
export const [charging, setCharging] = createSignal<boolean>(false);
export const [wifiOn, setWifiOn] = createSignal<boolean>(false);

// Launcher.
export const [iconOffsets, setIconOffsets] = createSignal<i32[]>([0, 466, 932, 1398, 1864, -1864, -1398, -932, -466]);
export const [appIndex, setAppIndex] = createSignal<i32>(0);
export const [appTitle, setAppTitle] = createSignal<string>("FluidBox");
export const [titleOpacity, setTitleOpacity] = createSignal<f32>(1.0);

// Watch faces.
export const [face, setFace] = createSignal<i32>(0);
export const [faceRotation, setFaceRotation] = createSignal<Deg>(0.0);
export const [flowOffsets, setFlowOffsets] = createSignal<i32[]>([66, 66, 66, 66, 66, 66]);
export const [anchorDX, setAnchorDX] = createSignal<i32[]>([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
export const [anchorDY, setAnchorDY] = createSignal<i32[]>([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
export const [classicMode, setClassicMode] = createSignal<i32>(0);
export const [hourAngle, setHourAngle] = createSignal<Deg>(0.0);
export const [minuteAngle, setMinuteAngle] = createSignal<Deg>(0.0);
export const [secondAngle, setSecondAngle] = createSignal<Deg>(0.0);
export const [watchTime, setWatchTime] = createSignal<string>("OO:OO");
export const [watchWeekday, setWatchWeekday] = createSignal<string>("SUN");
export const [watchDay, setWatchDay] = createSignal<string>("O1");
export const [watchDate, setWatchDate] = createSignal<string>("2026/1/1 THU");
export const [watchHH, setWatchHH] = createSignal<string>("00");
export const [watchMM, setWatchMM] = createSignal<string>("00");
export const [watchSS, setWatchSS] = createSignal<string>("00");
export const [plainTime, setPlainTime] = createSignal<string>("00:00");
export const [themeIndex, setThemeIndex] = createSignal<i32>(0);
export const [bigDigits, setBigDigits] = createSignal<i32[]>([0, 0, 0, 0]);
export const [faceBackground, setFaceBackground] = createSignal<Color>("#000000");
export const [facePanel, setFacePanel] = createSignal<Color>("#1A1919");
export const [simpleDot, setSimpleDot] = createSignal<boolean>(true);
export const [simpleTips, setSimpleTips] = createSignal<boolean>(true);
export const [dotX, setDotX] = createSignal<i32>(0);
export const [dotY, setDotY] = createSignal<i32>(-214);
export const [ticks, setTicks] = createSignal<Tick[]>([]);

// Stopwatch.
export const [swState, setSwState] = createSignal<i32>(0);
export const [swTime, setSwTime] = createSignal<string>("OO:OO:OO.OO");
export const [swLaps, setSwLaps] = createSignal<LapRow[]>([]);
export const [swLapScroll, setSwLapScroll] = createSignal<i32>(0);
export const [swLeftOffset, setSwLeftOffset] = createSignal<i32>(0);
export const [swRightOffset, setSwRightOffset] = createSignal<i32>(0);

// Rollers (three slots, five visible rows each).
export const [rollerRows, setRollerRows] = createSignal<string[]>(["", "", "", "", "", "", "", "", "", "", "", "", "", "", ""]);
export const [rollerShift, setRollerShift] = createSignal<i32[]>([0, 0, 0]);

// Alarm clock.
export const [alarmScreen, setAlarmScreen] = createSignal<i32>(0);
export const [alarmRows, setAlarmRows] = createSignal<AlarmRow[]>([]);
export const [alarmScroll, setAlarmScroll] = createSignal<i32>(0);
export const [alarmAddY, setAlarmAddY] = createSignal<i32>(88);
export const [alarmDialog, setAlarmDialog] = createSignal<boolean>(false);
export const [ringing, setRinging] = createSignal<boolean>(false);
export const [ringTime, setRingTime] = createSignal<string>("00:00");

// Settings.
export const [setupScreen, setSetupScreen] = createSignal<i32>(0);
export const [setupScroll, setSetupScroll] = createSignal<i32>(0);
export const [setupLevel, setSetupLevel] = createSignal<i32>(80);
export const [setupSummary, setSetupSummary] = createSignal<string>("");
export const [setupDateStage, setSetupDateStage] = createSignal<i32>(0);
export const [sfxOn, setSfxOn] = createSignal<boolean>(true);
export const [vibrateOn, setVibrateOn] = createSignal<boolean>(true);
export const [wifiStatus, setWifiStatus] = createSignal<string>("Off");
export const [aboutProgress, setAboutProgress] = createSignal<string>("0% Complete");

// Lucky wheel.
export const [wheelScreen, setWheelScreen] = createSignal<i32>(0);
export const [wheelRight, setWheelRight] = createSignal<Sector[]>([]);
export const [wheelLeft, setWheelLeft] = createSignal<Sector[]>([]);
export const [wheelLabels, setWheelLabels] = createSignal<Sector[]>([]);
export const [wheelPointer, setWheelPointer] = createSignal<Deg>(0.0);

// IMU.
export const [imuBallX, setImuBallX] = createSignal<i32>(0);
export const [imuBallY, setImuBallY] = createSignal<i32>(0);
export const [imuBallSize, setImuBallSize] = createSignal<i32>(80);
export const [imuOrbitX, setImuOrbitX] = createSignal<i32>(0);
export const [imuOrbitY, setImuOrbitY] = createSignal<i32>(-214);
export const [imuLabels, setImuLabels] = createSignal<boolean>(true);
export const [imuTextX, setImuTextX] = createSignal<string>("X: 0.0");
export const [imuTextY, setImuTextY] = createSignal<string>("Y: 0.0");
export const [imuTextZ, setImuTextZ] = createSignal<string>("Z: 0.0");

// Audio FFT.
export const [fftBars, setFftBars] = createSignal<Bar[]>([]);
export const [fftScale, setFftScale] = createSignal<f32>(1.0);
export const [fftLabels, setFftLabels] = createSignal<boolean>(true);
export const [fftHz, setFftHz] = createSignal<string>("0.0");

// Badge.
export const [badgeDialog, setBadgeDialog] = createSignal<boolean>(false);
export const [badgeEditing, setBadgeEditing] = createSignal<boolean>(false);
export const [badgeMessage, setBadgeMessage] = createSignal<string>("");

// FluidBox.
export const [fluidDots, setFluidDots] = createSignal<Dot[]>([]);

// ------------------------------ shell state --------------------------------

let firstLauncherOpen: boolean = true;
let statusBarDue: f64 = -1.0;
let lastChargeCheck: f64 = 0.0;
let wasCharging: boolean = false;
let lastClockUpdate: f64 = -10000.0;
let lastFrameMs: f64 = 0.0;
let lapScroll: i32 = 0;
let lapScrollOrigin: i32 = 0;
let lapDragging: boolean = false;

function two(value: i32): string {
  return value < 10 ? `0${value}` : `${value}`;
}

function updateClock(force: boolean): void {
  if (!force && device.uptimeMs - lastClockUpdate <= 1000.0) {
    return;
  }
  lastClockUpdate = device.uptimeMs;
  const text = stopwatch.letterO(`${two(device.hour)}:${two(device.minute)}`);
  setClockText(text);
  setClock0(stopwatch.letterO(`${idiv(device.hour, 10)}`));
  setClock1(stopwatch.letterO(`${device.hour % 10}`));
  setClock3(stopwatch.letterO(`${idiv(device.minute, 10)}`));
  setClock4(stopwatch.letterO(`${device.minute % 10}`));
}

function publishStatus(): void {
  setStatusShown(statusbar.visible());
  setStatusY(statusbar.y());
  setBatteryLevel(device.batteryLevel);
  setBatteryText(`${device.batteryLevel}%`);
  setCharging(device.charging);
  setWifiOn(device.wifiEnabled);
}

// --------------------------------- pages -----------------------------------

function openPage(next: Page): void {
  if (page() === "launcher") {
    statusbar.destroy();
  }
  roller.disableAll();
  if (next === "launcher") {
    openLauncher();
  } else if (next === "watch") {
    watch.open();
    publishWatch();
  } else if (next === "alarm") {
    alarms.open();
    publishAlarms();
  } else if (next === "setup") {
    setup.open();
    publishSetup();
  } else if (next === "wheel") {
    wheel.open();
    publishWheel();
  } else if (next === "imu") {
    imu.open();
    publishImu();
  } else if (next === "fft") {
    fft.open();
  } else if (next === "badge") {
    badge.open();
    publishBadge();
  } else if (next === "fluid") {
    fluid.open();
    publishFluid();
  } else if (next === "stopwatch") {
    lapScroll = 0;
    motion.springConfigure(motion.SPRING_STOPWATCH_LEFT, 0.3, 0.5);
    motion.springConfigure(motion.SPRING_STOPWATCH_RIGHT, 0.3, 0.5);
    motion.springTeleport(motion.SPRING_STOPWATCH_LEFT, 0.0);
    motion.springTeleport(motion.SPRING_STOPWATCH_RIGHT, 0.0);
    publishStopwatch();
  }
  setPage(next);
}

function openLauncher(): void {
  if (firstLauncherOpen) {
    // The first launcher open shows the status bar after 800 ms, visibly.
    statusBarDue = device.uptimeMs + 800.0;
  } else {
    statusbar.create(true);
  }
  lastChargeCheck = device.uptimeMs;
  wasCharging = device.charging;
  firstLauncherOpen = false;
}

/** Tap on the centre icon: open that app. */
export function openCurrentApp(): void {
  if (launcher.suppressTap || launcher.busy()) {
    return;
  }
  openPage(APP_PAGES[launcher.currentIndex()]);
}

export function launcherPrevious(): void {
  launcher.scrollBy(-1);
}

export function launcherNext(): void {
  launcher.scrollBy(1);
}

export function statusTap(): void {
  statusbar.hide();
}

function launcherFrame(): void {
  if (input.keyEvent === input.KEY_PREVIOUS) {
    launcher.scrollBy(-1);
  } else if (input.keyEvent === input.KEY_NEXT) {
    launcher.scrollBy(1);
  }
  launcher.update();
  if (statusBarDue >= 0.0 && device.uptimeMs >= statusBarDue) {
    statusBarDue = -1.0;
    statusbar.create(false);
  }
  if (device.uptimeMs - lastChargeCheck >= 1000.0) {
    lastChargeCheck = device.uptimeMs;
    if (device.charging && !wasCharging && statusbar.created && statusbar.hidden) {
      statusbar.show();
    }
    wasCharging = device.charging;
  }
  statusbar.update();
  const offsets: i32[] = [];
  for (let i = 0; i < launcher.APP_COUNT; i++) {
    push(offsets, launcher.iconOffset(i));
  }
  setIconOffsets(offsets);
  const index = launcher.currentIndex();
  setAppIndex(index);
  setAppTitle(APP_TITLES[index]);
  setTitleOpacity(f32(launcher.titleOpacity()) / 100.0);
}

function guideFrame(): void {
  if (input.keyEvent === input.KEY_HOME) {
    openPage("launcher");
  }
}

// Watch faces --------------------------------------------------------------

export function watchTap(): void {
  watch.tap();
  publishWatch();
}

// The factory turns the whole watch-face panel with the accelerometer. A
// turned subtree loses its rounded corners in the PocketJS rasterizer, so
// each element is turned on its own: anchors move around the screen centre
// and text, hands, ticks and images take the rotation; round shapes only move.
const ANCHOR_X: f32[] = [0.0, 0.0, 125.0, -117.0, 0.0, 117.0, 0.0, -42.0, 42.0, -42.0, 42.0, 0.0, 0.0, 0.0, -135.0, -99.0, -18.0, 18.0, 99.0, 135.0];
const ANCHOR_Y: f32[] = [104.0, 138.0, 0.0, -22.0, -22.0, -22.0, 76.0, -51.0, -51.0, 51.0, 51.0, -24.0, 57.0, 130.0, -22.0, -22.0, -22.0, -22.0, -22.0, -22.0];
let ticksRotation: f64 = 1000.0;

function buildTicks(rotation: f64): void {
  ticksRotation = rotation;
  const list: Tick[] = [];
  for (let i = 0; i < 60; i++) {
    const hourTick = i % 5 === 0;
    const length: f64 = hourTick ? 28.0 : 14.0;
    const middle: f64 = 224.0 - length / 2.0;
    const degrees: f64 = f64(i * 6 - 90) + rotation;
    const radians: f64 = degrees * 0.017453292519943295;
    push(list, {
      id: i,
      x: round(middle * cos(radians)),
      y: round(middle * sin(radians)),
      angle: f32(degrees) as Deg,
      hour: hourTick,
    });
  }
  setTicks(list);
}

function publishAnchors(rotation: f64): void {
  const radians = rotation * 0.017453292519943295;
  const c = cos(radians);
  const s = sin(radians);
  const dx: i32[] = [];
  const dy: i32[] = [];
  for (let k = 0; k < len(ANCHOR_X); k++) {
    const x = f64(ANCHOR_X[k]);
    const y = f64(ANCHOR_Y[k]);
    push(dx, round(c * x - s * y - x));
    push(dy, round(s * x + c * y - y));
  }
  setAnchorDX(dx);
  setAnchorDY(dy);
}

function publishWatch(): void {
  setFace(watch.face);
  setFaceRotation(f32(watch.rotation) as Deg);
  if (watch.rotation !== ticksRotation) {
    buildTicks(watch.rotation);
    publishAnchors(watch.rotation);
  }
  setClassicMode(watch.classicMode);
  setHourAngle(f32(watch.hourAngle() + watch.rotation) as Deg);
  setMinuteAngle(f32(watch.minuteAngle() + watch.rotation) as Deg);
  setSecondAngle(f32(watch.secondAngle() + watch.rotation) as Deg);
  setWatchTime(stopwatch.letterO(`${two(watch.shownHour)}:${two(watch.shownMinute)}`));
  setPlainTime(`${two(watch.shownHour)}:${two(watch.shownMinute)}`);
  setWatchWeekday(WEEKDAYS[device.weekday]);
  setWatchDay(stopwatch.letterO(two(device.day)));
  setWatchDate(`${device.year}/${device.month}/${device.day} ${WEEKDAYS[device.weekday]}`);
  setWatchHH(two(watch.shownHour));
  setWatchMM(two(watch.shownMinute));
  setWatchSS(two(watch.shownSecond));
  if (watch.face === watch.FACE_NUMBER_FLOW) {
    setThemeIndex(watch.flowTheme);
    setFaceBackground(FLOW_BACKGROUND[watch.flowTheme]);
    setFacePanel(FLOW_PANEL[watch.flowTheme]);
  } else if (watch.face === watch.FACE_SIMPLE) {
    setThemeIndex(watch.simpleTheme);
    setFaceBackground(FLOW_BACKGROUND[watch.simpleTheme]);
    setFacePanel(FLOW_DATE[watch.simpleTheme]);
  } else if (watch.face === watch.FACE_BIG_NUMBER) {
    setThemeIndex(watch.bigPalette);
  } else {
    setThemeIndex(0);
  }
  setBigDigits([idiv(watch.shownHour, 10), watch.shownHour % 10, idiv(watch.shownMinute, 10), watch.shownMinute % 10]);
  if (watch.face === watch.FACE_NUMBER_FLOW) {
    setFlowOffsets([
      watch.flowOffset(0),
      watch.flowOffset(1),
      watch.flowOffset(2),
      watch.flowOffset(3),
      watch.flowOffset(4),
      watch.flowOffset(5),
    ]);
  }
  setSimpleDot(watch.simpleDot);
  setSimpleTips(watch.simpleTips);
  setDotX(watch.dotX());
  setDotY(watch.dotY());
}

function watchFrame(): void {
  if (watch.update()) {
    openPage("launcher");
    return;
  }
  publishWatch();
}

// Stopwatch ----------------------------------------------------------------

export function stopwatchLeft(): void {
  stopwatch.left();
  publishStopwatch();
}

export function stopwatchRight(): void {
  stopwatch.right();
  publishStopwatch();
}

function publishStopwatch(): void {
  setSwState(stopwatch.state);
  setSwTime(stopwatch.format(stopwatch.elapsedMs()));
  const count = len(stopwatch.laps);
  if (len(swLaps()) !== count) {
    const rows: LapRow[] = [];
    for (let i = 0; i < count; i++) {
      push(rows, {
        id: i,
        title: `LAP ${i + 1}`,
        time: stopwatch.format(stopwatch.laps[i]),
        y: (count - 1 - i) * 48,
      });
    }
    setSwLaps(rows);
  }
  setSwLapScroll(lapScroll);
  setSwLeftOffset(round(motion.springPos[motion.SPRING_STOPWATCH_LEFT]));
  setSwRightOffset(round(motion.springPos[motion.SPRING_STOPWATCH_RIGHT]));
}

function stopwatchFrame(): void {
  if (input.keyEvent === input.KEY_HOME) {
    stopwatch.reset();
    openPage("launcher");
    return;
  }
  // Physical keys act on press and nudge their button 8 px inwards.
  if (input.keyAPressed) {
    stopwatch.left();
    motion.springTo(motion.SPRING_STOPWATCH_LEFT, 8.0);
  }
  if (input.keyBPressed) {
    stopwatch.right();
    motion.springTo(motion.SPRING_STOPWATCH_RIGHT, 8.0);
  }
  if (!device.keyA) {
    motion.springTo(motion.SPRING_STOPWATCH_LEFT, 0.0);
  }
  if (!device.keyB) {
    motion.springTo(motion.SPRING_STOPWATCH_RIGHT, 0.0);
  }
  // Lap list: vertical drag inside the 214 px laps panel (top 252 on screen).
  if (input.pointerPressed && input.pointerY >= 252) {
    lapDragging = true;
    lapScrollOrigin = lapScroll;
  }
  if (input.pointerDown && lapDragging) {
    const content = len(stopwatch.laps) * 48 + 24 + 74;
    const limit = content > 214 ? content - 214 : 0;
    const next = lapScrollOrigin - (input.pointerY - input.startY);
    lapScroll = next < 0 ? 0 : next > limit ? limit : next;
  }
  if (input.pointerReleased) {
    lapDragging = false;
  }
  publishStopwatch();
}


// Rollers -------------------------------------------------------------------

/** Row label of `value` for roller kind: 0 "07", 1 "2026", 2 "07" (1-based), 3 "7" (2-based). */
function rollerLabel(kind: i32, value: i32): string {
  if (value < 0) {
    return "";
  }
  if (kind === 1) {
    return `${2000 + value}`;
  }
  if (kind === 2) {
    return two(value + 1);
  }
  if (kind === 3) {
    return `${value + 2}`;
  }
  return two(value);
}

function publishRollers(k0: i32, k1: i32, k2: i32): void {
  const rows: string[] = [];
  const kinds: i32[] = [k0, k1, k2];
  for (let slot = 0; slot < 3; slot++) {
    for (let k = -2; k <= 2; k++) {
      push(rows, rollerLabel(kinds[slot], roller.rowAt(slot, k)));
    }
  }
  setRollerRows(rows);
  setRollerShift([roller.shift(0), roller.shift(1), roller.shift(2)]);
}

// Alarm clock --------------------------------------------------------------

function publishAlarms(): void {
  setAlarmScreen(alarms.screen);
  const rows: AlarmRow[] = [];
  for (let i = 0; i < alarms.count(); i++) {
    push(rows, {
      id: i,
      time: `${two(device.alarmHours[i])}:${two(device.alarmMinutes[i])}`,
      enabled: device.alarmEnabled[i],
      y: alarms.rowTop(i),
    });
  }
  setAlarmRows(rows);
  setAlarmScroll(alarms.scroll);
  setAlarmAddY(alarms.rowTop(alarms.count()));
  setAlarmDialog(alarms.deleteRow >= 0);
  if (alarms.screen === alarms.ADD) {
    publishRollers(0, 0, 0);
  }
}

export function alarmToggle(index: i32): void {
  alarms.toggle(index);
  publishAlarms();
}

export function alarmAdd(): void {
  alarms.openAdd();
  publishAlarms();
}

export function alarmConfirmAdd(): void {
  alarms.confirmAdd();
  publishAlarms();
}

export function alarmDelete(): void {
  alarms.confirmDelete();
  publishAlarms();
}

export function alarmCancel(): void {
  alarms.cancelDelete();
  publishAlarms();
}

export function alarmDismiss(): void {
  alarms.dismiss();
  setRinging(false);
}

function publishRinging(): void {
  setRinging(alarms.ringing >= 0);
  setRingTime(`${two(alarms.ringHour)}:${two(alarms.ringMinute)}`);
}

// Settings -----------------------------------------------------------------

function publishSetup(): void {
  setSetupScreen(setup.screen);
  setSetupScroll(setup.scroll);
  setSetupLevel(setup.level);
  setSetupDateStage(setup.dateStage);
  setSfxOn(device.buttonSfx);
  setVibrateOn(device.buttonVibrate);
  setWifiOn(device.wifiEnabled);
  setWifiStatus(setup.wifiStatus());
  setAboutProgress(`${setup.progress}% Complete`);
  if (setup.screen === setup.TIME) {
    setSetupSummary(setup.summary());
    publishRollers(0, 0, 0);
  } else if (setup.screen === setup.DATE) {
    setSetupSummary(setup.summary());
    publishRollers(1, 2, 2);
  }
}

export function setupOpen(item: i32): void {
  setup.openItem(item);
  publishSetup();
}

export function setupVersion(): void {
  setup.tapVersion();
  publishSetup();
}

export function setupConfirm(): void {
  setup.confirm();
  publishSetup();
}

export function setupToggleSfx(): void {
  setup.toggleSfx();
  publishSetup();
}

export function setupToggleVibrate(): void {
  setup.toggleVibrate();
  publishSetup();
}

export function setupToggleWifi(): void {
  setup.toggleWifi();
  publishSetup();
}

// Lucky wheel --------------------------------------------------------------

function publishWheel(): void {
  setWheelScreen(wheel.screen);
  setWheelPointer(f32(wheel.pointerAngle()) as Deg);
  if (wheel.screen === wheel.SELECT) {
    publishRollers(3, 0, 0);
  }
}

export function wheelConfirm(): void {
  wheel.confirm();
  setWheelRight(wheel.rightHalf);
  setWheelLeft(wheel.leftHalf);
  setWheelLabels(wheel.labels);
  publishWheel();
}

export function wheelTap(): void {
  wheel.spin(0);
}

// IMU ------------------------------------------------------------------------

function publishImu(): void {
  const size = round(imu.ballSize);
  setImuBallSize(size);
  setImuBallX(233 + round(imu.ballX) - idiv(size, 2));
  setImuBallY(233 + round(imu.ballY) - idiv(size, 2));
  setImuOrbitX(imu.orbitX());
  setImuOrbitY(imu.orbitY());
  setImuLabels(imu.labels);
  setImuTextX(imu.label("X", device.accelX));
  setImuTextY(imu.label("Y", device.accelY));
  setImuTextZ(imu.label("Z", device.accelZ));
}

export function imuTap(): void {
  imu.tap();
  publishImu();
}

// Audio FFT --------------------------------------------------------------------

function publishFft(): void {
  const bars: Bar[] = [];
  for (let i = 0; i < 20; i++) {
    const radius = fft.bars[i];
    const angle = f32(i * 9 + 90) + 4.5;
    push(bars, { id: i, angle: angle as Deg, length: round(radius), level: fft.level(radius) });
  }
  for (let i = 0; i < 20; i++) {
    const radius = fft.bars[i];
    const angle = f32(i * 9 + 90) + 4.5;
    push(bars, { id: i + 20, angle: (180.0 - angle) as Deg, length: round(radius), level: fft.level(radius) });
  }
  setFftBars(bars);
  setFftScale(f32(fft.discScale));
  setFftLabels(fft.labels);
  setFftHz(`${round(device.peakHz)}`);
}

export function fftTap(): void {
  fft.tap();
  setFftLabels(fft.labels);
}

// Badge ------------------------------------------------------------------------

function publishBadge(): void {
  setBadgeDialog(badge.dialog);
  setBadgeEditing(badge.editing);
  setBadgeMessage(badge.message());
}

export function badgeEdit(): void {
  badge.startEdit();
  publishBadge();
}

export function badgeCancel(): void {
  badge.cancel();
  publishBadge();
}

// FluidBox -----------------------------------------------------------------------

function publishFluid(): void {
  const dots: Dot[] = [];
  for (let i = 0; i < fluid.COUNT; i++) {
    push(dots, { id: i, x: round(fluid.px[i]) - 11, y: round(fluid.py[i]) - 11, level: fluid.speedLevel(i) });
  }
  setFluidDots(dots);
}

// --------------------------------- frame -----------------------------------

/** Key fallback while no bridge reports raw keys: ActionHandler clicks. */
export function keyAClick(): void {
  input.keyFeedback(true);
  if (page() === "stopwatch") {
    stopwatchLeft();
  } else {
    input.injectClick(input.KEY_PREVIOUS);
  }
}

export function keyBClick(): void {
  input.keyFeedback(false);
  if (page() === "stopwatch") {
    stopwatchRight();
  } else {
    input.injectClick(input.KEY_NEXT);
  }
}

function frame(): void {
  device.simulate(33.333);
  setBridged(device.bridged);
  const dt = device.uptimeMs - lastFrameMs;
  lastFrameMs = device.uptimeMs;
  input.updateKeys();
  input.updatePointer();
  const seconds = dt > 100.0 ? 0.1 : dt / 1000.0;
  motion.springStepAll(seconds);
  updateClock(false);
  if (alarms.ringing >= 0) {
    // The trigger screen blocks the app underneath until OK.
    alarms.check();
    publishRinging();
    publishStatus();
    return;
  }
  const current = page();
  if (current === "launcher") {
    launcherFrame();
  } else if (current === "guide") {
    guideFrame();
  } else if (current === "watch") {
    watchFrame();
  } else if (current === "stopwatch") {
    stopwatchFrame();
  } else if (current === "alarm") {
    if (alarms.update()) {
      openPage("launcher");
    } else {
      publishAlarms();
    }
  } else if (current === "setup") {
    if (setup.update()) {
      openPage("launcher");
    } else {
      publishSetup();
    }
  } else if (current === "wheel") {
    if (wheel.update()) {
      openPage("launcher");
    } else {
      publishWheel();
    }
  } else if (current === "imu") {
    if (imu.update(seconds)) {
      openPage("launcher");
    } else {
      publishImu();
    }
  } else if (current === "fft") {
    if (fft.update()) {
      openPage("launcher");
    } else {
      publishFft();
    }
  } else if (current === "badge") {
    if (badge.update()) {
      openPage("launcher");
    } else {
      publishBadge();
    }
  } else if (current === "fluid") {
    if (fluid.update(seconds)) {
      openPage("launcher");
    } else {
      publishFluid();
    }
  }
  if (current !== "boot") {
    alarms.check();
    publishRinging();
  }
  publishStatus();
}

export async function run(): Promise<void> {
  buildTicks(0.0);
  updateClock(true);
  for (let step: i32 = 0; step < 2147483647; step++) {
    await frames(1);
    if (step === 1) {
      // The board publishes its settings before the first frame; decide on
      // the home-gesture guide once they arrived (factory: first 5 boots).
      updateClock(true);
      device.playBootSfx();
      if (device.guideLaunchCount < 5) {
        device.countGuide();
        setPage("guide");
      } else {
        openPage("launcher");
      }
    }
    frame();
  }
}

// Host entry points for the JS-mode bridge (docs/APP-HOST.md).
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
