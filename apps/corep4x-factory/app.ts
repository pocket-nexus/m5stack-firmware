// The CoreP4X factory demo model: the three swipe screens, the function
// windows with their open and close motion, the dialogs, and each window's
// state. The views read the signals below; the window modules hold plain
// state and the root model republishes it after every frame. Hardware goes
// through ./device only.
import { createSignal } from "solid-js";
import { f32, frames, i32, idiv, len, push, removeAt, type Color, type f64, type Px } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as dialog from "./dialog";
import type { PlacedButton } from "./dialog";
import * as frame from "./frame";
import * as i2c from "./i2c";
import * as imu from "./imu";
import * as ir from "./ir";
import * as music from "./music";
import * as net from "./net";
import type { Target } from "./net";
import type { Bar } from "./music";
import type { Bubble } from "./i2c";
import type { Dash } from "./frame";
import * as nav from "./nav";
import * as off from "./off";
import * as power from "./power";
import type { Pin } from "./power";
import * as rec from "./rec";
import * as sd from "./sd";
import type { Row } from "./sd";
import * as sleep from "./sleep";
import * as timer from "./timer";
import * as touch from "./touch";
import * as windows from "./windows";
import { dots, wrap } from "./wrap";

const PX: Px = 1;

// ---- screens -------------------------------------------------------------------

export const [startupOn, setStartupOn] = createSignal<boolean>(true);
export const [infoOn, setInfoOn] = createSignal<boolean>(false);
export const [functionOn, setFunctionOn] = createSignal<boolean>(false);
export const [startupX, setStartupX] = createSignal<Px>(0);
export const [infoX, setInfoX] = createSignal<Px>(0);
export const [functionX, setFunctionX] = createSignal<Px>(0);

function publishScreens(): void {
  setStartupOn(nav.visible(nav.STARTUP));
  setInfoOn(nav.visible(nav.INFO));
  setFunctionOn(nav.visible(nav.FUNCTION));
  setStartupX(nav.offset(nav.STARTUP) * PX);
  setInfoX(nav.offset(nav.INFO) * PX);
  setFunctionX(nav.offset(nav.FUNCTION) * PX);
}

// ---- windows -------------------------------------------------------------------

export const [win, setWin] = createSignal<i32>(-1);
export const [winLeft, setWinLeft] = createSignal<Px>(0);
export const [winTop, setWinTop] = createSignal<Px>(0);
export const [winWidth, setWinWidth] = createSignal<Px>(480);
export const [winHeight, setWinHeight] = createSignal<Px>(480);
export const [winOpacity, setWinOpacity] = createSignal<f32>(1);
export const [winPages, setWinPages] = createSignal<i32>(0);
export const [winTabs, setWinTabs] = createSignal<string[]>([]);
export const [winTab, setWinTab] = createSignal<i32>(0);
export const [winOutline, setWinOutline] = createSignal<Dash[]>([]);
export const [winTabEdges, setWinTabEdges] = createSignal<Dash[]>([]);
export const [winPlaceholder, setWinPlaceholder] = createSignal<Dash[]>([]);
export const [actionShown, setActionShown] = createSignal<boolean>(false);
export const [actionPlaceholder, setActionPlaceholder] = createSignal<boolean>(true);
export const [actionText, setActionText] = createSignal<string>("");
export const [actionIcon, setActionIcon] = createSignal<boolean>(false);
export const [actionBg, setActionBg] = createSignal<Color>("#2563EB");
export const [actionFg, setActionFg] = createSignal<Color>("#FFFFFF");
export const [actionEnabled, setActionEnabled] = createSignal<boolean>(true);

let tab: i32 = 0;
let lastOpen: i32 = -1;

function tabNames(id: i32): string[] {
  if (id === windows.POWER) {
    return ["Peripheral", "System", "M5BUS"];
  }
  return [];
}

const ACTION_TEXT: string[] = ["", "Canvas", "Test", "Clean", "", "Set", "Ping", "NTP", "Info", "Check", "Reinit", "", "", ""];

/** The header's left slot: an action button or the dashed placeholder. */
function publishAction(id: i32): void {
  setActionPlaceholder(id === windows.POWER || id === windows.SLEEP || id === windows.OFF || id === windows.MUSIC);
  setActionShown(id !== windows.TIMER && id !== windows.WIFI);
  setActionEnabled(true);
  setActionBg(id === windows.TIMER ? "#7C3AED" : id === windows.IMU ? "#F3F4F6" : "#2563EB");
  setActionFg(id === windows.IMU ? "#111827" : "#FFFFFF");
  setActionText(ACTION_TEXT[id]);
  setActionIcon(id === windows.IMU);
}

