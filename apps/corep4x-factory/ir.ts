// IR TX/RX window (ui_function_irtxrx_window.cpp): NEC on the RMT (TX GPIO49
// at 38 kHz, RX GPIO13). Four byte tiles (ADDR H, ADDR L, CMD H, CMD L)
// change by one for every 12 px a finger drags up or down on them; Send NEC
// transmits address (ADDR H << 8 | ADDR L) and command, Repeat x2 sends the
// frame twice, Auto+1 advances the command after each send. The values and
// switches persist while the firmware runs; received NEC frames are shown
// with their count. The board reports its init and send results.
import { fromCodePoint, i32, idiv, imod, type Color } from "@pocketjs/framework/solid/std";
import * as device from "./device";

export let address: i32 = 0x0004;
export let command: i32 = 0x0008;
export let autoInc: boolean = false;
export let repeat: boolean = false;
/** The tile being dragged (0..3), -1 for none. */
export let pressed: i32 = -1;
let dragStart: i32 = 0;
let wasDown: boolean = false;
let local: string = "";
let localTone: i32 = 0;
/** The frame count when the window opened (0 after Clear): the RX fields
 *  show "--" until a frame arrives. */
let base: i32 = 0;

const TILE_LEFT: i32[] = [35, 140, 245, 350];
const TILE_TOP: i32 = 129;

function digit(value: i32): string {
  return fromCodePoint(value < 10 ? 48 + value : 55 + value);
}

/** "%02X" */
export function hex2(value: i32): string {
  return `${digit(idiv(value, 16))}${digit(imod(value, 16))}`;
}

/** "0x%04lX" */
export function hex4(value: i32): string {
  return `0x${hex2(idiv(value, 256))}${hex2(imod(value, 256))}`;
}

export function open(): void {
  pressed = -1;
  local = "";
  base = device.irFrames;
  device.request("ir 1");
}

export function close(): void {
  pressed = -1;
  device.request("ir 0");
}

export function reinit(): void {
  local = "";
  device.request("irInit");
}

export function bytes(): i32[] {
  return [idiv(address, 256), imod(address, 256), idiv(command, 256), imod(command, 256)];
}

function setByte(index: i32, value: i32): void {
  const v = imod(value + 256, 256);
  if (index === 0) {
    address = v * 256 + imod(address, 256);
  } else if (index === 1) {
    address = idiv(address, 256) * 256 + v;
  } else if (index === 2) {
    command = v * 256 + imod(command, 256);
  } else {
    command = idiv(command, 256) * 256 + v;
  }
  local = `NEC addr ${hex4(address)}  cmd ${hex4(command)}`;
  localTone = device.irReady ? 0 : 2;
}

/** One frame: the tile drags. */
export function step(x: i32, y: i32, down: boolean): void {
  if (down && !wasDown) {
    pressed = -1;
    for (let i: i32 = 0; i < 4; i++) {
      if (x >= TILE_LEFT[i] && x < TILE_LEFT[i] + 99 && y >= TILE_TOP && y < TILE_TOP + 132) {
        pressed = i;
        dragStart = y;
      }
    }
  } else if (down && pressed >= 0) {
    const steps = idiv(dragStart - y, 12);
    if (steps !== 0) {
      setByte(pressed, bytes()[pressed] + steps);
      dragStart = dragStart - steps * 12;
    }
  } else if (!down) {
    pressed = -1;
  }
  wasDown = down;
}

export function toggleAuto(): void {
  if (device.irSending) {
    return;
  }
  autoInc = !autoInc;
  local = autoInc ? "Auto+1 ON: command advances after Send." : "Auto+1 OFF: command stays fixed.";
  localTone = autoInc ? 4 : 0;
}

export function toggleRepeat(): void {
  repeat = !repeat;
}

export function send(): void {
  if (!device.irReady || device.irSending) {
    return;
  }
  local = "";
  device.request(`irSend ${address} ${command} ${repeat ? 1 : 0} ${autoInc ? 1 : 0}`);
}

/** After an Auto+1 send the board reports the next command. */
export function sent(next: i32): void {
  if (autoInc) {
    command = imod(next, 65536);
  }
}

export function clear(): void {
  device.request("irClear");
  base = 0;
}

export function resultText(): string {
  return local !== "" ? local : device.irResult;
}

export function resultColor(): Color {
  const t = local !== "" ? localTone : device.irResultTone;
  return t === 1 ? "#16A34A" : t === 2 ? "#D97706" : t === 3 ? "#DC2626" : t === 4 ? "#2563EB" : "#64748B";
}

export function statusText(): string {
  const init = device.irInitializing ? "WORK" : device.irReady ? "OK" : "NO";
  return `Init:${init}  L3B:${device.rails[0] ? "ON" : "OFF"}  RX:${device.irReady ? "NEC" : "--"}`;
}

export function statusColor(): Color {
  return device.irInitializing ? "#D97706" : device.irReady ? "#16A34A" : "#DC2626";
}

/** "0x%08lX" of address << 16 | command. */
export function raw(): string {
  const a = device.irAddress;
  const c = device.irCommand;
  return `0x${hex2(idiv(a, 256))}${hex2(imod(a, 256))}${hex2(idiv(c, 256))}${hex2(imod(c, 256))}`;
}

export function rxShown(): boolean {
  return device.irFrames > base;
}
