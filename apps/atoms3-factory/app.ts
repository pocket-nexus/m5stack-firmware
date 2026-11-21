// Root model of the AtomS3 factory demo (M5AtomS3-UserDemo src/main.cpp).
// One task replays the factory's loop(): every frame it reads the button
// edges, the device.ts values and the function timers, and updates the
// signals the views draw. Navigation: click = next function in the menu,
// hold (500 ms) = enter or leave a function; inside a function a click is the
// function's own action.
import { createSignal } from "solid-js";
import {
  abs,
  after,
  asin,
  atan2,
  clear,
  copy,
  f32,
  fixed,
  frames,
  fromCodePoint,
  i32,
  idiv,
  imod,
  len,
  min,
  push,
  removeAt,
  sqrt,
  trunc,
  f64,
} from "@pocketjs/framework/solid/std";
import * as device from "./device";

export const SCREEN_BOOT = 0;
export const SCREEN_MENU = 1;
export const SCREEN_FUNCTION = 2;

export const FUNC_WIFI = 0;
export const FUNC_I2C = 1;
export const FUNC_UART = 2;
export const FUNC_PWM = 3;
export const FUNC_ADC = 4;
export const FUNC_IR = 5;
export const FUNC_IMU = 6;
const FUNC_COUNT = 7;
const FUNC_NAMES: string[] = ["wifi", "i2c", "uart", "pwm", "adc", "ir", "imu"];

// One printed line of the Wi-Fi function's scrolling canvas.
export interface Line {
  id: i32;
  text: string;
  big: boolean;
}

// Screen and navigation ------------------------------------------------------
export const [screen, setScreen] = createSignal<i32>(SCREEN_BOOT);
export const [bootStep, setBootStep] = createSignal<i32>(0);
export const [func, setFunc] = createSignal<i32>(FUNC_WIFI);

// Wi-Fi scan
export const [wifiListing, setWifiListing] = createSignal<boolean>(false);
export const [wifiLines, setWifiLines] = createSignal<Line[]>([]);
export const [wifiOverflow, setWifiOverflow] = createSignal<boolean>(false);
// I2C scan: 0 scanning, 1 not found, 2 found
export const [i2cState, setI2cState] = createSignal<i32>(0);
export const [i2cCount, setI2cCount] = createSignal<i32>(0);
export const [i2cTexts, setI2cTexts] = createSignal<string[]>([]);
export const [i2cColors, setI2cColors] = createSignal<i32[]>([]);
// UART monitor
export const [uartMode, setUartMode] = createSignal<i32>(0);
export const [uartRxText, setUartRxText] = createSignal<string>("");
export const [uartRxColor, setUartRxColor] = createSignal<i32>(0);
export const [uartTxText, setUartTxText] = createSignal<string>("");
export const [uartTxColor, setUartTxColor] = createSignal<i32>(0);
// PWM
export const [pwmMode, setPwmMode] = createSignal<i32>(0);
export const [pwmDutyText, setPwmDutyText] = createSignal<string>("0x80");
// ADC
export const [adc1Text, setAdc1Text] = createSignal<string>("");
export const [adc2Text, setAdc2Text] = createSignal<string>("");
// IR
export const [irMode, setIrMode] = createSignal<i32>(0);
export const [irText, setIrText] = createSignal<string>("");
// IMU ball centre, canvas pixels
export const [ballX, setBallX] = createSignal<i32>(64);
export const [ballY, setBallY] = createSignal<i32>(49);

// Private state of the factory's func_*_t objects -----------------------------
let frame: i32 = 0;
let clicked: boolean = false;
let held: boolean = false;
let rng: i32 = 4242;

let wifiShowIndex: i32 = 0;
let wifiPaused: boolean = false;
let wifiLastShow: i32 = 0;
let wifiSeen: i32 = 0;
let wifiSimAt: i32 = -1;
let lineId: i32 = 0;
let lineHeights: i32 = 0;

let i2cSeen: i32 = 0;
let i2cSimAt: i32 = -1;

let pwmDuty: i32 = 128;
let adcLast: i32 = 0;
let adcSeen: i32 = 0;
let irCmd: i32 = 0;
let irLastSend: i32 = 0;
let imuSeen: i32 = 0;

// Madgwick IMU filter state (quaternion) and gain.
let q0: f32 = 1.0;
let q1: f32 = 0.0;
let q2: f32 = 0.0;
let q3: f32 = 0.0;
const BETA: f32 = 0.1;
const SAMPLE_DT: f32 = 0.0333333;
const DEG_TO_RAD: f32 = 0.0174532925;
const RAD_TO_DEG: f32 = 57.29578;

// Formatting -------------------------------------------------------------------

