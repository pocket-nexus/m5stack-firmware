# efont to TrueType

`build_efont.py` builds the text faces of `apps/atoms3-factory` from the
efont bitmap data that M5GFX compiles into the AtomS3 factory demo.

| Output | Source array | Use |
| --- | --- | --- |
| `fonts/EfontBiwidth12-Regular.ttf` | `lgfx_efont_cn_12` | `mono` face, 12 px |
| `fonts/EfontBiwidth16-Regular.ttf` | `lgfx_efont_cn_16` | `regular` face, 16 px |
| `fonts/EfontBiwidth16-Bold.ttf` | `lgfx_efont_cn_16_b` | `bold` face, 16 px |
| `img/not_found.png` | `lgfx_efont_cn_24` | "Not found", 24 px, red |
| `img/top5_list.png` | `lgfx_efont_cn_14` | "Top 5 list:", 14 px, white |

Each glyph outline is one square per lit pixel, scaled so a square covers one
screen pixel at the face's size; the PocketJS baker draws every pixel fully on
or off at that size. An app has three faces (`regular`, `bold`, `mono`), so the
14 px and 24 px strings are images. Only the ASCII glyphs the arrays contain are
converted.

## Inputs

- `src/lgfx/Fonts/efont/lgfx_efont_cn.c` of
  [M5GFX](https://github.com/m5stack/M5GFX) tag `0.2.19`
  (SHA-256 `ceb7bae98279b5b196a4742b0814970d095ddcaabea8a9ac3cebe1e1f7add873`)
- Python 3 with the packages in `requirements.txt`

## Run

```sh
python3 -m venv .pocket-build/efont-venv
.pocket-build/efont-venv/bin/pip install -r tools/fonts/efont/requirements.txt
.pocket-build/efont-venv/bin/python tools/fonts/efont/build_efont.py \
    <M5GFX>/src/lgfx/Fonts/efont/lgfx_efont_cn.c \
    apps/atoms3-factory/fonts apps/atoms3-factory/img
```

The head table timestamps are fixed, so the run reproduces the committed files
byte for byte. The efont licence (`apps/atoms3-factory/fonts/efont-COPYRIGHT.txt`)
applies to the outputs.