function publishWindow(): void {
  const id = windows.open;
  if (id !== lastOpen) {
    lastOpen = id;
    if (id >= 0) {
      const pages = windows.PAGES[id];
      tab = 0;
      setWinPages(pages);
      setWinTabs(tabNames(id));
      setWinTab(0);
      setWinOutline(frame.outline(pages));
      setWinTabEdges(pages > 0 ? frame.tabEdges(pages, 0) : []);
      publishAction(id);
    }
    setWin(id);
  }
  setWinLeft(windows.left * PX);
  setWinTop(windows.top * PX);
  setWinWidth(windows.width * PX);
  setWinHeight(windows.height * PX);
  setWinOpacity(windows.opacity);
}

export function pressArea(id: i32): void {
  if (nav.gestured || nav.moving() || windows.open >= 0) {
    return;
  }
  windows.openWindow(id);
  if (id === windows.OFF) {
    off.open();
  } else if (id === windows.SLEEP) {
    sleep.open();
  } else if (id === windows.POWER) {
    power.resetBus();
    gpioConfigsSeen = device.gpioConfigs;
    device.request("powerPage 1");
  } else if (id === windows.TIMER) {
    timer.open();
    device.request("timerPage 1");
  } else if (id === windows.I2C) {
    i2c.open();
  } else if (id === windows.SD) {
    sd.open();
    sdTestsSeen = device.sdTests;
  } else if (id === windows.TOUCH) {
    touch.open();
  } else if (id === windows.IMU) {
    imu.open();
  } else if (id === windows.REC) {
    rec.open();
  } else if (id === windows.MUSIC) {
    music.open();
  } else if (id === windows.PING) {
    net.openPing();
  } else if (id === windows.USB) {
    net.openUsb();
  } else if (id === windows.IR) {
    ir.open();
  }
  publishWindow();
  publishAll();
}

function closing(id: i32): void {
  if (id === windows.OFF) {
    off.close();
  } else if (id === windows.POWER) {
    power.close();
    device.request("powerPage 0");
  } else if (id === windows.TIMER) {
    device.request("timerPage 0");
  } else if (id === windows.TOUCH) {
    touch.close();
  } else if (id === windows.IMU) {
    imu.close();
  } else if (id === windows.REC) {
    rec.close();
  } else if (id === windows.MUSIC) {
    music.close();
  } else if (id === windows.PING) {
    net.closePing();
  } else if (id === windows.IR) {
    ir.close();
  }
}

/** Deletes the open window at once (no close animation). */
function dropWindow(): void {
  const id = windows.open;
  windows.dropWindow();
  closing(id);
}

export function exitWindow(): void {
  if (windows.open === windows.IMU) {
    imu.exit();
  }
  windows.closeWindow();
}

export function windowAction(): void {
  const id = windows.open;
  if (id === windows.I2C) {
    i2c.clean();
  } else if (id === windows.SD) {
    sd.test();
  } else if (id === windows.TIMER) {
    timer.ntp();
  } else if (id === windows.TOUCH) {
    touch.showCanvas();
  } else if (id === windows.IMU) {
    imu.recenter();
  } else if (id === windows.REC) {
    rec.openSet();
  } else if (id === windows.PING) {
    net.ping();
  } else if (id === windows.USB) {
    net.check();
  } else if (id === windows.IR) {
    ir.reinit();
  }
  publishAll();
}

export function selectTab(index: i32): void {
  const pages = windows.PAGES[windows.open];
  if (pages === 0 || index === tab) {
    return;
  }
  tab = index;
  setWinTab(index);
  setWinTabEdges(frame.tabEdges(pages, index));
}

// ---- dialogs -------------------------------------------------------------------

export const [dialogOn, setDialogOn] = createSignal<boolean>(false);
export const [dialogTitle, setDialogTitle] = createSignal<string>("");
export const [dialogText, setDialogText] = createSignal<string>("");
export const [dialogHeight, setDialogHeight] = createSignal<i32>(248);
export const [dialogTop, setDialogTop] = createSignal<i32>(116);
export const [dialogButtons, setDialogButtons] = createSignal<PlacedButton[]>([]);

let dialogRevision: i32 = -1;

/** The footer: 456 px inside the border, padding 5, buttons 116 px with
 *  10 px gaps, LV_FLEX_ALIGN_SPACE_EVENLY over the free width. */