function hexDigit(value: i32): string {
  return value < 10 ? fromCodePoint(48 + value) : fromCodePoint(55 + value);
}

// printf("0x%02X")
export function hex(value: i32): string {
  let digits = "";
  let rest = value;
  let count = 0;
  while (rest > 0 || count < 2) {
    digits = `${hexDigit(imod(rest, 16))}${digits}`;
    rest = idiv(rest, 16);
    count++;
  }
  return `0x${digits}`;
}

// The factory picks random(0, 255) per channel; the view has a 16-colour
// palette (RandomText.tsx), so the model picks a random palette index.
function randomColor(): i32 {
  rng = imod(rng * 75 + 74, 65537);
  return imod(rng, 16);
}

function nowMs(): i32 {
  return idiv(frame * 100, 3);
}

// Input --------------------------------------------------------------------------

export function click(): void {
  clicked = true;
}

export function hold(): void {
  held = true;
}

// Boot animation, then the frame loop ------------------------------------------

export async function run(): Promise<void> {
  for (let step = 1; step <= 8; step++) {
    setBootStep(step);
    await after(200);
  }
  await after(500);
  setScreen(SCREEN_MENU);
  clicked = false;
  held = false;
  for (let step: i32 = 0; step < 2147483647; step++) {
    await frames(1);
    frame = frame + 1;
    loopOnce();
  }
}

function loopOnce(): void {
  const isClick = clicked;
  const isHold = held;
  clicked = false;
  held = false;
  const entered = screen() === SCREEN_FUNCTION;
  if (isClick && !entered) {
    setFunc(imod(func() + 1, FUNC_COUNT));
  }
  if (isHold) {
    if (!entered) {
      setScreen(SCREEN_FUNCTION);
      enter(func());
    } else {
      leave(func());
      setScreen(SCREEN_MENU);
    }
  }
  if (screen() === SCREEN_FUNCTION) {
    update(func(), isClick && entered);
  }
}

function enter(index: i32): void {
  device.request(`enter ${FUNC_NAMES[index]}`);
  switch (index) {
    case FUNC_WIFI:
      wifiStart();
      break;
    case FUNC_I2C:
      i2cScan();
      break;
    case FUNC_UART:
      uartStart();
      break;
    case FUNC_PWM:
      device.request(`pwm ${pwmDuty} ${pwmDuty}`);
      setPwmDutyText(hex(pwmDuty));
      break;
    case FUNC_ADC:
      setAdc1Text("");
      setAdc2Text("");
      adcLast = 0;
      break;
    case FUNC_IR:
      setIrText("");
      irLastSend = 0;
      break;
    case FUNC_IMU:
      setBallX(64);
      setBallY(49);
      break;
  }
}

function leave(index: i32): void {
  device.request(`leave ${FUNC_NAMES[index]}`);
  clear(device.uartRx);
  clear(device.uartTx);
  clear(device.usbRx);
}

function update(index: i32, isClick: boolean): void {
  switch (index) {
    case FUNC_WIFI:
      wifiUpdate(isClick);
      break;
    case FUNC_I2C:
      i2cUpdate(isClick);
      break;
    case FUNC_UART:
      uartUpdate(isClick);
      break;
    case FUNC_PWM:
      pwmUpdate(isClick);
      break;
    case FUNC_ADC:
      adcUpdate();
      break;
    case FUNC_IR:
      irUpdate(isClick);
      break;
    case FUNC_IMU:
      imuUpdate();
      break;
  }
}

// func_wifi_t ------------------------------------------------------------------

function wifiStart(): void {
  wifiShowIndex = 0;
  wifiPaused = false;
  wifiLastShow = 0;
  lineHeights = 0;
  setWifiListing(false);
  setWifiLines([]);
  setWifiOverflow(false);
  wifiSeen = device.wifiDone;
  if (device.attached) {
    device.request("wifiScan");
  } else {
    wifiSimAt = nowMs() + 1500;
  }
}

function printLine(text: string, big: boolean): void {
  const lines = copy(wifiLines());
  lineId++;
  push(lines, { id: lineId, text: text, big: big });
  lineHeights = lineHeights + (big ? 14 : 12);
  // Lines scrolled fully past the top of the 98-pixel canvas are dropped.
  while (len(lines) > 9) {
    const first = removeAt(lines, 0);
    lineHeights = lineHeights - (first.big ? 14 : 12);
  }
  setWifiLines(lines);
  setWifiOverflow(lineHeights > 98);
}

