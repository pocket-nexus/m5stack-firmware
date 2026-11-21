// StickS3 startup menu model, after UIFlow2 m5stack/modules/startup/sticks3.py:
// the Framework key loop, LauncherApp, CloudApp, UsbApp and SetupApp.
import { createSignal } from "solid-js";
import { after, type f32, type f64, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import {
  accessCode,
  apSsid,
  apStations,
  batteryLevel,
  charging,
  CLOUD_CONNECTED,
  cloudStatus,
  firmwareVersion,
  nickName,
  portalClients,
  requestBrightness,
  requestSetup,
  requestTone,
  requestVolume,
  WIFI_CONNECTING,
  WIFI_GOT_IP,
  WIFI_IDLE,
  wifiConfigured,
  wifiStatus,
} from "./device";

export type Page = "launcher" | "cloud" | "usb" | "setup";

/** Launcher icons, in UIFlow order: CLOUD (1a), USB (1b), SETUP (1e). */
export type Mode = "cloud" | "usb" | "setup";

/** Launcher state dot (wifiNeverSet / wifiNG / wifiOKServerNG / wifiOKServerOK). */
export type LinkState = "never-set" | "wifi-ng" | "server-ng" | "server-ok";

/** SetupApp._STATE_*. */
export type SetupState = "ap" | "web" | "wait-wifi" | "wifi-ng" | "wifi-ok" | "server-ng" | "server-ok";

// The StickS3 startup starts the cloud app, not the launcher (fw.start(cloud_app)).
export const [page, setPage] = createSignal<Page>("cloud");
export const [mode, setMode] = createSignal<Mode>("cloud");
export const [setupState, setSetupState] = createSignal<SetupState>("ap");

export const [battery, setBattery] = createSignal<i32>(100);
export const [isCharging, setIsCharging] = createSignal<boolean>(false);
export const [link, setLink] = createSignal<LinkState>("never-set");
export const [wifiOk, setWifiOk] = createSignal<boolean>(false);
export const [serverOk, setServerOk] = createSignal<boolean>(false);
export const [code, setCode] = createSignal<string>("");
export const [nick, setNick] = createSignal<string>("");
export const [ssid, setSsid] = createSignal<string>("S3_0000");
export const [version, setVersion] = createSignal<string>("v0.1.0");

// M5Unified Button_Class: a click is decided when no further click follows
// within the hold threshold (500 ms); the count tells single from double.
const CLICK_WINDOW_MS: i32 = 500;
let clicksA: i32 = 0;
let clicksB: i32 = 0;

function linkState(): LinkState {
  if (!wifiConfigured) {
    return "never-set";
  }
  if (wifiStatus !== WIFI_GOT_IP) {
    return "wifi-ng";
  }
  if (cloudStatus !== CLOUD_CONNECTED) {
    return "server-ng";
  }
  return "server-ok";
}

/** Copies the host values the visible page shows (the apps' on_run loops). */
function refresh(): void {
  setBattery(batteryLevel);
  setIsCharging(charging);
  setLink(linkState());
  setWifiOk(wifiStatus === WIFI_GOT_IP);
  setServerOk(wifiStatus === WIFI_GOT_IP && cloudStatus === CLOUD_CONNECTED);
  setCode(accessCode);
  setNick(nickName);
  setSsid(apSsid);
  setVersion(firmwareVersion);
}

/** SetupApp.on_run, one 100 ms step. */
function stepSetup(): void {
  const state = setupState();
  if (state === "ap") {
    if (apStations > 0) {
      setSetupState("web");
    }
    return;
  }
  if (state === "web") {
    if (portalClients > 0) {
      setSetupState("wait-wifi");
    }
    return;
  }
  if (state === "wait-wifi") {
    if (wifiStatus === WIFI_IDLE || wifiStatus === WIFI_CONNECTING) {
      return;
    }
    setSetupState(wifiStatus === WIFI_GOT_IP ? "wifi-ok" : "wifi-ng");
    return;
  }
  if (state === "wifi-ng") {
    if (wifiStatus === WIFI_GOT_IP) {
      setSetupState("wifi-ok");
    }
    return;
  }
  if (state === "wifi-ok") {
    if (cloudStatus === CLOUD_CONNECTED && accessCode !== "") {
      setSetupState("server-ok");
    } else if (cloudStatus < 0 || cloudStatus > CLOUD_CONNECTED) {
      setSetupState("server-ng");
    }
    return;
  }
  if (state === "server-ng") {
    // UIFlow retries only when an M5Things client exists.
    if (cloudStatus !== -1) {
      setSetupState("wifi-ok");
    }
  }
}

/** Runs for the lifetime of the app: the on_run loops of the visible app. */
export async function run(): Promise<void> {
  for (let step: i32 = 0; step < 2147483647; step++) {
    if (page() === "setup") {
      stepSetup();
    }
    refresh();
    await after(100);
  }
}

export function start(): void {
  // StickS3_Startup.startup and _prepare_board: brightness 90 %, volume 100,
  // Speaker.tone(4000, 50) before the menu starts.
  requestBrightness(230);
  requestVolume(100);
  requestTone(4000, 50);
  refresh();
  run();
}

function openLauncher(): void {
  if (page() === "setup") {
    requestSetup(false);
  }
  // LauncherApp.on_launch restarts the icon cycle at CLOUD.
  setMode("cloud");
  setPage("launcher");
  refresh();
}

function openMode(): void {
  const selected = mode();
  if (selected === "setup") {
    setSetupState("ap");
    requestSetup(true);
    setPage("setup");
  } else if (selected === "usb") {
    setPage("usb");
  } else {
    setPage("cloud");
  }
  refresh();
}

function nextMode(): void {
  const current = mode();
  setMode(current === "cloud" ? "usb" : current === "usb" ? "setup" : "cloud");
  refresh();
}

/** BtnA single click: Framework plays 4000 Hz, then the app's enter handler. */
function enterKey(): void {
  requestTone(4000, 50);
  if (page() === "launcher") {
    openMode();
  } else {
    openLauncher();
  }
}

/** BtnA double click: 3500 Hz, then the app's back handler. */
function backKey(): void {
  requestTone(3500, 50);
  if (page() !== "launcher") {
    openLauncher();
  }
}

/** BtnB single click: 6000 Hz, then the app's dpad-down handler. */
function downKey(): void {
  requestTone(6000, 50);
  if (page() === "launcher") {
    nextMode();
  }
}

async function decideA(): Promise<void> {
  await after(CLICK_WINDOW_MS);
  const count = clicksA;
  clicksA = 0;
  if (count === 1) {
    enterKey();
  } else if (count === 2) {
    backKey();
  }
}

async function decideB(): Promise<void> {
  await after(CLICK_WINDOW_MS);
  const count = clicksB;
  clicksB = 0;
  if (count === 1) {
    downKey();
  }
}

/** KEY1 (front) press. A new press restarts the decision window. */
export function pressA(): void {
  clicksA = clicksA + 1;
  decideA();
}

/** KEY2 (side) press. */
export function pressB(): void {
  clicksB = clicksB + 1;
  decideB();
}

// Root exports that main.tsx (JS mode) uses to reach device.ts.
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