function placeButtons(): PlacedButton[] {
  let visible: i32 = 0;
  for (let i: i32 = 0; i < len(dialog.buttons); i++) {
    if (!dialog.buttons[i].hidden) {
      visible = visible + 1;
    }
  }
  const placed: PlacedButton[] = [];
  const free: i32 = 446 - visible * 116 - (visible - 1) * 10;
  const gap: i32 = visible > 0 ? i32(f32(free) / f32(visible + 1)) : 0;
  let x: i32 = 5 + gap;
  for (let i: i32 = 0; i < len(dialog.buttons); i++) {
    const b = dialog.buttons[i];
    if (!b.hidden) {
      push(placed, { id: b.id, x: x, text: b.text, bg: b.bg, fg: b.fg });
      x = x + 116 + 10 + gap;
    }
  }
  return placed;
}

function publishDialog(): void {
  if (dialog.revision === dialogRevision) {
    return;
  }
  dialogRevision = dialog.revision;
  setDialogOn(dialog.open);
  setDialogTitle(dialog.title);
  setDialogText(wrap(dialog.text, 420, 16));
  setDialogHeight(dialog.height(dialog.size));
  setDialogTop(idiv(480 - dialog.height(dialog.size), 2));
  setDialogButtons(placeButtons());
}

export function dialogButton(index: i32): void {
  if (dialog.owner === power.OWNER) {
    power.dialogButton(index);
  } else {
    dialog.close();
  }
  publishDialog();
  publishAll();
}

// ---- OFF -------------------------------------------------------------------------

export const [offStatus, setOffStatus] = createSignal<string>("");
export const [offVersion, setOffVersion] = createSignal<string>("");
export const [offAction, setOffAction] = createSignal<string>("Ready.");
export const [offActionColor, setOffActionColor] = createSignal<Color>("#64748B");
export const [offSoundOn, setOffSoundOn] = createSignal<boolean>(true);
export const [offPowerText, setOffPowerText] = createSignal<string>("Power Off");
export const [offPowerBg, setOffPowerBg] = createSignal<Color>("#DC2626");
export const [offPowerEnabled, setOffPowerEnabled] = createSignal<boolean>(true);
export const [offRestartText, setOffRestartText] = createSignal<string>("ESP Restart");
export const [offRestartBg, setOffRestartBg] = createSignal<Color>("#D97706");
export const [offRestartEnabled, setOffRestartEnabled] = createSignal<boolean>(true);
export const [offBar, setOffBar] = createSignal<Px>(0);
export const [offBarColor, setOffBarColor] = createSignal<Color>("#E5E7EB");

function toneColor(tone: i32): Color {
  return tone === 1 ? "#16A34A" : tone === 2 ? "#D97706" : tone === 3 ? "#DC2626" : "#64748B";
}

function publishOff(): void {
  setOffStatus(wrap(off.statusText(), 278, 14));
  setOffVersion(device.firmwareVersion);
  setOffAction(wrap(off.action, 278, 14));
  setOffActionColor(toneColor(off.actionTone));
  setOffSoundOn(device.bootSound);
  const countingOff = off.running && off.pending === off.POWER_OFF;
  const countingRestart = off.running && off.pending === off.RESTART;
  setOffPowerText(countingOff ? "Cancel" : "Power Off");
  setOffPowerBg(countingOff ? "#4B5563" : "#DC2626");
  setOffPowerEnabled(off.running ? countingOff : device.pm1Ready);
  setOffRestartText(countingRestart ? "Cancel" : "ESP Restart");
  setOffRestartBg(countingRestart ? "#4B5563" : "#D97706");
  setOffRestartEnabled(off.running ? countingRestart : true);
  setOffBar(f32(off.running ? off.bar : 0) * f32(4.02) * PX);
  setOffBarColor(off.running ? (off.pending === off.RESTART || (!countingOff && off.bar > 0 && off.action === "Restart in 3 s...") ? "#D97706" : "#DC2626") : "#E5E7EB");
}

export function offSound(): void {
  off.toggleBootSound();
  publishOff();
}

export function offPower(): void {
  off.pressPowerOff();
  publishOff();
}

export function offRestart(): void {
  off.pressRestart();
  publishOff();
}

// ---- SLEEP -----------------------------------------------------------------------

export const [sleepStatus, setSleepStatus] = createSignal<string>("");
export const [sleepResult, setSleepResult] = createSignal<string>("");
export const [sleepResultColor, setSleepResultColor] = createSignal<Color>("#64748B");
export const [sleepButton, setSleepButton] = createSignal<boolean>(true);
export const [sleepRtc, setSleepRtc] = createSignal<boolean>(false);
export const [sleepSeconds, setSleepSeconds] = createSignal<i32>(60);
export const [sleepTouch, setSleepTouch] = createSignal<boolean>(false);
export const [sleepImu, setSleepImu] = createSignal<boolean>(false);
export const [sleepEnabled, setSleepEnabled] = createSignal<boolean>(true);

