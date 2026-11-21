# Decode the u8g2-format efont arrays of M5GFX (lgfx_efont_cn.c) for ASCII.
import re, sys

def c_string_bytes(text):
    out = bytearray()
    for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', text):
        i = 0
        while i < len(lit):
            c = lit[i]
            if c != '\\':
                out.append(ord(c)); i += 1; continue
            n = lit[i + 1]
            if n in '01234567':
                j = i + 1; digits = ''
                while j < len(lit) and len(digits) < 3 and lit[j] in '01234567':
                    digits += lit[j]; j += 1
                out.append(int(digits, 8)); i = j
            else:
                out.append({'n': 10, 't': 9, 'r': 13, '\\': 92, '"': 34, "'": 39, '?': 63}[n]); i += 2
    return bytes(out)

def load(path, name):
    src = open(path, encoding='latin-1').read()
    start = src.index('PROGMEM const uint8_t %s[' % name)
    body = src[src.index('=', start) + 1: src.index('";\n', start) + 1]
    return c_string_bytes(body)

class Bits:
    def __init__(self, data, pos):
        self.d = data; self.p = pos; self.b = 0
    def u(self, n):
        val = self.d[self.p] >> self.b
        s = 8 - self.b
        self.b += n
        if self.b >= 8:
            self.b -= 8; self.p += 1
            val |= self.d[self.p] << s
        return val & ((1 << n) - 1)
    def s(self, n):
        return self.u(n) - (1 << (n - 1))

def decode(data):
    h = data[:23]
    info = dict(cnt=h[0], b0=h[2], b1=h[3], bw=h[4], bh=h[5], bx=h[6], by=h[7], bd=h[8],
                maxw=h[9], maxh=h[10], xoff=(h[11] ^ 0x80) - 0x80, yoff=(h[12] ^ 0x80) - 0x80,
                ascA=(h[13] ^ 0x80) - 0x80, descg=(h[14] ^ 0x80) - 0x80)
    glyphs = {}
    p = 23
    while True:
        enc, nxt = data[p], data[p + 1]
        if nxt == 0:
            break
        bits = Bits(data, p + 2)
        w = bits.u(info['bw']); hh = bits.u(info['bh'])
        x = bits.s(info['bx']); y = bits.s(info['by']); dx = bits.s(info['bd'])
        rows = [[0] * w for _ in range(hh)]
        if hh > 0 and w > 0:
            cx = cy = 0
            def run(cnt, v):
                nonlocal cx, cy
                while cnt > 0:
                    take = min(cnt, w - cx)
                    if v:
                        for k in range(cx, cx + take):
                            rows[cy][k] = 1
                    cx += take; cnt -= take
                    if cx >= w:
                        cx = 0; cy += 1
            while True:
                a = bits.u(info['b0']); b = bits.u(info['b1'])
                while True:
                    run(a, 0); run(b, 1)
                    if bits.u(1) == 0:
                        break
                if cy >= hh:
                    break
        glyphs[enc] = dict(w=w, h=hh, x=x, y=y, dx=dx, rows=rows)
        p += nxt
    return info, glyphs

if __name__ == '__main__':
    info, g = decode(load(sys.argv[1], sys.argv[2]))
    print(info, len(g))
    for ch in 'Ag':
        gl = g[ord(ch)]
        print(ch, {k: gl[k] for k in 'whxyd' if k in gl}, gl['dx'])
        for r in gl['rows']:
            print(''.join('#' if v else '.' for v in r))
