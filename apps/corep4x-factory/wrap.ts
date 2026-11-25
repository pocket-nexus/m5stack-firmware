// LVGL label wrapping (lv_text_get_next_line with LV_LABEL_LONG_WRAP): text
// breaks after a space or one of ",.;:-_)]}" when the next word would pass
// the label width; a word wider than the label breaks between characters.
// PocketJS draws "\n" as a line break but does not wrap by itself, so the
// models wrap strings with these Montserrat Medium advances (thousandths of
// an em for code points 32..126, from fonts/Montserrat-Medium.ttf).
import { codePoints, f32, fromCodePoint, i32, imod, len, push } from "@pocketjs/framework/solid/std";

const ADVANCE: i32[] = [
  269, 268, 391, 703, 621, 843, 686, 210, 337, 338, 400, 582, 227, 383, 227, 352, 667, 370, 574, 572, 669, 574, 617, 598, 644,
  617, 227, 227, 582, 582, 582, 573, 1034, 732, 757, 713, 826, 670, 635, 772, 812, 310, 513, 719, 594, 955, 812, 840, 722, 840,
  727, 621, 587, 791, 712, 1126, 673, 647, 657, 333, 352, 333, 583, 500, 600, 598, 682, 571, 682, 612, 353, 690, 681, 279, 284,
  615, 279, 1057, 681, 635, 682, 682, 410, 501, 414, 677, 559, 899, 552, 559, 521, 351, 299, 351, 582,
];

function advance(code: i32, size: i32): f32 {
  const index: i32 = code >= 32 && code < 127 ? code - 32 : 0;
  return (f32(ADVANCE[index]) * f32(size)) / f32(1000);
}

/** Width in px of one line of text at a Montserrat size. */
export function width(text: string, size: i32): f32 {
  const codes = codePoints(text);
  let sum: f32 = 0;
  for (let i: i32 = 0; i < len(codes); i++) {
    sum = sum + advance(codes[i], size);
  }
  return sum;
}

function breaks(code: i32): boolean {
  // " ,.;:-_)]}"
  return code === 32 || code === 44 || code === 46 || code === 59 || code === 58 || code === 45 || code === 95 || code === 41 || code === 93 || code === 125;
}

// The models ask for the same strings in every frame: the last results,
// replaced in turn.
const CACHE: i32 = 32;
let cacheText: string[] = [];
let cacheMax: i32[] = [];
let cacheSize: i32[] = [];
let cacheDots: boolean[] = [];
let cacheResult: string[] = [];
let cacheNext: i32 = 0;

function cached(text: string, max: i32, size: i32, dotted: boolean): i32 {
  for (let i: i32 = 0; i < len(cacheText); i++) {
    if (cacheText[i] === text && cacheMax[i] === max && cacheSize[i] === size && cacheDots[i] === dotted) {
      return i;
    }
  }
  return -1;
}

function remember(text: string, max: i32, size: i32, dotted: boolean, result: string): string {
  if (len(cacheText) < CACHE) {
    push(cacheText, text);
    push(cacheMax, max);
    push(cacheSize, size);
    push(cacheDots, dotted);
    push(cacheResult, result);
  } else {
    cacheText[cacheNext] = text;
    cacheMax[cacheNext] = max;
    cacheSize[cacheNext] = size;
    cacheDots[cacheNext] = dotted;
    cacheResult[cacheNext] = result;
    cacheNext = imod(cacheNext + 1, CACHE);
  }
  return result;
}

/** The text with "\n" inserted where an LVGL label of `max` px wraps it. */
export function wrap(text: string, max: i32, size: i32): string {
  const hit = cached(text, max, size, false);
  if (hit >= 0) {
    return cacheResult[hit];
  }
  return remember(text, max, size, false, wrapped(text, max, size));
}

function wrapped(text: string, max: i32, size: i32): string {
  const codes = codePoints(text);
  const out: i32[] = [];
  let lineWidth: f32 = 0;
  let i: i32 = 0;
  const limit: f32 = f32(max);
  while (i < len(codes)) {
    const code = codes[i];
    if (code === 10) {
      push(out, 10);
      lineWidth = 0;
      i = i + 1;
      continue;
    }
    // The next word: up to and including a break character.
    let end: i32 = i;
    let wordWidth: f32 = 0;
    while (end < len(codes) && codes[end] !== 10) {
      wordWidth = wordWidth + advance(codes[end], size);
      end = end + 1;
      if (breaks(codes[end - 1])) {
        break;
      }
    }
    // A trailing space does not count against the width.
    const trailing: f32 = codes[end - 1] === 32 ? advance(32, size) : f32(0);
    if (lineWidth > 0 && lineWidth + wordWidth - trailing > limit) {
      push(out, 10);
      lineWidth = 0;
    }
    if (wordWidth - trailing > limit) {
      // Too long for any line: break between characters.
      for (let k: i32 = i; k < end; k++) {
        const w = advance(codes[k], size);
        if (lineWidth > 0 && lineWidth + w > limit) {
          push(out, 10);
          lineWidth = 0;
        }
        push(out, codes[k]);
        lineWidth = lineWidth + w;
      }
    } else {
      for (let k: i32 = i; k < end; k++) {
        push(out, codes[k]);
      }
      lineWidth = lineWidth + wordWidth;
    }
    i = end;
  }
  let result = "";
  for (let k: i32 = 0; k < len(out); k++) {
    result = result + fromCodePoint(out[k]);
  }
  return result;
}

/** LV_LABEL_LONG_DOT on a one-line label of `max` px: the text, or its
 *  longest prefix that fits followed by "...". */
export function dots(text: string, max: i32, size: i32): string {
  const hit = cached(text, max, size, true);
  if (hit >= 0) {
    return cacheResult[hit];
  }
  return remember(text, max, size, true, dotted(text, max, size));
}

function dotted(text: string, max: i32, size: i32): string {
  if (width(text, size) <= f32(max)) {
    return text;
  }
  const codes = codePoints(text);
  const room: f32 = f32(max) - advance(46, size) * f32(3);
  let used: f32 = 0;
  let result = "";
  for (let i: i32 = 0; i < len(codes); i++) {
    const w = advance(codes[i], size);
    if (used + w > room) {
      break;
    }
    used = used + w;
    result = result + fromCodePoint(codes[i]);
  }
  return result + "...";
}