function publishSleep(): void {
  setSleepStatus(wrap(sleep.statusText(), 290, 16));
  setSleepResult(wrap(sleep.result, 290, 16));
  setSleepResultColor(toneColor(sleep.resultTone));
  setSleepButton(sleep.buttonSleep);
  setSleepRtc(sleep.rtcWake);
  setSleepSeconds(sleep.rtcSeconds);
  setSleepTouch(sleep.touchWake);
  setSleepImu(sleep.imuWake);
  setSleepEnabled(!sleep.running);
}

export function sleepToggleButton(): void {
  sleep.toggleButtonSleep();
  publishSleep();
}
export function sleepToggleRtc(): void {
  sleep.toggleRtc();
  publishSleep();
}
export function sleepSetSeconds(seconds: i32): void {
  sleep.setSeconds(seconds);
  publishSleep();
}
export function sleepToggleTouch(): void {
  sleep.toggleTouch();
  publishSleep();
}
export function sleepToggleImu(): void {
  sleep.toggleImu();
  publishSleep();
}
export function sleepDeep(): void {
  sleep.deepSleep();
  publishSleep();
}

// ---- POWER -----------------------------------------------------------------------

export const [powerRails, setPowerRails] = createSignal<boolean[]>([]);
export const [powerBacklight, setPowerBacklight] = createSignal<i32>(100);
export const [powerKnob, setPowerKnob] = createSignal<Px>(0);
export const [powerIndicator, setPowerIndicator] = createSignal<Px>(0);
export const [powerVbat, setPowerVbat] = createSignal<string>("-.--V");
export const [powerCharge, setPowerCharge] = createSignal<i32>(1);
export const [powerPins, setPowerPins] = createSignal<Pin[]>([]);
export const [gpioOpen, setGpioOpen] = createSignal<boolean>(false);
export const [gpioTitle, setGpioTitle] = createSignal<string>("");
export const [gpioStatus, setGpioStatus] = createSignal<string>("");
export const [gpioOutput, setGpioOutput] = createSignal<boolean>(false);
export const [gpioHigh, setGpioHigh] = createSignal<boolean>(false);
export const [gpioOpenDrain, setGpioOpenDrain] = createSignal<boolean>(true);
export const [gpioPull, setGpioPull] = createSignal<i32>(0);

let gpioConfigsSeen: i32 = 0;

function publishPower(): void {
  setPowerRails(device.rails);
  setPowerBacklight(device.backlight);
  const knob = power.knobX(device.backlight);
  setPowerKnob((knob - f32(16)) * PX);
  setPowerIndicator((knob - f32(power.SLIDER_LEFT) + f32(10)) * PX);
  setPowerVbat(power.vbatText());
  setPowerCharge(device.charge);
  if (tab === 2) {
    setPowerPins(power.pins());
  }
  setGpioOpen(power.popupPin > 0);
  if (power.popupPin > 0) {
    setGpioTitle(power.popupTitle());
    setGpioStatus(wrap(power.popupStatus(), 446, 16));
    setGpioOutput(power.draftOutput);
    setGpioHigh(power.draftLevel);
    setGpioOpenDrain(power.draftOpenDrain);
    setGpioPull(power.draftPull);
  }
}

export function powerToggle(rail: i32): void {
  power.toggle(rail);
  publishPower();
  publishDialog();
}
export function powerSetCharge(value: i32): void {
  power.setCharge(value);
  publishPower();
}
export function powerPin(pin: i32): void {
  power.openPopup(pin);
  publishPower();
}
export function gpioSetMode(output: boolean): void {
  power.setDraftOutput(output);
  publishPower();
}
export function gpioSetLevel(high: boolean): void {
  power.setDraftLevel(high);
  publishPower();
}
export function gpioSetDrive(openDrain: boolean): void {
  power.setDraftOpenDrain(openDrain);
  publishPower();
}
export function gpioSetPull(pull: i32): void {
  power.setDraftPull(pull);
  publishPower();
}
export function gpioApply(): void {
  power.apply();
  if (!device.hostAttached) {
    power.applied("");
  }
  publishPower();
}
export function gpioClose(): void {
  power.closePopup();
  publishPower();
}

// ---- TIMER -----------------------------------------------------------------------

export const [timerValues, setTimerValues] = createSignal<string[]>(["2026", "01", "01", "00", "00", "00"]);
export const [timerEnabled, setTimerEnabled] = createSignal<boolean>(true);

function publishTimer(): void {
  setTimerValues([timer.text(0), timer.text(1), timer.text(2), timer.text(3), timer.text(4), timer.text(5)]);
  setTimerEnabled(device.rtcReady);
  setActionShown(device.ntpReady);
}

export function timerSave(): void {
  timer.save();
  publishTimer();
}

// ---- I2C -------------------------------------------------------------------------