function wifiUpdate(isClick: boolean): void {
  if (!wifiListing()) {
    if (!device.attached && wifiSimAt >= 0 && nowMs() >= wifiSimAt) {
      wifiSimAt = -1;
      device.simulateWifi();
    }
    if (device.wifiDone !== wifiSeen) {
      wifiSeen = device.wifiDone;
      // A scan that found nothing keeps "Scaning..." as on the factory.
      if (device.wifiCount > 0) {
        setWifiListing(true);
      }
    }
    return;
  }
  if (isClick) {
    wifiPaused = !wifiPaused;
  }
  if (wifiPaused || nowMs() - wifiLastShow <= 500) {
    return;
  }
  const shown = min(min(device.wifiCount, 5), len(device.wifiRssi));
  if (wifiShowIndex < shown) {
    const rssi = i32(device.wifiRssi[wifiShowIndex]);
    printLine(`${wifiShowIndex + 1}. ${device.wifiSsids[wifiShowIndex]} ${rssi}`, false);
    wifiShowIndex++;
  } else {
    printLine("", false);
    printLine("", false);
    printLine("Top 5 list:", true);
    wifiShowIndex = 0;
  }
  wifiLastShow = nowMs();
}

// func_i2c_t -------------------------------------------------------------------

function i2cScan(): void {
  setI2cState(0);
  i2cSeen = device.i2cDone;
  if (device.attached) {
    device.request("i2cScan");
  } else {
    i2cSimAt = nowMs() + 100;
  }
}

function i2cUpdate(isClick: boolean): void {
  if (isClick) {
    i2cScan();
  }
  if (!device.attached && i2cSimAt >= 0 && nowMs() >= i2cSimAt) {
    i2cSimAt = -1;
    device.simulateI2c();
  }
  if (device.i2cDone === i2cSeen) {
    return;
  }
  i2cSeen = device.i2cDone;
  const count = min(len(device.i2cAddresses), 6);
  const texts: string[] = [];
  const colors: i32[] = [];
  for (let i = 0; i < count; i++) {
    push(texts, `${i + 1}. ${hex(i32(device.i2cAddresses[i]))}`);
    push(colors, randomColor());
  }
  setI2cTexts(texts);
  setI2cColors(colors);
  setI2cCount(count);
  setI2cState(count === 0 ? 1 : 2);
}

export function i2cText(index: i32): string {
  return i2cTexts()[index];
}

export function i2cColor(index: i32): i32 {
  return i2cColors()[index];
}

// func_uart_t ------------------------------------------------------------------

function uartStart(): void {
  const mode = uartMode();
  const baud = mode === 0 || mode === 2 ? 9600 : 115200;
  // Serial.begin(baud, SERIAL_8N1, rx, tx): modes 0/1 RX G2 TX G1, 2/3 swapped.
  if (mode < 2) {
    device.request(`uart ${baud} 2 1`);
  } else {
    device.request(`uart ${baud} 1 2`);
  }
  setUartRxText("");
  setUartTxText("");
}

function uartUpdate(isClick: boolean): void {
  if (isClick) {
    setUartMode(imod(uartMode() + 1, 4));
    uartStart();
  }
  for (const byte of device.uartRx) {
    setUartRxText(hex(byte));
    setUartRxColor(randomColor());
  }
  clear(device.uartRx);
  for (const byte of device.uartTx) {
    setUartTxText(hex(byte));
    setUartTxColor(randomColor());
  }
  clear(device.uartTx);
}

// func_pwm_t -------------------------------------------------------------------

function pwmUpdate(isClick: boolean): void {
  if (isClick) {
    setPwmMode(imod(pwmMode() + 1, 4));
    pwmDuty = pwmMode() !== 0 ? 0 : 15;
    device.request(`pwm ${pwmDuty} ${pwmDuty}`);
    setPwmDutyText(hex(pwmDuty));
  }
  for (const byte of device.usbRx) {
    pwmDuty = byte;
    const mode = pwmMode();
    if (mode <= 1) {
      device.request(`pwm ${pwmDuty} ${pwmDuty}`);
    } else if (mode === 2) {
      device.request(`pwm ${pwmDuty} 0`);
    } else {
      device.request(`pwm 0 ${pwmDuty}`);
    }
    setPwmDutyText(hex(pwmDuty));
  }
  clear(device.usbRx);
}

// func_adc_t -------------------------------------------------------------------

function volts(raw: f32): string {
  return `${fixed(f64(raw) / 4095.0 * 3.3, 1)}V`;
}

function adcUpdate(): void {
  if (device.adcSamples !== adcSeen && len(device.adc) >= 2) {
    adcSeen = device.adcSamples;
    setAdc1Text(volts(device.adc[0]));
    setAdc2Text(volts(device.adc[1]));
  }
  if (nowMs() - adcLast < 90) {
    return;
  }
  adcLast = nowMs();
  if (device.attached) {
    device.request("adc");
  } else {
    device.simulateAdc();
  }
}

