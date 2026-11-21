// Root model of the StopWatch factory application. It owns every signal the
// views render; the state modules beside it hold the per-app logic in plain
// fields, and `frame` publishes their results once per frame.
import { createSignal } from "solid-js";
import { cos, f32, frames, idiv, len, round, sin, type Color, type Deg, type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import * as launcher from "./launcher";
import * as motion from "./motion";
import * as statusbar from "./statusbar";
import * as stopwatch from "./stopwatch";
import * as watch from "./watch";

export type Page =
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

export const [page, setPage] = createSignal<Page>("launcher");
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
  if (next === "launcher") {
    openLauncher();
  } else if (next === "watch") {
    watch.open();
    publishWatch();
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
    offsets[i] = launcher.iconOffset(i);
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

function buildTicks(): void {
  const list: Tick[] = [];
  for (let i = 0; i < 60; i++) {
    const hourTick = i % 5 === 0;
    const length: f32 = hourTick ? 28.0 : 14.0;
    const middle: f32 = 224.0 - length / 2.0;
    const radians: f32 = (f32(i * 6) - 90.0) * 0.017453292;
    list[i] = {
      id: i,
      x: round(middle * cos(radians)),
      y: round(middle * sin(radians)),
      angle: f32(i * 6 - 90) as Deg,
      hour: hourTick,
    };
  }
  setTicks(list);
}

function publishWatch(): void {
  setFace(watch.face);
  setFaceRotation(f32(watch.rotation) as Deg);
  setClassicMode(watch.classicMode);
  setHourAngle(f32(watch.hourAngle()) as Deg);
  setMinuteAngle(f32(watch.minuteAngle()) as Deg);
  setSecondAngle(f32(watch.secondAngle()) as Deg);
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
      rows[i] = {
        id: i,
        title: `LAP ${i + 1}`,
        time: stopwatch.format(stopwatch.laps[i]),
        y: (count - 1 - i) * 48,
      };
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
  motion.springStepAll(dt > 100.0 ? 0.1 : dt / 1000.0);
  updateClock(false);
  const current = page();
  if (current === "launcher") {
    launcherFrame();
  } else if (current === "guide") {
    guideFrame();
  } else if (current === "watch") {
    watchFrame();
  } else if (current === "stopwatch") {
    stopwatchFrame();
  } else if (input.keyEvent === input.KEY_HOME) {
    openPage("launcher");
  }
  publishStatus();
}

export async function run(): Promise<void> {
  buildTicks();
  updateClock(true);
  if (device.guideLaunchCount < 5) {
    device.countGuide();
    device.playBootSfx();
    setPage("guide");
    firstLauncherOpen = true;
  } else {
    device.playBootSfx();
    openLauncher();
  }
  for (let step: i32 = 0; step < 2147483647; step++) {
    await frames(1);
    frame();
  }
}