export const [i2cSummary, setI2cSummary] = createSignal<string>("");
export const [i2cSummaryColor, setI2cSummaryColor] = createSignal<Color>("#64748B");
export const [i2cBubbles, setI2cBubbles] = createSignal<Bubble[]>([]);

function publishI2c(): void {
  setI2cSummary(i2c.summary);
  setI2cSummaryColor(i2c.summaryError ? "#DC2626" : "#64748B");
  setI2cBubbles(i2c.bubbles);
}

export function i2cScan(bus: i32): void {
  i2c.scan(bus);
  if (!device.hostAttached) {
    i2c.scanned(0);
  }
  publishI2c();
}

// ---- SD --------------------------------------------------------------------------

export const [sdPath, setSdPath] = createSignal<string>("/sdcard");
export const [sdRows, setSdRows] = createSignal<Row[]>([]);
export const [sdBack, setSdBack] = createSignal<boolean>(false);
export const [sdSize, setSdSize] = createSignal<string>("");
export const [sdStatusText, setSdStatusText] = createSignal<string>("");

let sdTestsSeen: i32 = 0;
let sdTestTextSeen: string = "";

function publishSd(): void {
  setSdPath(sd.pathText());
  setSdRows(sd.rows());
  setSdBack(sd.backEnabled());
  setSdSize(sd.sizeText());
  setSdStatusText(device.sdStatus);
  setActionEnabled(!sd.testRunning);
}

export function sdRow(index: i32): void {
  sd.tapRow(index);
  publishSd();
}

export function sdGoBack(): void {
  sd.back();
  publishSd();
}

// ---- TOUCH -----------------------------------------------------------------------

export const [touchCanvasOn, setTouchCanvasOn] = createSignal<boolean>(false);
export const [touchTextureRow, setTouchTextureRow] = createSignal<i32[]>([0]);
export const [touchPill, setTouchPill] = createSignal<string>("0/25");
export const [touchPillDarkOn, setTouchPillDarkOn] = createSignal<boolean>(false);
export const [touchComplete, setTouchComplete] = createSignal<boolean>(false);
export const [touchStatusText, setTouchStatusText] = createSignal<string>("");
export const [touchIndicatorOn, setTouchIndicatorOn] = createSignal<boolean>(false);

let touchTextureSeen: i32 = -2;

function publishTouch(): void {
  setTouchCanvasOn(windows.open === windows.TOUCH && touch.canvasShown);
  if (device.touchTexture !== touchTextureSeen) {
    touchTextureSeen = device.touchTexture;
    setTouchTextureRow([device.touchTexture]);
  }
  setTouchPill(touch.pillText());
  setTouchPillDarkOn(device.touchPillDark);
  setTouchComplete(touch.complete());
  setTouchStatusText(device.touchStatus);
  setTouchIndicatorOn(device.touchIndicator);
}

export function touchInfo(): void {
  touch.showInfo();
  publishTouch();
}
export function touchClean(): void {
  touch.clean();
  publishTouch();
}
export function touchExit(): void {
  dropWindow();
  publishWindow();
  publishTouch();
}
export function touchToggleIndicator(): void {
  touch.toggleIndicator();
  publishTouch();
}

// ---- IMU -------------------------------------------------------------------------

export const [imuHidden, setImuHidden] = createSignal<boolean>(false);
export const [imuTextureRow, setImuTextureRow] = createSignal<i32[]>([0]);
export const [imuOverlay, setImuOverlay] = createSignal<boolean>(true);
export const [imuOverlayText, setImuOverlayText] = createSignal<string>("");
export const [imuOverlayError, setImuOverlayError] = createSignal<boolean>(false);
export const [imuMarkerCount, setImuMarkerCount] = createSignal<i32>(0);
export const [imuMarker0X, setImuMarker0X] = createSignal<Px>(0);
export const [imuMarker0Y, setImuMarker0Y] = createSignal<Px>(0);
export const [imuMarker1X, setImuMarker1X] = createSignal<Px>(0);
export const [imuMarker1Y, setImuMarker1Y] = createSignal<Px>(0);

let imuTextureSeen: i32 = -2;

function publishImu(): void {
  setImuHidden(imu.hidden);
  if (device.imuTexture !== imuTextureSeen) {
    imuTextureSeen = device.imuTexture;
    setImuTextureRow([device.imuTexture]);
  }
  setImuOverlay(imu.overlayShown());
  setImuOverlayText(imu.overlayText());
  setImuOverlayError(imu.overlayError());
  setImuMarkerCount(imu.hidden ? 0 : device.imuMarkers);
  setImuMarker0X(f32(device.imuMarker0X) * PX);
  setImuMarker0Y(f32(device.imuMarker0Y) * PX);
  setImuMarker1X(f32(device.imuMarker1X) * PX);
  setImuMarker1Y(f32(device.imuMarker1Y) * PX);
}

