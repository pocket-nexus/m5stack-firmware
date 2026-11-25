# Fonts

**An application chooses its regular, bold and monospace faces with
`app.fonts` in its `pocket.host.config.ts`. `tools/pocket compile` bakes the
font atlases from those faces in JS mode and in AOT mode. Text sizes stay
those of the slot table of the pinned PocketJS (10, 12, 14, 16, 18, 20, 24,
36 and 54 logical px); other sizes are drawn as images.**

Without `app.fonts` PocketJS bakes Inter Regular, Inter Bold and JetBrains
Mono Regular (`third_party/pocketjs/assets/fonts/`).

## Declare the faces

```text
apps/stopwatch-factory/
├─ pocket.host.config.ts
└─ fonts/
   ├─ Montserrat-Medium.ttf
   ├─ Montserrat-SemiBold.ttf
   ├─ Montserrat-OFL.txt
   ├─ MapleMono-Medium.ttf
   └─ MapleMono-OFL.txt
```

```ts
export default defineHostConfig({
  host: "../../programs/stopwatch/pocket.host.json",
  app: {
    // ...
    fonts: {
      regular: "fonts/Montserrat-Medium.ttf",
      bold: "fonts/Montserrat-SemiBold.ttf",
      mono: "fonts/MapleMono-Medium.ttf",
    },
  },
});
```

| Field | Face of | Default |
| --- | --- | --- |
| `regular` | text without `font-bold` or `font-mono` | Inter Regular |
| `bold` | text with `font-bold` | Inter Bold |
| `mono` | text with `font-mono` | JetBrains Mono Regular |

- Paths are relative to the host config, name a `.ttf` or `.otf` and hold no
  `..` segment: the faces sit in the application directory.
- Every field is optional; an omitted field keeps the default face. Other
  fields are rejected.
- The utilities select a role, not a weight: there is no `font-medium` or
  `font-semibold`. A factory that uses Medium and SemiBold declares Medium as
  `regular` and SemiBold as `bold`.
- Use static instances. The baker (opentype.js) draws a variable font
  (`Montserrat[wght].ttf`) at its default instance only.
- PocketJS's own `fonts.json` beside the entry still applies: `fallback` faces
  supply codepoints the slot's face does not map (icons), `characters`,
  `characterFiles` and `ranges` add codepoints to every atlas.

### Faces of the applications

