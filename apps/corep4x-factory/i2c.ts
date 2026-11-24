// I2C SCAN window (ui_function_i2cscan_window.cpp): two bus buttons
// (Internal: I2C0 SCL 9 / SDA 11; PORT.A: I2C1 SCL 16 / SDA 18). A scan probes
// 0x08..0x77 three times 50 ms apart and keeps the addresses that answered
// every time. The internal bus shows its eight known devices as OK or MISS,
// then up to three unknown addresses; Port A shows up to eleven devices.
// "Clean" hides the results.
import { fromCodePoint, i32, idiv, imod, len, push, type Color } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import { dots } from "./wrap";

export interface Bubble {
  id: i32;
  left: i32;
  top: i32;
  address: string;
  name: string;
  state: string;
  bg: Color;
  border: Color;
  text: Color;
}

const KNOWN_ADDRESS: i32[] = [0x6e, 0x6f, 0x18, 0x40, 0x58, 0x6a, 0x6b, 0x32];
const KNOWN_NAME: string[] = ["PM1", "PYB", "ES8311", "ES7210", "CST3XX", "LSM6DS3TR-C", "LSM6DS3TR-C ALT", "RX8130"];
const TITLES: string[] = ["Internal I2C", "PORT.A I2C"];
const IDLE_SUMMARY: string = "Tap a bus to search 0x08-0x77.";

export let scanning: boolean = false;
let scanBus: i32 = 0;
export let summary: string = IDLE_SUMMARY;
export let summaryError: boolean = false;
export let bubbles: Bubble[] = [];

export function open(): void {
  summary = IDLE_SUMMARY;
  summaryError = false;
  bubbles = [];
}

export function clean(): void {
  if (scanning) {
    return;
  }
  open();
}

export function scan(bus: i32): void {
  if (scanning) {
    return;
  }
  scanning = true;
  scanBus = bus;
  summary = `Scanning ${TITLES[bus]} bus...`;
  summaryError = false;
  bubbles = [];
  device.request(`i2cScan ${bus}`);
}

function digit(value: i32): string {
  return fromCodePoint(value < 10 ? 48 + value : 55 + value);
}

function hex(value: i32): string {
  return `0x${digit(idiv(value, 16))}${digit(imod(value, 16))}`;
}

function place(index: i32): i32 {
  return 35 + imod(index, 2) * 204;
}

function row(index: i32): i32 {
  return 201 + idiv(index, 2) * 43;
}

function bubble(index: i32, address: string, name: string, state: string, tone: i32): Bubble {
  // tone 0 found (green), 1 missing (orange), 2 unknown (grey)
  return {
    id: index,
    left: place(index),
    top: row(index),
    address: address,
    name: dots(name, 74, 16),
    state: state,
    bg: tone === 0 ? "#DCFCE7" : tone === 1 ? "#FFEDD5" : "#E2E8F0",
    border: tone === 0 ? "#86EFAC" : tone === 1 ? "#FDBA74" : "#CBD5E1",
    text: tone === 0 ? "#166534" : tone === 1 ? "#9A3412" : "#334155",
  };
}

function found(address: i32): boolean {
  for (let i: i32 = 0; i < len(device.i2cAddresses); i++) {
    if (device.i2cAddresses[i] === address) {
      return true;
    }
  }
  return false;
}

/** The board finished a scan: `status` 0, -1 when the bus was not ready,
 *  -2 when the scan task could not start; the addresses are in
 *  device.i2cAddresses, ascending. */
export function scanned(status: i32): void {
  scanning = false;
  const title = TITLES[scanBus];
  if (status === -2) {
    summary = "Scan failed: cannot create worker task";
    summaryError = true;
    bubbles = [];
    return;
  }
  if (status < 0) {
    summary = `Scan failed: ${title} bus not ready`;
    summaryError = true;
    bubbles = [];
    return;
  }
  const count = len(device.i2cAddresses);
  summary = count === 0 ? `${title}: Found 0 devices` : `${title}: Found ${count} device(s)`;
  summaryError = false;
  const out: Bubble[] = [];
  if (scanBus === 0) {
    for (let k: i32 = 0; k < 8; k++) {
      let address = KNOWN_ADDRESS[k];
      let ok = found(address);
      if (k === 1 && !ok && found(0x4f)) {
        address = 0x4f;
        ok = true;
      }
      push(out, bubble(k, hex(address), KNOWN_NAME[k], ok ? "OK" : "MISS", ok ? 0 : 1));
    }
    let unknown: i32 = 0;
    for (let i: i32 = 0; i < count; i++) {
      const address = device.i2cAddresses[i];
      let known = address === 0x4f;
      for (let k: i32 = 0; k < 8; k++) {
        if (KNOWN_ADDRESS[k] === address) {
          known = true;
        }
      }
      if (!known) {
        unknown = unknown + 1;
      }
    }
    let shown: i32 = 0;
    for (let i: i32 = 0; i < count && shown < 3; i++) {
      const address = device.i2cAddresses[i];
      let known = address === 0x4f;
      for (let k: i32 = 0; k < 8; k++) {
        if (KNOWN_ADDRESS[k] === address) {
          known = true;
        }
      }
      if (!known) {
        const state = shown === 2 && unknown > 3 ? `+${unknown - 2}` : "UNK";
        push(out, bubble(8 + shown, hex(address), "Unknown", state, 2));
        shown = shown + 1;
      }
    }
  } else {
    for (let i: i32 = 0; i < count && i < 11; i++) {
      const state = i === 10 && count > 11 ? `+${count - 10}` : "OK";
      push(out, bubble(i, hex(device.i2cAddresses[i]), "Device", state, 0));
    }
  }
  bubbles = out;
}