// ---- REC -------------------------------------------------------------------------

export const [recTextureRow, setRecTextureRow] = createSignal<i32[]>([0]);
export const [recSpectrum, setRecSpectrum] = createSignal<boolean>(false);
export const [recShowRef, setRecShowRef] = createSignal<boolean>(false);
export const [recDb, setRecDb] = createSignal<string[]>(["-inf", "-inf", "-inf"]);
export const [recText, setRecText] = createSignal<string>("REC");
export const [recBg, setRecBg] = createSignal<Color>("#2563EB");
export const [recFg, setRecFg] = createSignal<Color>("#FFFFFF");
export const [recEnabled, setRecEnabled] = createSignal<boolean>(true);
export const [recPlayText, setRecPlayText] = createSignal<string>("PLAY LAST");
export const [recPlayBg, setRecPlayBg] = createSignal<Color>("#F3F4F6");
export const [recPlayFg, setRecPlayFg] = createSignal<Color>("#111827");
export const [recPlayEnabled, setRecPlayEnabled] = createSignal<boolean>(false);
export const [recSetOn, setRecSetOn] = createSignal<boolean>(false);
export const [recSetText, setRecSetText] = createSignal<string>("");

let recTextureSeen: i32 = -2;
let recSpectrumSeen: boolean = false;

function publishRec(): void {
  if (device.recTexture !== recTextureSeen) {
    recTextureSeen = device.recTexture;
    setRecTextureRow([device.recTexture]);
  }
  if (device.recSpectrum !== recSpectrumSeen) {
    recSpectrumSeen = device.recSpectrum;
    rec.boardView(device.recSpectrum);
  }
  setRecSpectrum(rec.spectrum);
  setRecShowRef(rec.showRef);
  setRecDb([device.recDb0, device.recDb1, device.recDb2]);
  const b = rec.buttons();
  setRecText(b.recText);
  setRecBg(b.recBg);
  setRecFg(b.recFg);
  setRecEnabled(b.recEnabled);
  setRecPlayText(b.playText);
  setRecPlayBg(b.playBg);
  setRecPlayFg(b.playFg);
  setRecPlayEnabled(b.playEnabled);
  setRecSetOn(rec.setOpen && windows.open === windows.REC);
  setRecSetText(rec.setText());
}

export function recPressRec(): void {
  rec.pressRec();
  publishRec();
}
export function recPressPlay(): void {
  rec.pressPlay();
  publishRec();
}
export function recToggleRef(): void {
  rec.toggleRef();
  publishRec();
}
export function recWaveform(): void {
  rec.setMode(false);
  publishRec();
}
export function recSpectrumMode(): void {
  rec.setMode(true);
  publishRec();
}
export function recCloseSet(): void {
  rec.closeSet();
  publishRec();
}

// ---- MUSIC -----------------------------------------------------------------------

export const [musicBars, setMusicBars] = createSignal<Bar[]>([]);
export const [musicPlaying, setMusicPlaying] = createSignal<boolean>(false);
export const [musicProgress, setMusicProgress] = createSignal<Px>(0);
export const [musicElapsed, setMusicElapsed] = createSignal<string>("0:00");
export const [musicDuration, setMusicDuration] = createSignal<string>("0:45");
export const [musicStatus, setMusicStatus] = createSignal<string>("Ready");
export const [musicStatusColor, setMusicStatusColor] = createSignal<Color>("#64748B");
export const [musicButtonText, setMusicButtonText] = createSignal<string>("PLAY");
export const [musicButtonBg, setMusicButtonBg] = createSignal<Color>("#2563EB");
export const [musicButtonFg, setMusicButtonFg] = createSignal<Color>("#FFFFFF");
export const [musicButtonEnabled, setMusicButtonEnabled] = createSignal<boolean>(true);
export const [musicVolume, setMusicVolume] = createSignal<string>("VOL --%");
export const [musicKnob, setMusicKnob] = createSignal<Px>(0);
export const [musicIndicator, setMusicIndicator] = createSignal<Px>(0);
export const [musicSliderEnabled, setMusicSliderEnabled] = createSignal<boolean>(true);
export const [musicBarOpacity, setMusicBarOpacity] = createSignal<f32>(0.6);

