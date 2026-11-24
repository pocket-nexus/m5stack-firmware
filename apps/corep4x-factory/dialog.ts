// corep4_ui_open_function_dialog: an lv_msgbox on the top layer, 460 px wide
// and 176, 248 or 328 px high, centred, over a backdrop of 0x111827 at 50 %.
// Its title sits in a 43 px grey header, the text (Montserrat 16, 420 px,
// wrapped) in the content area and up to three 116x40 buttons, spaced
// evenly, in a 43 px footer. Opening a dialog closes the previous one. The
// owner (a window module) gets the index of the pressed button.
import { clear, len, push, type Color, type i32 } from "@pocketjs/framework/solid/std";

export const SMALL: i32 = 0;
export const MEDIUM: i32 = 1;
export const LARGE: i32 = 2;

export interface DialogButton {
  id: i32;
  text: string;
  bg: Color;
  fg: Color;
  hidden: boolean;
}

/** A visible footer button at its x inside the box. */
export interface PlacedButton {
  id: i32;
  x: i32;
  text: string;
  bg: Color;
  fg: Color;
}

export let open: boolean = false;
/** The module the buttons report to, and a value it chose. */
export let owner: i32 = -1;
export let kind: i32 = 0;
export let title: string = "";
export let text: string = "";
export let size: i32 = MEDIUM;
export let buttons: DialogButton[] = [];
/** Bumped on every change, so the root model republishes. */
export let revision: i32 = 0;

export function show(ownerId: i32, dialogKind: i32, dialogTitle: string, message: string, dialogSize: i32): void {
  open = true;
  owner = ownerId;
  kind = dialogKind;
  title = dialogTitle;
  text = message;
  size = dialogSize;
  clear(buttons);
  revision = revision + 1;
}

export function button(label: string, bg: Color, fg: Color, hidden: boolean): void {
  push(buttons, { id: len(buttons), text: label, bg: bg, fg: fg, hidden: hidden });
  revision = revision + 1;
}

export function setText(message: string): void {
  text = message;
  revision = revision + 1;
}

export function setHidden(index: i32, hidden: boolean): void {
  const next: DialogButton[] = [];
  for (let i: i32 = 0; i < len(buttons); i++) {
    const b = buttons[i];
    push(next, { id: b.id, text: b.text, bg: b.bg, fg: b.fg, hidden: i === index ? hidden : b.hidden });
  }
  buttons = next;
  revision = revision + 1;
}

export function close(): void {
  open = false;
  owner = -1;
  revision = revision + 1;
}

/** Height of the box for a size. */
export function height(dialogSize: i32): i32 {
  return dialogSize === SMALL ? 176 : dialogSize === LARGE ? 328 : 248;
}
