// A minimal 8-bit PNG encoder for the asset converter and the preview tool.
import { deflateSync } from "node:zlib";

export interface Picture {
  width: number;
  height: number;
  rgba: Uint8Array;
}

const CRC_TABLE = Array.from({ length: 256 }, (_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});

function crc32(bytes: Uint8Array): number {
  let c = 0xffffffff;
  for (const byte of bytes) c = CRC_TABLE[(c ^ byte) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function chunk(type: string, data: Uint8Array): Uint8Array {
  const out = new Uint8Array(12 + data.length);
  const view = new DataView(out.buffer);
  view.setUint32(0, data.length);
  out.set(new TextEncoder().encode(type), 4);
  out.set(data, 8);
  view.setUint32(8 + data.length, crc32(out.subarray(4, 8 + data.length)));
  return out;
}

/** An 8-bit RGBA PNG (RGB when every pixel is opaque). */
export function encodePng(picture: Picture): Uint8Array {
  const opaque = picture.rgba.every((value, i) => i % 4 !== 3 || value === 255);
  const channels = opaque ? 3 : 4;
  const stride = picture.width * channels;
  const raw = new Uint8Array((stride + 1) * picture.height);
  for (let y = 0; y < picture.height; y++) {
    raw[y * (stride + 1)] = 0;
    for (let x = 0; x < picture.width; x++) {
      const from = (y * picture.width + x) * 4;
      raw.set(picture.rgba.subarray(from, from + channels), y * (stride + 1) + 1 + x * channels);
    }
  }
  const header = new Uint8Array(13);
  const view = new DataView(header.buffer);
  view.setUint32(0, picture.width);
  view.setUint32(4, picture.height);
  header[8] = 8;
  header[9] = opaque ? 2 : 6;
  const signature = Uint8Array.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
  const parts = [signature, chunk("IHDR", header), chunk("IDAT", deflateSync(raw, { level: 9 })), chunk("IEND", new Uint8Array())];
  const png = new Uint8Array(parts.reduce((total, part) => total + part.length, 0));
  let offset = 0;
  for (const part of parts) {
    png.set(part, offset);
    offset += part.length;
  }
  return png;
}