function publishMusic(): void {
  const state = device.musicState;
  const playing = state === music.PLAYING;
  setMusicBars(music.bars());
  setMusicPlaying(playing);
  setMusicProgress(music.progress() * f32(432) * PX);
  setMusicElapsed(music.elapsedText());
  setMusicDuration(music.durationText());
  setMusicStatus(music.statusText());
  setMusicStatusColor(music.statusColor());
  const waiting = state === music.PREPARING || state === music.NOT_READY;
  setMusicButtonText(playing ? "PAUSE" : state === music.PREPARING ? "WAIT" : "PLAY");
  setMusicButtonBg(playing ? "#DC2626" : waiting ? "#E5E7EB" : "#2563EB");
  setMusicButtonFg(waiting ? "#64748B" : "#FFFFFF");
  setMusicButtonEnabled(!waiting);
  setMusicVolume(`VOL ${device.volume}%`);
  const knob = music.knob();
  setMusicKnob((knob - f32(228 + 1 + 15)) * PX);
  setMusicIndicator((knob - f32(251) + f32(9)) * PX);
  setMusicSliderEnabled(state !== music.NOT_READY);
  setMusicBarOpacity(playing ? 1 : 0.6);
}

export function musicPress(): void {
  music.press();
  publishMusic();
}

// ---- PING, USB -------------------------------------------------------------------

export const [pingEthText, setPingEthText] = createSignal<string>("");
export const [pingEthDot, setPingEthDot] = createSignal<Color>("#64748B");
export const [pingEthDetail, setPingEthDetail] = createSignal<string>("");
export const [pingTargets, setPingTargets] = createSignal<Target[]>([]);
export const [pingEnabled, setPingEnabled] = createSignal<boolean>(true);
export const [pingSummary, setPingSummary] = createSignal<string>("");
export const [pingSummaryColor, setPingSummaryColor] = createSignal<Color>("#64748B");
export const [usbResult, setUsbResult] = createSignal<string>("");

function publishPing(): void {
  setPingEthText(net.ethText());
  setPingEthDot(net.ethDot());
  setPingEthDetail(net.ethDetail());
  setPingTargets(net.targets());
  setPingEnabled(!device.pingRunning);
  setPingSummary(wrap(net.summaryText(), 414, 16));
  setPingSummaryColor(net.summaryColor());
  setActionEnabled(!device.pingRunning);
}

export function pingToggle(index: i32): void {
  net.toggle(index);
  publishPing();
}

// ---- IR --------------------------------------------------------------------------

export const [irStatus, setIrStatus] = createSignal<string>("");
export const [irStatusColor, setIrStatusColor] = createSignal<Color>("#DC2626");
export const [irResultText, setIrResultText] = createSignal<string>("");
export const [irResultColor, setIrResultColor] = createSignal<Color>("#D97706");
export const [irBytes, setIrBytes] = createSignal<string[]>(["00", "04", "00", "08"]);
export const [irPressed, setIrPressed] = createSignal<i32>(-1);
export const [irAuto, setIrAuto] = createSignal<boolean>(false);
export const [irRepeatOn, setIrRepeatOn] = createSignal<boolean>(false);
export const [irSendEnabled, setIrSendEnabled] = createSignal<boolean>(false);
export const [irAutoEnabled, setIrAutoEnabled] = createSignal<boolean>(true);
export const [irFramesText, setIrFramesText] = createSignal<string>("Frames:0");
export const [irShown, setIrShown] = createSignal<boolean>(false);
export const [irRepeatText, setIrRepeatText] = createSignal<string>("NO");
export const [irAddressText, setIrAddressText] = createSignal<string>("");
export const [irCommandText, setIrCommandText] = createSignal<string>("");
export const [irRaw, setIrRaw] = createSignal<string>("");

function publishIr(): void {
  setIrStatus(ir.statusText());
  setIrStatusColor(ir.statusColor());
  setIrResultText(dots(ir.resultText(), 410, 14));
  setIrResultColor(ir.resultColor());
  const b = ir.bytes();
  setIrBytes([ir.hex2(b[0]), ir.hex2(b[1]), ir.hex2(b[2]), ir.hex2(b[3])]);
  setIrPressed(ir.pressed);
  setIrAuto(ir.autoInc);
  setIrRepeatOn(ir.repeat);
  setIrSendEnabled(device.irReady && !device.irInitializing && !device.irSending);
  setIrAutoEnabled(!device.irSending);
  setIrFramesText(`Frames:${device.irFrames}`);
  setIrShown(ir.rxShown());
  setIrRepeatText(device.irRepeat ? "YES" : "NO");
  setIrAddressText(ir.hex4(device.irAddress));
  setIrCommandText(ir.hex4(device.irCommand));
  setIrRaw(ir.raw());
}

export function irToggleAuto(): void {
  ir.toggleAuto();
  publishIr();
}
export function irToggleRepeat(): void {
  ir.toggleRepeat();
  publishIr();
}
export function irSend(): void {
  ir.send();
  publishIr();
}
export function irClear(): void {
  ir.clear();
  publishIr();
}

