// SD CARD window (ui_function_sdcard_window.cpp): a file explorer of the FAT
// card at /sdcard (first 48 entries, directories first, then by name), a
// tap selects a row, a second tap within 400 ms opens a directory, Back goes
// up (never above /sdcard). "Test" writes, reads and deletes a 512 KiB file
// and reports the speeds in a dialog. The board lists a directory as text
// lines "<D|F><size>\t<name>".
import { codePoints, fromCodePoint, i32, len, push, type Color } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as dialog from "./dialog";
import { dots } from "./wrap";

export const OWNER: i32 = 2;
const ROOT: string = "/sdcard";
const LIST_TOP: i32 = 115;
const LIST_HEIGHT: i32 = 274;
const ROW_PITCH: i32 = 36;

export interface Row {
  id: i32;
  top: i32;
  text: string;
  bg: Color;
  border: Color;
  fg: Color;
}

interface Entry {
  dir: boolean;
  name: string;
  size: string;
}

let entries: Entry[] = [];
/** Counts the listings parsed into `entries`. */
let listing: i32 = 0;
export let current: string = ROOT;
let selected: i32 = -1;
let lastTap: i32 = -1;
let lastTapFrame: i32 = -1000;
let frame: i32 = 0;
export let scroll: i32 = 0;
let dragging: boolean = false;
let dragStartY: i32 = 0;
let dragStartScroll: i32 = 0;
let wasDown: boolean = false;
/** A drag scrolled the list during this contact: no row tap. */
let dragged: boolean = false;
export let testRunning: boolean = false;
let listedSeen: i32 = 0;
let listingSeen: string = "";

export function open(): void {
  current = ROOT;
  selected = -1;
  scroll = 0;
  entries = [];
  listing = listing + 1;
  listedSeen = device.sdListed;
  device.request(`sdList ${ROOT}`);
}

function parse(text: string): void {
  const codes = codePoints(text);
  const out: Entry[] = [];
  let i: i32 = 0;
  while (i < len(codes)) {
    let end: i32 = i;
    while (end < len(codes) && codes[end] !== 10) {
      end = end + 1;
    }
    if (end > i + 1) {
      const dir = codes[i] === 68;
      let size = "";
      let name = "";
      let k: i32 = i + 1;
      while (k < end && codes[k] !== 9) {
        size = size + fromCodePoint(codes[k]);
        k = k + 1;
      }
      for (let n: i32 = k + 1; n < end; n++) {
        name = name + fromCodePoint(codes[n]);
      }
      push(out, { dir: dir, name: name, size: size });
    }
    i = end + 1;
  }
  entries = out;
  listing = listing + 1;
}

/** One frame: a new listing from the board, the double-tap clock and the
 *  list drag. */
export function step(x: i32, y: i32, down: boolean): void {
  frame = frame + 1;
  // The listing text and its event can arrive in different frames: parse
  // on either.
  if (device.sdListed !== listedSeen || device.sdListing !== listingSeen) {
    listedSeen = device.sdListed;
    listingSeen = device.sdListing;
    parse(device.sdListing);
    selected = -1;
    scroll = 0;
  }
  if (down && !wasDown) {
    dragging = x >= 35 && x < 445 && y >= LIST_TOP && y < LIST_TOP + LIST_HEIGHT;
    dragged = false;
    dragStartY = y;
    dragStartScroll = scroll;
  } else if (down && dragging) {
    const dy = y - dragStartY;
    if (dragged || dy > 8 || dy < -8) {
      dragged = true;
      const rows = ready() ? len(entries) : 1;
      const content = rows * ROW_PITCH - 6;
      const max = content > LIST_HEIGHT ? content - LIST_HEIGHT : 0;
      let next = dragStartScroll - dy;
      next = next < 0 ? 0 : next > max ? max : next;
      scroll = next;
    }
  }
  wasDown = down;
}

export function ready(): boolean {
  return device.sdReady;
}

export function tapRow(index: i32): void {
  if (dragged || testRunning || !ready() || index >= len(entries)) {
    return;
  }
  if (index === lastTap && frame - lastTapFrame <= 12) {
    lastTap = -1;
    const entry = entries[index];
    if (entry.dir) {
      current = `${current}/${entry.name}`;
      device.request(`sdList ${current}`);
    }
    return;
  }
  lastTap = index;
  lastTapFrame = frame;
  selected = selected === index ? -1 : index;
}

export function back(): void {
  if (testRunning || !ready() || current === ROOT) {
    return;
  }
  const codes = codePoints(current);
  let cut: i32 = len(codes) - 1;
  while (cut > 0 && codes[cut] !== 47) {
    cut = cut - 1;
  }
  let next = "";
  for (let i: i32 = 0; i < cut; i++) {
    next = next + fromCodePoint(codes[i]);
  }
  current = len(codePoints(next)) < len(codePoints(ROOT)) ? ROOT : next;
  device.request(`sdList ${current}`);
}

/** Changes when rows() would: the view takes new rows only then. */
export function rowsKey(): string {
  return `${ready() ? 1 : 0}|${listing}|${selected}|${scroll}`;
}

export function rows(): Row[] {
  const out: Row[] = [];
  if (!ready()) {
    push(out, { id: 0, top: LIST_TOP - scroll, text: "Check L3B, card detect and mount.", bg: "#F8FAFC", border: "#CBD5E1", fg: "#64748B" });
    return out;
  }
  if (len(entries) === 0) {
    push(out, { id: 0, top: LIST_TOP - scroll, text: "(empty)", bg: "#F8FAFC", border: "#CBD5E1", fg: "#64748B" });
    return out;
  }
  for (let i: i32 = 0; i < len(entries); i++) {
    const entry = entries[i];
    const text = entry.dir ? `[D]  ${entry.name}` : `[F]  ${entry.name}  (${entry.size})`;
    const on = i === selected;
    push(out, {
      id: i,
      top: LIST_TOP + i * ROW_PITCH - scroll,
      text: dots(text, 384, 14),
      bg: on ? "#2563EB" : "#FFFFFF",
      border: on ? "#2563EB" : "#CBD5E1",
      fg: on ? "#FFFFFF" : entry.dir ? "#D97706" : "#111827",
    });
  }
  return out;
}

export function pathText(): string {
  if (!ready()) {
    return "/sdcard not ready";
  }
  return dots(device.sdPathError !== "" ? device.sdPathError : current, 230, 14);
}

export function sizeText(): string {
  if (!ready()) {
    return "";
  }
  if (selected >= 0 && selected < len(entries) && !entries[selected].dir) {
    return `File: ${entries[selected].size}`;
  }
  if (current === ROOT) {
    return "";
  }
  return `Folder: ${device.sdFolderSize}`;
}

export function backEnabled(): boolean {
  return ready() && !testRunning && current !== ROOT;
}

export function test(): void {
  if (testRunning) {
    return;
  }
  testRunning = true;
  dialog.show(OWNER, 0, "SD Speed Test", "Preparing SD speed test...", dialog.LARGE);
  dialog.button("Close", "#F3F4F6", "#111827", false);
  device.request("sdTest");
}

/** The board's progress text, and the end of the test. */
export function testText(text: string): void {
  if (dialog.open && dialog.owner === OWNER) {
    dialog.setText(text);
  }
}

export function testDone(): void {
  testRunning = false;
  device.request(`sdList ${current}`);
}
