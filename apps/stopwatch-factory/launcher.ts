// Launcher carousel (factory app_launcher/view/view.cpp): one 466 px page per
// tool, endless in both directions, finger-following drag with a snap to the
// nearest page (at most one page per throw, LV_OBJ_FLAG_SCROLL_ONE), page
// dots and a title that fades with the distance from the page centre.
import { abs, clamp, f64, floor, round, type i32 } from "@pocketjs/framework/solid/std";
import * as device from "./device";
import * as input from "./input";
import { lvEaseOut } from "./motion";

export const PAGE: i32 = 466;
export const APP_COUNT: i32 = 9;

/** Scroll offset in px; page i is centred at i * PAGE. */
export let scroll: f64 = 0.0;
let dragging: boolean = false;
let dragOrigin: f64 = 0.0;
let pressPage: i32 = 0;
let animating: boolean = false;
let animFrom: f64 = 0.0;
let animTo: f64 = 0.0;
let animStart: f64 = 0.0;
let animDuration: f64 = 300.0;
/** Suppresses the tap the guest reports for a contact that dragged. */
export let suppressTap: boolean = false;

/** Index of the page nearest to the centre, 0..APP_COUNT-1. */
export function currentIndex(): i32 {
  const page = floor((scroll + f64(PAGE) / 2.0) / f64(PAGE));
  return ((page % APP_COUNT) + APP_COUNT) % APP_COUNT;
}

/** Signed distance in px from the nearest page centre. */
export function distanceToCentre(): f64 {
  const page = floor((scroll + f64(PAGE) / 2.0) / f64(PAGE));
  return scroll - f64(page * PAGE);
}

/** Horizontal offset of icon `index` from the screen centre. */
export function iconOffset(index: i32): i32 {
  const width: f64 = f64(PAGE * APP_COUNT);
  let offset: f64 = f64(index * PAGE) - scroll;
  offset = offset - width * f64(floor((offset + width / 2.0) / width));
  return round(offset);
}

/** Title opacity in percent: factory DynamicIconLabel (range 150, fade 80). */
export function titleOpacity(): i32 {
  const distance = abs(distanceToCentre());
  if (distance > 150.0) {
    return 0;
  }
  if (distance > 70.0) {
    return round((1.0 - (distance - 70.0) / 80.0) * 100.0);
  }
  return 100;
}

export function jumpTo(index: i32): void {
  scroll = f64(index * PAGE);
  animating = false;
}

function animateTo(target: f64): void {
  animFrom = scroll;
  animTo = target;
  animStart = device.uptimeMs;
  // LVGL clamps scroll animations between 200 and 400 ms by distance.
  animDuration = clamp(abs(target - scroll) * 0.8, 200.0, 400.0);
  animating = true;
}

/** Scrolls one page left (-1) or right (+1). */
export function scrollBy(direction: i32): void {
  const page = floor((scroll + f64(PAGE) / 2.0) / f64(PAGE));
  animateTo(f64((page + direction) * PAGE));
}

/** One frame of drag, throw and snap handling. */
export function update(): void {
  if (input.pointerPressed) {
    suppressTap = false;
    dragOrigin = scroll;
    pressPage = floor((scroll + f64(PAGE) / 2.0) / f64(PAGE));
    dragging = false;
  }
  if (input.pointerDown) {
    const dx = input.pointerX - input.startX;
    const dy = input.pointerY - input.startY;
    if (!dragging && abs(dx) > 10 && abs(dx) > abs(dy) && input.startY > 20) {
      dragging = true;
      animating = false;
      suppressTap = true;
      dragOrigin = scroll + f64(dx > 0 ? 10 : -10);
    }
    if (dragging) {
      scroll = dragOrigin - f64(dx);
    }
  }
  if (input.pointerReleased && dragging) {
    dragging = false;
    // Throw: LVGL predicts the glide from the release velocity with 10 %
    // friction per 33 ms sample, then snaps to a page centre one page away
    // from the pressed page at most.
    const glide = -input.velocityX * 33.0 * 9.0;
    let page = floor((scroll + glide + f64(PAGE) / 2.0) / f64(PAGE));
    page = clamp(page, pressPage - 1, pressPage + 1);
    animateTo(f64(page * PAGE));
  }
  if (animating) {
    const t = (device.uptimeMs - animStart) / animDuration;
    if (t >= 1.0) {
      scroll = animTo;
      animating = false;
    } else {
      scroll = animFrom + (animTo - animFrom) * lvEaseOut(t);
    }
  }
  // Keep the offset bounded; the carousel is endless.
  const width: f64 = f64(PAGE * APP_COUNT);
  if (!dragging && !animating && (scroll > width * 4.0 || scroll < -width * 4.0)) {
    scroll = scroll - width * f64(floor(scroll / width));
  }
}

export function busy(): boolean {
  return dragging || animating;
}
