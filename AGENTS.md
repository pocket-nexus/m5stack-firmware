# Engineering conventions

These rules bind every change to this repository, by people and by agents.
Architecture and implementation notes belong in `docs/`, not here.

## Scope of the repository

- Each board has one firmware program that embeds one application: the
  PocketJS recreation of that board's M5Stack factory demo. The StickS3 is
  the exception: its program embeds `apps/sticks3-demos`, a set of demos of
  the board's hardware, instead of a factory recreation. There is no
  launcher of installable packages, no Applet packaging and no pocket-shell
  integration.
- PocketJS is `third_party/pocketjs`, pinned to `v0.17.0` or a later commit.
- Every program builds its application in two modes. **JS mode is the
  default**: the app is compiled to a bundle and runs in the QuickJS guest.
  **AOT mode** compiles the same TSX/TS sources with MicroTS into a Rust
  static library. Application sources therefore stay inside the MicroTS
  admission subset (`site/content/docs/microts-solid.md` in the submodule).
- A board whose internal RAM cannot hold the QuickJS guest is AOT-only: its
  `sdkconfig.defaults` sets `CONFIG_M5_POCKET_APP_MODE_AOT=y` and its program
  README gives the measurement. The AtomS3 (no PSRAM) is the only such board.

## Commits and pull requests

- Commit messages use Conventional Commits: `type(scope): summary`. Types:
  `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`, `ci`, `chore`,
  `style`, `revert`. The `commit-msg` hook runs commitlint with
  `commitlint.config.mjs`.
- The scope names the directory the change touches: a component
  (`m5-bsp-tab5`, `m5-bsp-stopwatch`, `m5-bsp-sticks3`, `m5-bsp-atoms3`,
  `m5-bsp-corep4x`, `m5-pocket-app`), a program (`tab5`, `stopwatch`,
  `sticks3`, `atoms3`, `corep4x`), an app (`tab5-factory`,
  `stopwatch-factory`, `sticks3-demos`, `atoms3-factory`,
  `corep4x-factory`), `tools`, `docs` or `deps`. Omit the scope only for
  repository-wide changes.
- The header is at most 72 characters; the summary is imperative, lower case,
  without a trailing period.
- Commit when a complete feature works and builds: one logical change per
  commit. Formatting-only changes to existing code go in their own `style`
  commit.
- A `third_party/pocketjs` bump is its own commit,
  `build(deps): bump pocketjs to <short-sha>`, with the upstream commit range
  in the body.
- `main` keeps a linear history: no merge commits. Squash merge is the
  preferred way to merge a pull request; its title becomes the commit header
  and follows the rules above. Use a rebase merge only when the commits of
  the pull request must stay separate on `main`.

## Formatting

- C and C++ sources are formatted by the clang-format version pinned in
  `package.json` (`clang-format-node`) with the root `.clang-format`. Do not
  reformat with another clang-format binary.
- The `pre-commit` hook formats staged C and C++ files and stages the result.
  Do not bypass hooks (`--no-verify`, `LEFTHOOK=0`). Run `bun run format:check`
  before pushing.
- Wrap large static tables (panel init sequences, lookup tables, generated
  data) in `// clang-format off` and `// clang-format on`. The markers enclose
  the table and nothing else.
- Files copied from M5Stack or Espressif sources are reformatted with the
  repository style when they are imported.

## Tooling

- `bun install` installs the JavaScript dependencies and the git hooks.
  `lefthook.yml` is the only hook definition; do not add scripts to
  `.git/hooks`.
- Use the Bun version in `.bun-version` and ESP-IDF `>=6.0,<6.3`. Source
  `$IDF_PATH/export.sh` before `idf.py`.

## Dependencies and third-party code

- `third_party/pocketjs` is a pinned submodule. Do not edit files in it from
  this repository; land the change upstream, then bump the submodule. Build
  output that PocketJS tools write inside the submodule is removed before
  committing.
- Board support code is taken from M5Stack's repositories (factory demos,
  M5GFX, M5Unified, M5PM1, M5IOE1, the Tab5 BSP) and lives in this
  repository. Do not send changes to the M5Stack repositories for this work.
- ESP-IDF registry dependencies are declared with exact versions in
  `idf_component.yml`. Each program commits its `dependencies.lock`.
- Code derived from third-party sources keeps its original copyright and
  `SPDX-License-Identifier` lines, and its component README names the source
  repository, the commit or tag and the files taken.

## Directory placement

- Board support for one board: `platforms/esp-idf/m5_bsp_<board>/`.
- Components shared by several programs (the JS/AOT application host):
  `platforms/esp-idf/<name>/`; their Rust crates: `platforms/rust/<name>/`.
- Firmware composition, `sdkconfig.defaults`, partitions and the host
  contract (`pocket.host.json`): `programs/<board>/`.
- The host config (`pocket.host.config.ts`) sits in the application
  directory, `apps/<board>-factory/` (StickS3: `apps/sticks3-demos/`), and
  names the program's host contract by relative path. The Pocket CLI
  rejects `..` in the app entry path, and the AOT model check covers only
  files under the config's directory.
- Components that only one program uses: `programs/<board>/components/`.
- Applications: `apps/<board>-factory/`; the StickS3 demos:
  `apps/sticks3-demos/`.
- Build, packaging and repository scripts: `tools/`.

## C code

- Every component library calls `m5_strict_warnings(${COMPONENT_LIB})` from
  `m5_pocket_app`. ESP-IDF passes `-Wall -Wextra -Werror` and then appends
  `-Wno-unused-parameter -Wno-sign-compare`; the helper restores those
  warnings as errors after IDF's options. Fix warnings; do not silence them
  with pragmas, casts that hide a defect, or by removing flags.
  Imported third-party sources that do not build warning-free keep their
  warnings scoped to that component with a comment naming the source.
- Every component states the IDF targets it supports (`REQUIRED_IDF_TARGETS`
  or `targets` in `idf_component.yml`).
- Public headers live under `include/<namespace>/`; private headers stay in
  `src/`. Public symbols carry the component prefix (`m5_tab5_`,
  `m5_stopwatch_`, `m5_sticks3_`, `m5_atoms3_`, `m5_corep4x_`,
  `m5_pocket_`).
- PocketJS ABI records: set `struct_size` on every record written, validate
  `struct_size` on every record read, and advance record arrays by each
  element's `struct_size`.
- Driver callbacks return PocketJS status codes; ESP-IDF APIs return
  `esp_err_t`. Do not mix the two in one function's return value.

## Build output and validation

- Never commit `build/`, `sdkconfig`, `managed_components/`, `node_modules/`,
  `target/`, `gen/`, `*.pkb` or `.pocket-build/`. Commit `sdkconfig.defaults`.
- Keep device logs, screenshots, captures and receipts in ignored
  `.pocket-build/validation/<task>/<run>/`.
- A change states how it was validated: commands run, results, and for device
  runs the board revision. A firmware build, a simulator screenshot or a
  host-side test is not device validation; say which one was done.

## Documentation

- Reference documentation (`README.md`, `docs/`, component READMEs) is in
  English. It states the mechanism and the concrete facts (pins, sizes,
  limits, versions) without slogans or intensifiers.
- Each application README lists the factory screens it reproduces and every
  known difference from the factory firmware. The StickS3 demo README lists
  each demo, its controls, the hardware it uses and its limits.
- When a change alters a documented behaviour, pin, limit or command, update
  that documentation in the same commit.
