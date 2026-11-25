// PING, WiFi and USB windows.
//
// PING (ui_function_ping_window.cpp): the Ethernet state (IP101 over RMII,
// DHCP) and four targets (Google DNS 8.8.8.8, Cloudflare DNS 1.1.1.1,
// M5Stack Site m5stack.com, the gateway); "Ping" pings the checked targets
// one after the other, 4 echoes of 32 bytes, 500 ms apart, 1000 ms timeout,
// and shows the mean round trip. The board runs the pings and reports each
// row as "<dot tone><text tone><text>" lines.
//
// WiFi (ui_function_wifi_window.cpp) needs the A172 Stamp-AddOn C6 For P4
// module over ESP-Hosted; this firmware does not drive it, so the window
// keeps the factory layout under its module overlay.
//
// USB (ui_function_usb_window.cpp) is the USB-A host hub page; this
// firmware does not run the USB host, so the counters stay at 0 and Check
// reports that.
import { codePoints, fromCodePoint, i32, len, push, type Color } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import { framesIn } from "./ease";

export const MUTED: i32 = 0;
export const SUCCESS: i32 = 1;
export const WARNING: i32 = 2;
export const ERROR: i32 = 3;

export function tone(value: i32): Color {
  return value === SUCCESS ? "#16A34A" : value === WARNING ? "#D97706" : value === ERROR ? "#DC2626" : "#64748B";
}

// ---- PING ------------------------------------------------------------------------

export interface Target {
  id: i32;
  top: i32;
  info: string;
  result: string;
  dot: Color;
  resultColor: Color;
  checked: boolean;
}

const NAMES: string[] = ["Google DNS (8.8.8.8)", "Cloudflare DNS (1.1.1.1)", "M5Stack Site (m5stack.com)", ""];
export let checked: boolean[] = [true, true, false, false];
let summary: string = "Select targets and tap Ping to check network connectivity.";
let summaryTone: i32 = MUTED;
let summarySeen: string = "";

export function openPing(): void {
  summary = "Select targets and tap Ping to check network connectivity.";
  summaryTone = MUTED;
  summarySeen = device.pingSummary;
  device.request("pingPage 1");
}

export function closePing(): void {
  device.request("pingPage 0");
}

export function toggle(index: i32): void {
  if (!device.pingRunning) {
    checked[index] = !checked[index];
  }
}

export function ping(): void {
  if (device.pingRunning) {
    return;
  }
  if (device.ethState !== 2) {
    summary = "Network not ready. Connect ETH or WiFi first.";
    summaryTone = ERROR;
    return;
  }
  let mask: i32 = 0;
  for (let i: i32 = 0; i < 4; i++) {
    if (checked[i]) {
      mask = mask + (1 << i);
    }
  }
  if (mask === 0) {
    summary = "Select at least one target.";
    summaryTone = WARNING;
    return;
  }
  device.request(`ping ${mask}`);
}

/** Line `index` of the board's row report. */
function line(index: i32): string {
  const codes = codePoints(device.pingRows);
  let current: i32 = 0;
  let out = "";
  for (let i: i32 = 0; i < len(codes); i++) {
    if (codes[i] === 10) {
      current = current + 1;
    } else if (current === index) {
      out = out + fromCodePoint(codes[i]);
    }
  }
  return out;
}

function gatewayInfo(): string {
  if (device.ethState === 2 && device.ethGateway !== "" && device.ethGateway !== "192.168.1.1") {
    return `Gateway (${device.ethGateway})`;
  }
  return "Gateway (--)";
}

/** Changes when targets() would: the view takes new rows only then. */
export function targetsKey(): string {
  let flags = "";
  for (let i: i32 = 0; i < 4; i++) {
    flags = flags + (checked[i] ? "1" : "0");
  }
  return `${device.pingRows}|${flags}|${gatewayInfo()}`;
}

export function targets(): Target[] {
  const out: Target[] = [];
  for (let i: i32 = 0; i < 4; i++) {
    const row = line(i);
    const codes = codePoints(row);
    let dotTone: i32 = MUTED;
    let textTone: i32 = MUTED;
    let text = "--";
    if (len(codes) >= 2) {
      dotTone = codes[0] - 48;
      textTone = codes[1] - 48;
      text = "";
      for (let k: i32 = 2; k < len(codes); k++) {
        text = text + fromCodePoint(codes[k]);
      }
    }
    push(out, {
      id: i,
      top: 197 + i * 46,
      info: i === 3 ? gatewayInfo() : NAMES[i],
      result: text,
      dot: tone(dotTone),
      resultColor: tone(textTone),
      checked: checked[i],
    });
  }
  return out;
}

export function summaryText(): string {
  if (device.pingSummary !== summarySeen) {
    summarySeen = device.pingSummary;
    summary = device.pingSummary;
    summaryTone = device.pingSummaryTone;
  }
  return summary;
}

export function summaryColor(): Color {
  return tone(summaryTone);
}

export function ethText(): string {
  return device.ethState === 2 ? "Ethernet: Got IP" : device.ethState === 1 ? "Ethernet: Link Up" : "Ethernet: Disconnected";
}

export function ethDot(): Color {
  return device.ethState === 2 ? "#16A34A" : device.ethState === 1 ? "#D97706" : "#64748B";
}

export function ethDetail(): string {
  if (device.ethState !== 2) {
    return "IP: --   |   GW: --";
  }
  return `IP: ${device.ethIp}   |   GW: ${device.ethGateway}`;
}

// ---- USB -------------------------------------------------------------------------

export let usbResult: string = "";
let usbFrames: i32 = 0;

export function openUsb(): void {
  usbResult = "";
  usbFrames = 0;
}

export function check(): void {
  usbResult = "USB host is not supported by this firmware.";
}

/** The factory checks 1000 ms after the window opens. */
export function stepUsb(): void {
  usbFrames = usbFrames + 1;
  if (usbFrames === framesIn(1000)) {
    check();
  }
}
