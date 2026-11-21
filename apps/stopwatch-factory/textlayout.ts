// Layout of text drawn with the factory glyph images (glyphs.ts): the
// codepoint of every character and its pen position.
import { codePoints, idiv, len, push, type i32 } from "@pocketjs/framework/solid/std";
import { advance } from "./glyphs";

/** Codepoints of `text`, one per glyph slot. */
export function ids(text: string): i32[] {
  return codePoints(text);
}

/** Width of `text` in px. */
export function width(text: string, codes: i32[], advances: i32[]): i32 {
  let total = 0;
  for (const cp of codePoints(text)) {
    total = total + advance(codes, advances, cp);
  }
  return total;
}

/** Pen x of every character, starting at `x`. */
export function pens(text: string, codes: i32[], advances: i32[], x: i32): i32[] {
  const out: i32[] = [];
  let pen = x;
  for (const cp of codePoints(text)) {
    push(out, pen);
    pen = pen + advance(codes, advances, cp);
  }
  return out;
}

/** Pen x of every character of `text` centred on `centre`. */
export function centred(text: string, codes: i32[], advances: i32[], centre: i32): i32[] {
  const w = width(text, codes, advances);
  return pens(text, codes, advances, centre - idiv(w, 2));
}

export function count(text: string): i32 {
  return len(codePoints(text));
}