| Board | Factory fonts | Declaration |
| --- | --- | --- |
| StopWatch | Montserrat Medium and SemiBold, Maple Mono Medium 28/48 px, Commissioner Medium 64/108 px | `regular` Montserrat Medium, `bold` Montserrat SemiBold; Maple Mono and Commissioner sizes are outside the table ([sizes](#sizes-outside-the-table)) |
| Tab5 | Montserrat (LVGL) 16 to 36 physical px | `regular`/`bold` Montserrat; raster density 2, so a factory size of N physical px is the `N/2` logical px slot |
| StickS3 (demos, no factory font) | — | `regular` Montserrat Medium, `bold` Montserrat SemiBold |
| CoreP4X | Montserrat (LVGL) 10 to 44 px | `regular` Montserrat Medium, `bold` Montserrat SemiBold; the 28 and 44 px texts use the 24 and 36 px slots (`apps/corep4x-factory/README.md`) |
| AtomS3 | M5GFX bitmap fonts: efont 12/14/16/24 px and 16 px bold (`efontCN_*`, `efontCN_16_b`), GLCD `Font0` 6×8 | Pixel-outline TrueType converted from M5GFX's efont data: `regular` efont 16, `bold` efont 16 bold, `mono` efont 12; the 14 and 24 px strings are images (`apps/atoms3-factory/README.md`) |

## Licensing

Font files are committed in the application, `apps/<board>-factory/fonts/`
(StickS3: `apps/sticks3-demos/fonts/`), each family with its licence text
beside it (`<Family>-OFL.txt`, `<Family>-LICENSE.txt`). The application
README lists each face with its version or commit, source URL and licence.

| Family | Licence | Source |
| --- | --- | --- |
| Montserrat | SIL OFL 1.1 | `github.com/JulietaUla/Montserrat` (`fonts/ttf/`) |
| Maple Mono | SIL OFL 1.1 | `github.com/subframe7536/maple-font` (release asset `MapleMono-TTF.zip`) |
| Commissioner | SIL OFL 1.1 | `github.com/kosbarts/Commissioner` (`fonts/ttf/`) |
| Inter, JetBrains Mono | SIL OFL 1.1 | in the submodule, not copied |

- OFL 1.1 and Apache 2.0 permit redistribution with software and embedding in
  firmware. OFL requires the copyright notice and licence text with every copy
  of the font files; Apache 2.0 requires the licence and any `NOTICE` file.
- Commit the font files unmodified, under their original names. A subset or
  otherwise modified OFL font is a Modified Version and may not keep a
  Reserved Font Name. The build reads only the glyphs it needs, so subsetting
  is not necessary.
- Do not commit fonts without a licence that permits redistribution (system
  fonts, commercial fonts).

## How the faces are applied

`tools/pocket` runs the Pocket CLI of the submodule with Bun
(`third_party/pocketjs/tools/pocket.ts`). The CLI reads `app.fonts` into the
build plan, and both compiles bake from it:

| Mode | Atlases |
| --- | --- |
| JS bundle | `tools/build.ts` bakes the pak's font entries from the plan's faces |
| AOT (`--aot`) | `tools/aot-compile.ts` bakes `font-<slot>.bin` from them; the same atlases feed the MicroTS layout oracle, so baked layout uses the declared faces' metrics |

The depfile of a compile lists the declared faces, so a changed face
rebuilds.

### Submodule bumps

After bumping `third_party/pocketjs`, check that the slot table
(`framework/compiler/tailwind.ts`, `MAX_FONT_SLOTS` in
`contracts/spec/spec.ts`) is unchanged; otherwise update the tables below.
Then run `bun tests/fonts/check.ts` and `bun tests/fonts/atlas-cost.ts`.

## Sizes

A font slot is a (px, weight, family) triple pinned in
`framework/compiler/tailwind.ts`. A build bakes one atlas per slot its class
strings use, plus slot 2 (16 px regular, the default style), which is always
baked.

| Utility | px | Regular slot | `font-bold` slot | `font-mono` slot |
| --- | --- | --- | --- | --- |
| `text-2xs` | 10 | 19 | 20 | none |
| `text-xs` | 12 | 0 | 7 | 16 |
| `text-sm` | 14 | 1 | 8 | 17 |
| `text-base` (default) | 16 | 2 | 9 | 18 |
| `text-lg` | 18 | 3 | 10 | none |
| `text-xl` | 20 | 4 | 11 | none |
| `text-2xl` | 24 | 5 | 12 | none |
| `text-4xl` | 36 | 6 | 13 | none |
| `text-5xl` | 54 | 14 | 15 | none |

Sizes are logical px; the atlas is rasterised at the host contract's
`display.rasterDensity` (Tab5: 2; StopWatch, StickS3, AtomS3: 1).

Limits (PocketJS v0.17.0 to the pinned `4f0dabf5`, same slot table):

- **No arbitrary text sizes.** `text-[108px]`, `text-[64px]`, `text-[16px]`,
  `text-3xl` and `text-6xl` are unknown utilities: the class string does not
  compile (a warning, an error with `--strict-classes`) and the node gets none
  of its styles. Spacing utilities accept `[Npx]`; text size does not.
- `font-mono` exists at 12, 14 and 16 px only; `font-mono` with another size
  is a compile error. `font-bold font-mono` uses the mono slot (no bold mono).
- Three faces per application, one per role, used for every size of that
  role, plus `fonts.json` fallback faces for codepoints those faces lack.
  A fourth family at a table size (Commissioner beside Montserrat) is not
  possible.
- 21 of the 24 slots (`MAX_FONT_SLOTS`) are assigned. New sizes need an
  upstream change of the slot table; this repository does not change it.

### Sizes outside the table

Draw them as images. For digits, which is what the factories show at these
sizes, the image set is small:

| Text | Image set | Pixels | RGBA8888 | RGBA4444 |
| --- | --- | --- | --- | --- |
| Commissioner Medium 64 px | digits 0–9, ink boxes 21–37 × 44–46 | 14,546 | 58,184 B | 29,092 B |
| Commissioner Medium 108 px | digits 0–9, ink boxes 34–61 × 75–78 | 41,134 | 164,536 B | 82,268 B |

For comparison, an atlas at those sizes (if a slot existed) at density 1:
Commissioner 108 px with the ASCII charset every atlas carries is 1,392,496 B
(cell 109×133), with only `0-9:.` 114,234 B; 64 px ASCII is 493,744 B.

- Render the PNGs from the declared TTF with the PocketJS rasteriser
  (`bakeSlot` in `framework/compiler/bake-font.ts` accepts any px; its
  coverage cells are the alpha channel) or commit PNGs rendered once, with
  the font's licence beside them.
- Text colour does not apply to an image: a digit shown in several colours
  needs one image per colour, or a white image under a colour the image
  pipeline applies. Images are `PSM_8888` unless `images.json` gives
  `psm: 2` (`PSM_4444`); AOT bakes the same format.
- Small mono text at other sizes (Maple Mono 28/48 px): digit images as above,
  or the nearest table size of the `regular` or `bold` face.

## Budgets

An atlas holds 95 ASCII glyphs always, plus every other character the sources
contain, plus the tofu glyph:

```text
bytes = 16 + n × 8 + n × (cellW × d) × (cellH × d)
```

`n` is the glyph count, `d` the raster density and `cellW × cellH` the
logical cell of the slot: the widest and tallest ink of any of its glyphs, so
every glyph costs the cell of the widest one (`W`, `@`, `M`). Measured with
`bun tests/fonts/atlas-cost.ts` (ASCII only, `n` = 96):

| Slot | Face | px | Inter / JetBrains Mono cell | @1x | @2x | Montserrat Medium, SemiBold / Maple Mono Medium cell | @1x | @2x |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 19 | regular | 10 | 10×13 | 13,264 | 50,704 | 11×13 | 14,512 | 55,696 |
| 0 | regular | 12 | 12×15 | 18,064 | 69,904 | 14×16 | 22,288 | 86,800 |
| 1 | regular | 14 | 14×18 | 24,976 | 97,552 | 16×18 | 28,432 | 111,376 |
| 2 | regular | 16 | 16×20 | 31,504 | 123,664 | 18×20 | 35,344 | 139,024 |
| 3 | regular | 18 | 18×22 | 38,800 | 152,848 | 20×22 | 43,024 | 169,744 |
| 4 | regular | 20 | 20×24 | 46,864 | 185,104 | 22×25 | 53,584 | 211,984 |
| 5 | regular | 24 | 24×29 | 67,600 | 268,048 | 27×30 | 78,544 | 311,824 |
| 6 | regular | 36 | 35×44 | 148,624 | 592,144 | 40×45 | 173,584 | 691,984 |
| 14 | regular | 54 | 52×66 | 330,256 | 1,318,672 | 60×66 | 380,944 | 1,521,424 |
| 20 | bold | 10 | 11×13 | 14,512 | 55,696 | 12×13 | 15,760 | 60,688 |
| 7 | bold | 12 | 13×15 | 19,504 | 75,664 | 14×16 | 22,288 | 86,800 |
| 8 | bold | 14 | 15×18 | 26,704 | 104,464 | 16×18 | 28,432 | 111,376 |
| 9 | bold | 16 | 17×20 | 33,424 | 131,344 | 18×20 | 35,344 | 139,024 |
| 10 | bold | 18 | 19×22 | 40,912 | 161,296 | 21×22 | 45,136 | 178,192 |
| 11 | bold | 20 | 21×24 | 49,168 | 194,320 | 23×25 | 55,984 | 221,584 |
| 12 | bold | 24 | 25×29 | 70,384 | 279,184 | 27×30 | 78,544 | 311,824 |
| 13 | bold | 36 | 37×44 | 157,072 | 625,936 | 41×45 | 177,904 | 709,264 |
| 15 | bold | 54 | 55×66 | 349,264 | 1,394,704 | 61×66 | 387,280 | 1,546,768 |
| 16 | mono | 12 | 8×16 | 13,072 | 49,936 | 7×16 | 11,536 | 43,792 |
| 17 | mono | 14 | 9×19 | 17,200 | 66,448 | 9×19 | 17,200 | 66,448 |
| 18 | mono | 16 | 10×21 | 20,944 | 81,424 | 10×21 | 20,944 | 81,424 |

Each further character costs `cellW × cellH × d² + 8` bytes in every baked
slot (Inter 16 px: 328 B at 1x, 1,288 B at 2x).

Where the bytes go:

| Mode | Flash | RAM |
| --- | --- | --- |
| JS | The atlases are in the bundle's pak section in the application image | None for coverage: the embedded bundle is handed to the runtime in place (`pocket_memory_package` `take`) and atlases read their cells from it (`load_font_atlas_shared`) |
| AOT | `font-<slot>.bin` are `include_bytes!` in the application library | None for coverage when the crate calls the generated `load_fonts_in_place` (every program in this repository does): atlases read their cells from flash through `load_font_atlas_shared`. The generated `load_fonts` instead copies the cells into the runtime heap (`Atlas::parse`) |

AtomS3 runs AOT without PSRAM. With `load_fonts_in_place` its atlases cost
flash, not internal RAM. A crate that uses the generated `load_fonts` instead
budgets the atlases against internal RAM (about 270 KB free while the
`tests/aot-smoke` AOT image runs under QEMU): slots 0, 1, 2 and 9 (12, 14,
16 px regular and 16 px bold Inter) take 107,968 B, and one 54 px slot
(330,256 B) does not fit.

## Validation

`bun tests/fonts/check.ts` compiles `apps/sticks3-demos` (which declares
`app.fonts`) and `tests/aot-smoke/app` (which does not) with `tools/pocket`
in both modes and compares every atlas with a bake from the declared and from
the default faces; see `tests/fonts/README.md`. These are host-side checks,
not device validation.
