// Badge app (factory app_badge), device side: the badge picture area with
// the "Tap and hold" hint, A/B slot selection, the long-press dialog and the
// edit screen that names the access point and the editor URL. The board
// owns the slots, the Wi-Fi access point and the HTTP editor.
import { type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";

export let dialog: boolean = false;
export let editing: boolean = false;
let pressArmed: boolean = false;
export let suppressTap: boolean = false;

export function open(): void {
  dialog = false;
  editing = false;
}

function step(direction: i32): void {
  // Next occupied slot in the direction, wrapping (factory loadNext/Previous).
  for (let n = 1; n <= 6; n++) {
    const slot = (((device.badgeActive + direction * n) % 6) + 6) % 6;
    if (device.badgeSlots[slot]) {
      device.requestBadge(slot);
      return;
    }
  }
}

export function startEdit(): void {
  dialog = false;
  editing = true;
  device.setBadgeEditing(true);
}

export function cancel(): void {
  dialog = false;
}

export function message(): string {
  if (device.apMessage !== "") {
    return device.apMessage;
  }
  return `Connect to Wi-Fi: ${device.apSsid}\nAnd open page:\nhttp://192.168.4.1`;
}

export function update(): boolean {
  if (input.keyEvent === input.KEY_HOME) {
    if (editing) {
      device.setBadgeEditing(false);
    }
    return true;
  }
  if (editing || dialog) {
    return false;
  }
  if (input.keyEvent === input.KEY_PREVIOUS) {
    step(-1);
  } else if (input.keyEvent === input.KEY_NEXT) {
    step(1);
  }
  if (input.pointerPressed) {
    pressArmed = true;
    suppressTap = false;
  }
  if (input.pointerDown && pressArmed && !input.moved && device.uptimeMs - input.startMs >= 400.0) {
    pressArmed = false;
    dialog = true;
    suppressTap = true;
  }
  if (input.pointerReleased) {
    pressArmed = false;
  }
  return false;
}
