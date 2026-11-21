# Builds the AtomS3 factory text faces from the efont bitmap data of M5GFX.
#
#   python3 tools/fonts/efont/build_efont.py <lgfx_efont_cn.c> <fonts dir> <images dir>
#
# <lgfx_efont_cn.c> is src/lgfx/Fonts/efont/lgfx_efont_cn.c of M5GFX 0.2.19.
# Each TrueType face has one square outline per lit pixel, scaled so that a
# square is one screen pixel at the face's size. Strings drawn in a fourth or
# fifth efont size become RGB images. The head table timestamps are fixed so
# the output is byte-identical between runs.
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import efont  # noqa: E402
from fontTools.fontBuilder import FontBuilder  # noqa: E402
from fontTools.pens.ttGlyphPen import TTGlyphPen  # noqa: E402
from PIL import Image  # noqa: E402

if len(sys.argv) != 4:
    sys.exit("usage: build_efont.py <lgfx_efont_cn.c> <fonts dir> <images dir>")
SRC, FONTS_OUT, IMAGES_OUT = sys.argv[1], sys.argv[2], sys.argv[3]
COPYRIGHT = ("(c) Copyright 2000-2003 /efont/ The Electronic Font Open Laboratory. "
             "Outlines converted from the u8g2 data in M5GFX 0.2.19 (lgfx_efont_cn.c).")
# head.created and head.modified, seconds since 1904-01-01 (the value of the
# first committed build).
HEAD_TIMESTAMP = 3878079658


def build_ttf(array, px, family, style, filename):
    info, glyphs = efont.decode(efont.load(SRC, array))
    unit = 64 if px == 16 else 100
    upm = px * unit
    ascent = info['maxh'] + info['yoff']
    descent = -info['yoff']
    order = ['.notdef']
    cmap = {}
    shapes = {}
    metrics = {}
    for enc in sorted(glyphs):
        name = 'uni%04X' % enc
        order.append(name)
        cmap[enc] = name
        g = glyphs[enc]
        pen = TTGlyphPen(None)
        for r, row in enumerate(g['rows']):
            top = (g['h'] + g['y'] - r) * unit
            bottom = top - unit
            c = 0
            while c < g['w']:
                if not row[c]:
                    c += 1
                    continue
                start = c
                while c < g['w'] and row[c]:
                    c += 1
                x0 = (g['x'] + start) * unit
                x1 = (g['x'] + c) * unit
                pen.moveTo((x0, top))
                pen.lineTo((x1, top))
                pen.lineTo((x1, bottom))
                pen.lineTo((x0, bottom))
                pen.closePath()
        shapes[name] = pen.glyph()
        metrics[name] = (g['dx'] * unit, g['x'] * unit if g['w'] else 0)
    empty = TTGlyphPen(None)
    shapes['.notdef'] = empty.glyph()
    metrics['.notdef'] = (px // 2 * unit, 0)
    fb = FontBuilder(upm, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(shapes)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ascent * unit, descent=-descent * unit)
    fb.setupNameTable({'familyName': family, 'styleName': style, 'copyright': COPYRIGHT,
                       'version': 'Version 0.4.2',
                       'licenseDescription': 'BSD-style /efont/ licence, see efont-COPYRIGHT.txt'})
    fb.setupOS2(sTypoAscender=ascent * unit, sTypoDescender=-descent * unit, sTypoLineGap=0,
                usWinAscent=ascent * unit, usWinDescent=descent * unit,
                usWeightClass=700 if style == 'Bold' else 400)
    fb.setupPost()
    fb.font['head'].created = HEAD_TIMESTAMP
    fb.font['head'].modified = HEAD_TIMESTAMP
    fb.font.recalcTimestamp = False
    fb.save(os.path.join(FONTS_OUT, filename))
    print(filename, len(order), 'glyphs, ascent', ascent, 'descent', descent)


def text_image(array, text, color, width, height, top_offset, center_x, filename):
    info, glyphs = efont.decode(efont.load(SRC, array))
    img = Image.new('RGB', (width, height), (0, 0, 0))
    total = sum(glyphs[ord(c)]['dx'] for c in text)
    pen_x = center_x - total // 2 if center_x is not None else 0
    baseline = top_offset + info['maxh'] + info['yoff']
    for c in text:
        g = glyphs[ord(c)]
        for r, row in enumerate(g['rows']):
            for col, v in enumerate(row):
                if v:
                    img.putpixel((pen_x + g['x'] + col, baseline - (g['h'] + g['y']) + r), color)
        pen_x += g['dx']
    img.save(os.path.join(IMAGES_OUT, filename))
    print(filename, 'width', total)


build_ttf('lgfx_efont_cn_12', 12, 'Efont Biwidth 12px', 'Regular', 'EfontBiwidth12-Regular.ttf')
build_ttf('lgfx_efont_cn_16', 16, 'Efont Biwidth 16px', 'Regular', 'EfontBiwidth16-Regular.ttf')
build_ttf('lgfx_efont_cn_16_b', 16, 'Efont Biwidth 16px', 'Bold', 'EfontBiwidth16-Bold.ttf')
# func_i2c_t: drawCenterString("Not found", 64, 49 - 16) in efontCN_24, TFT_RED.
text_image('lgfx_efont_cn_24', 'Not found', (255, 0, 0), 128, 32, 0, 64, 'not_found.png')
# func_wifi_t: printf("Top 5 list:") in efontCN_14 at the line's top-left.
text_image('lgfx_efont_cn_14', 'Top 5 list:', (255, 255, 255), 128, 16, 0, None, 'top5_list.png')