// ---- touch indicator -------------------------------------------------------------

export const [indicatorOn, setIndicatorOn] = createSignal<boolean>(false);
export const [indicatorX, setIndicatorX] = createSignal<Px>(0);
export const [indicatorY, setIndicatorY] = createSignal<Px>(0);

/** The global 8x8 touch indicator, on every screen while enabled. */
function publishIndicator(): void {
  setIndicatorOn(device.touchIndicator && device.pointerDown);
  setIndicatorX(f32(device.pointerX - 4) * PX);
  setIndicatorY(f32(device.pointerY - 4) * PX);
}

// ---- frame -----------------------------------------------------------------------

function publishAll(): void {
  const id = windows.open;
  if (id === windows.OFF) {
    publishOff();
  } else if (id === windows.SLEEP) {
    publishSleep();
  } else if (id === windows.POWER) {
    publishPower();
  } else if (id === windows.TIMER) {
    publishTimer();
  } else if (id === windows.I2C) {
    publishI2c();
  } else if (id === windows.SD) {
    publishSd();
  } else if (id === windows.TOUCH) {
    publishTouch();
  } else if (id === windows.IMU) {
    publishImu();
  } else if (id === windows.REC) {
    publishRec();
  } else if (id === windows.MUSIC) {
    publishMusic();
  } else if (id === windows.PING) {
    publishPing();
  } else if (id === windows.USB) {
    setUsbResult(net.usbResult);
  } else if (id === windows.IR) {
    publishIr();
  }
  publishDialog();
}

function deliverEvents(): void {
  while (len(device.powerOffResults) > 0) {
    const code = device.powerOffResults[0];
    removeAt(device.powerOffResults, 0);
    off.powerOffResult(code);
  }
  while (len(device.sleepErrors) > 0) {
    const error = device.sleepErrors[0];
    removeAt(device.sleepErrors, 0);
    sleep.failed(error);
  }
}

function step(): void {
  deliverEvents();
  nav.step(device.pointerX, device.pointerY, device.pointerDown, windows.open >= 0 || dialog.open);
  windows.step();
  if (windows.closed >= 0) {
    closing(windows.closed);
  }
  if (windows.open === windows.OFF) {
    off.step();
  } else if (windows.open === windows.POWER) {
    if (device.gpioConfigs !== gpioConfigsSeen) {
      gpioConfigsSeen = device.gpioConfigs;
      power.applied(device.gpioConfigError);
    }
    power.stepSlider(tab, device.pointerX, device.pointerY, device.pointerDown && !dialog.open && power.popupPin === 0);
    power.step();
  } else if (windows.open === windows.TIMER) {
    timer.step(device.pointerX, device.pointerY, device.pointerDown, device.rtcReady && windows.phase === 0);
  } else if (windows.open === windows.SD) {
    sd.step(device.pointerX, device.pointerY, device.pointerDown);
    if (device.sdTestText !== sdTestTextSeen) {
      sdTestTextSeen = device.sdTestText;
      sd.testText(device.sdTestText);
    }
    if (device.sdTests !== sdTestsSeen) {
      sdTestsSeen = device.sdTests;
      sd.testDone();
    }
  }
  if (windows.open === windows.TOUCH && windows.phase === 0 && touch.idle(device.pointerDown)) {
    dropWindow();
  }
  if (windows.open === windows.MUSIC) {
    music.step(device.pointerX, device.pointerY, device.pointerDown);
  } else if (windows.open === windows.USB) {
    net.stepUsb();
  } else if (windows.open === windows.IR) {
    ir.step(device.pointerX, device.pointerY, device.pointerDown && windows.phase === 0);
  }
  while (len(device.irNext) > 0) {
    const next = device.irNext[0];
    removeAt(device.irNext, 0);
    ir.sent(next);
  }
  if (windows.open === windows.IMU && imu.step()) {
    dropWindow();
  }
  if (windows.open < 0) {
    setTouchCanvasOn(false);
  }
  publishIndicator();
  while (len(device.i2cResults) > 0) {
    const status = device.i2cResults[0];
    removeAt(device.i2cResults, 0);
    i2c.scanned(status);
  }
  publishScreens();
  publishWindow();
  publishAll();
}

/** Mount: one model step per frame. */
export async function start(): Promise<void> {
  device.request("hello");
  setWinPlaceholder(frame.placeholder());
  publishScreens();
  // Bounded loop: over two years of frames at 30 Hz, one step each.
  for (let i: i32 = 0; i < 2147483647; i++) {
    await frames(1);
    step();
  }
}

// ---- host entry points (JS mode) -------------------------------------------------

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
