// Pull-down status bar (factory apps/common/status_bar/status_bar.cpp): a
// 158 × 65 pill that springs from y = -85 to y = -17 (bounce 0.1, visual
// duration 0.3 s). A downward swipe that starts in the top 20 px shows it, a
// tap on it hides it, and it hides itself after 1.8 s the first time and
// after 6 s later.
import { abs, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import { SPRING_STATUS_BAR, springConfigure, springDone, springPos, springTeleport, springTo } from "./motion";

export let created: boolean = false;
export let hidden: boolean = true;
let firstShow: boolean = true;
let shownAt = 0.0;
let tracking: boolean = false;

export function create(silent: boolean): void {
  created = true;
  hidden = true;
  springConfigure(SPRING_STATUS_BAR, 0.3, 0.1);
  springTeleport(SPRING_STATUS_BAR, -85.0);
  firstShow = !silent;
  if (!silent) {
    show();
  }
}

export function destroy(): void {
  created = false;
  hidden = true;
  tracking = false;
  springTeleport(SPRING_STATUS_BAR, -85.0);
}

export function show(): void {
  hidden = false;
  shownAt = device.uptimeMs;
  springTo(SPRING_STATUS_BAR, -17.0);
}

export function hide(): void {
  hidden = true;
  springTo(SPRING_STATUS_BAR, -85.0);
}

export function update(): void {
  if (!created) {
    return;
  }
  if (input.pointerPressed) {
    tracking = input.pointerY <= 20 && input.pointerY >= 0;
  }
  if (input.pointerReleased && tracking) {
    tracking = false;
    const dy = input.pointerY - input.startY;
    const dx = abs(input.pointerX - input.startX);
    if (dy > 50 && dy > dx) {
      show();
    }
  }
  if (!hidden && device.uptimeMs - shownAt > (firstShow ? 1800.0 : 6000.0)) {
    firstShow = false;
    hide();
  }
}

/** Panel top in px; the panel is not drawn once it rests hidden. */
export function y(): i32 {
  return round(springPos[SPRING_STATUS_BAR]);
}

export function visible(): boolean {
  return created && !(hidden && springDone(SPRING_STATUS_BAR));
}