// func_ir_t --------------------------------------------------------------------

function irSend(command: i32): void {
  device.request(`ir ${device.irAddress} ${imod(command, 65536)}`);
  setIrText(hex(command));
}

function irUpdate(isClick: boolean): void {
  if (isClick) {
    setIrMode(imod(irMode() + 1, 2));
    setIrText("");
  }
  if (irMode() === 0) {
    for (const byte of device.usbRx) {
      irSend(byte);
    }
    clear(device.usbRx);
  } else if (nowMs() - irLastSend > 1000) {
    irCmd++;
    irSend(irCmd);
    irLastSend = nowMs();
  }
}

// func_imu_t -------------------------------------------------------------------

// Madgwick's gradient-descent orientation filter, IMU (6-axis) form,
// written from S. Madgwick, "An efficient orientation filter for inertial
// and inertial/magnetic sensor arrays" (2010). Gyroscope in dps, accel in g.
function madgwick(gxd: f32, gyd: f32, gzd: f32, ax0: f32, ay0: f32, az0: f32): void {
  const gx = gxd * DEG_TO_RAD;
  const gy = gyd * DEG_TO_RAD;
  const gz = gzd * DEG_TO_RAD;
  let d0: f32 = f32(0.5) * (-q1 * gx - q2 * gy - q3 * gz);
  let d1: f32 = f32(0.5) * (q0 * gx + q2 * gz - q3 * gy);
  let d2: f32 = f32(0.5) * (q0 * gy - q1 * gz + q3 * gx);
  let d3: f32 = f32(0.5) * (q0 * gz + q1 * gy - q2 * gx);
  const anorm = sqrt(ax0 * ax0 + ay0 * ay0 + az0 * az0);
  if (anorm > f32(0.0)) {
    const ax = ax0 / anorm;
    const ay = ay0 / anorm;
    const az = az0 / anorm;
    const f1 = f32(2.0) * (q1 * q3 - q0 * q2) - ax;
    const f2 = f32(2.0) * (q0 * q1 + q2 * q3) - ay;
    const f3 = f32(2.0) * (f32(0.5) - q1 * q1 - q2 * q2) - az;
    let s0: f32 = f32(-2.0) * q2 * f1 + f32(2.0) * q1 * f2;
    let s1: f32 = f32(2.0) * q3 * f1 + f32(2.0) * q0 * f2 - f32(4.0) * q1 * f3;
    let s2: f32 = f32(-2.0) * q0 * f1 + f32(2.0) * q3 * f2 - f32(4.0) * q2 * f3;
    let s3: f32 = f32(2.0) * q1 * f1 + f32(2.0) * q2 * f2;
    const snorm = sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (snorm > f32(0.0)) {
      s0 = s0 / snorm;
      s1 = s1 / snorm;
      s2 = s2 / snorm;
      s3 = s3 / snorm;
      d0 = d0 - BETA * s0;
      d1 = d1 - BETA * s1;
      d2 = d2 - BETA * s2;
      d3 = d3 - BETA * s3;
    }
  }
  q0 = q0 + d0 * SAMPLE_DT;
  q1 = q1 + d1 * SAMPLE_DT;
  q2 = q2 + d2 * SAMPLE_DT;
  q3 = q3 + d3 * SAMPLE_DT;
  const qnorm = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q0 = q0 / qnorm;
  q1 = q1 / qnorm;
  q2 = q2 / qnorm;
  q3 = q3 / qnorm;
}

// Arduino map(long) of a clamped angle onto [centre - 45, centre + 45].
function mapAngle(angle: f32, centre: i32): i32 {
  let degrees = trunc(angle);
  if (abs(degrees) > 90) {
    degrees = degrees < 0 ? -90 : 90;
  }
  return idiv((degrees + 90) * 90, 180) + centre - 45;
}

function imuUpdate(): void {
  if (!device.attached) {
    device.simulateImu(frame);
  }
  if (device.imuSamples === imuSeen || len(device.imu) < 6) {
    return;
  }
  imuSeen = device.imuSamples;
  const s = device.imu;
  madgwick(s[3], s[4], s[5], s[0], s[1], s[2]);
  const roll = atan2(q0 * q1 + q2 * q3, f32(0.5) - q1 * q1 - q2 * q2) * RAD_TO_DEG;
  let sinp = f32(-2.0) * (q1 * q3 - q0 * q2);
  if (sinp > f32(1.0)) {
    sinp = f32(1.0);
  }
  if (sinp < f32(-1.0)) {
    sinp = f32(-1.0);
  }
  const pitch = asin(sinp) * RAD_TO_DEG;
  setBallX(mapAngle(pitch, 64));
  setBallY(mapAngle(roll, 49));
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
