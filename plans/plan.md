---
title: 'Minuta themes: Solum and Quartum home themes'
description: >-
  Port the Solum (one-cover) and Quartum (fixed 2x2 grid) home themes from
  hiitsalice/minuta into CrossInk's UI theme system
status: in-progress
priority: P2
branch: main
tags:
  - ui
  - themes
  - home
blockedBy: []
blocks: []
created: '2026-10-09T15:20:10.273Z'
createdBy: 'ck:plan'
source: skill
---

# Minuta themes: Solum and Quartum home themes

## Overview

CrossInk ships eight UI themes (`CLASSIC`, `MINIMAL`, `DASHBOARD`, `LYRA`, `LYRA_3_COVERS`,
`LYRA_CAROUSEL`, `ROUNDEDRAFF`, `COVER_GRID`). This plan adds two more, ported from the
reference fork **`hiitsalice/minuta`** (MIT, Copyright (c) 2025 Dave Allie — itself a fork of
CrossPoint Reader, CrossInk's upstream):

| Theme | Home screen | Reference |
| --- | --- | --- |
| **Solum** | One large cover (3:5), centred title + author underneath, no menu rows | minuta `src/components/themes/minuta/solum.{h,cpp}` |
| **Quartum** | Fixed 2×2 grid of the 4 most recent books; a button-driven cursor; only the cursor slot shows a wrapped title and a rotated italic author spine beside the cover | minuta `src/components/themes/minuta/quartum.{h,cpp}` |

Both themes derive from `LyraTheme`, so every screen except Home keeps Lyra's look — exactly
how minuta does it. The work is therefore: two new theme classes, one new home-layout branch
and one new input branch in `HomeActivity`, and the usual enum/i18n/registration plumbing.

Reference clone for this plan: `%TEMP%\minuta-ref` (read-only, not part of the repo).

## Scope

#### In scope

- Two new themes with the layouts above, on X3 (528×792), X4 (480×800), Sticky and X4 Pro.
- Home input routing for both (cursor navigation, tap targets, menu-on-demand).
- Enum/i18n/settings-registration plumbing, docs, changelog, unit test for the cursor math.

#### Out of scope (explicitly not doing)

- Renaming or restyling the existing eight themes.
- A theme preview/gallery screen in Settings (no such screen exists today — YAGNI).
- Porting minuta's own HomeActivity rewrites; we reuse CrossInk's existing capability.
- Changing the mass-storage/web portal API (it has no theme surface — `src/network/` has zero
  theme references).

## Reference behaviour (verified from the minuta clone)

- **Solum** — cover box 360×600 (exactly 3:5) at `hPadding = 60`, i.e. `tileWidth = rect.width - 120`;
  title baseline `coverBottom + 46` in 18 pt, author `+ getLineHeight(titleFont) - 4` in 12 pt,
  both centred in a 400 px text area; titles ellipsised **at word boundaries**
  (`truncateTitleAtWord`); empty recents state = a bare `drawRect` outline (no fill, no icon);
  covers drawn stretched, missing art = filled bottom two-thirds + 32 px `CoverIcon`.
- **Quartum** — 2×2 grid, slot size 171×285 (3:5), `columnGap = rowGap = 18`, `hPadding = 60`,
  35 px title band above the top row; **all four slots always drawn** (empty slots = outline only);
  selection = double inset outline (1 px and 2 px inset); only the cursor slot gets metadata
  (title wrapped to max 2 lines, 11 pt, centred above the top-row cover or below the bottom-row
  cover); author is one italic line truncated to 285 px and drawn **rotated beside the cover**
  (left column 270°, right column 90°) using a temporary `setOrientation`.
- Home button hints: Solum `Menu / – / – / Read`, Quartum `Menu / Read / Prev / Next`.
- Neither theme draws reading progress on Home, and neither draws home menu rows.

## Decisions (recommended option marked ✅)

| # | Decision | Options | Recommendation |
| --- | --- | --- | --- |
| D1 | Theme names | (a) keep upstream `Solum` / `Quartum`; (b) CrossInk-style rename | ✅ (a) — matches the existing whimsical Lyra/RoundedRaff naming, and attribution stays honest |
| D2 | Enum values | append after `COVER_GRID = 7` | ✅ `SOLUM = 8`, `QUARTUM = 9`, `UI_THEME_COUNT = 10`. `CrossPointSettings.h:352` requires existing raw values stay stable |
| D3 | Fonts | minuta uses Steinem 18/12/11 | ✅ Solum title `LEXENDDECA_16_FONT_ID` (closest to 18 pt, has italic+bold; registered globally at `main.cpp:1178-1181`), Solum author `UI_10_FONT_ID`, Quartum title `UI_12_FONT_ID`, Quartum rotated author `LEXENDDECA_10_FONT_ID` **ITALIC** — Inter has no italic (`fontIds.h`: `UI_*` families ship regular+bold only). Fallback if the simulator render looks inconsistent: `UI_12` title / `UI_10` author everywhere |
| D4 | Geometry | minuta hardcodes 60/171/285/600 assuming 480 wide | ✅ derive from `renderer.getScreenWidth()/getScreenHeight()`; keep `homeCoverHeight = 600` (Solum) and 171×285 slots (Quartum) as metrics, centre the block, and cap cover width at `coverHeight * 3 / 5`. Identical look on X3 and X4, no `deviceIsX3()` branch needed |
| D5 | Home menu | minuta has none (Back → Browse, long-press → Recents) | ✅ show covers only, and open CrossInk's existing **on-demand button menu** on the Menu button — reuse the `minimalMenuOpen` + `buildMinimalMenuItems` + `drawButtonMenu` pattern (`HomeActivity.cpp:1445`, `2088`). This also keeps `HomeActivity::initialMenuItem` (Quick Actions "open Home on Library") working, which a menu-less layout would break |
| D6 | Quartum cursor math | minuta special-cases `bookCount` 2 and 3 by hand | ✅ one pure helper with modular arithmetic: Left `(i-1+n)%n`, Right `(i+1)%n`, Up `(i-2+n)%n`, Down `(i+2)%n`, no-op when `n <= 1`. `n == 2` makes Up/Down a no-op naturally |
| D7 | Cover draw call | minuta calls `drawBitmapStretched` | ✅ **`drawBitmapStretched` does not exist here.** Use `renderer.drawBitmap(bitmap, x, y, w, h)`; because thumbnails are generated at the 3:5 cache-key height that matches the box, `drawBitmap` fills it exactly. Use `fillRect` + `CoverIcon` for missing art (`LyraCarouselTheme.cpp:425` precedent) |

## Architecture

### How a theme plugs in

```text
src/CrossPointSettings.h                     enum UI_THEME  (+2 values, count 10)
src/SettingsList.cpp:37-46                   locale-visible enum option list + withEnumRawValues
src/SettingsList.h:665-681                   per-device option removal (unchanged; new themes are universal)
lib/I18n/translations/english.yaml           STR_THEME_SOLUM / STR_THEME_QUARTUM (source of truth)
lib/I18n/translations/vietnamese.yaml        same keys, Vietnamese text
scripts/gen_i18n.py                          regenerate lib/I18n/I18n*.{h,cpp} (generated — never hand-edit)
src/components/UITheme.cpp:86-135             setTheme() switch + includes
src/components/themes/minuta/SolumTheme.{h,cpp}      new — metrics + drawRecentBookCover
src/components/themes/minuta/QuartumTheme.{h,cpp}    new — metrics + drawRecentBookCover
src/components/themes/minuta/MinutaTextLayout.h      new — shared truncate/wrap helpers
src/components/themes/minuta/QuartumGridNav.h        new — pure cursor math (unit-testable)
src/activities/home/HomeActivity.cpp         render() branch, loop() branch, predicate, kMaxCachedBooks
src/activities/settings/ButtonRemapActivity.cpp:218  usesLyraValueBadge() — add both (they are Lyra-derived)
docs/user-guide.md, docs/touch-navigation.md, CHANGELOG.md
```

Non-home screens need **no** changes: both themes inherit `LyraTheme`, which owns header, list,
tab bar, popup, reader status bar and sleep screens.

### Home render path (current, `HomeActivity::render()` at `HomeActivity.cpp:2043-2275`)

Order today: quick-actions popup → `coverGridUi` → `usesMinimalHomeInteraction()` → carousel fast
path → default path (`clearScreen` → header → `GUI.drawRecentBookCover` at **2229** → `drawButtonMenu`
at 2235 → hints → `displayHomeBuffer`).

New branch: insert `if (usesMinutaHomeLayout())` immediately after the Minimal branch. It must:
clone the Minimal branch's structure (header → `coverRect*` bookkeeping → `drawRecentBookCover` →
hints → `displayHomeBuffer` → `firstRenderDone`/`loadRecentCovers` tail), swap the hint set, and
add a `minutaMenuOpen` overlay path mirroring `minimalMenuOpen` at `2087-2104`.

The `coverRect*` bookkeeping is mandatory: `storeCoverBuffer()` snapshots only
`{coverRectX, coverRectY, coverRectW, coverRectH}` so we snapshot the cover band instead of the
48 KB framebuffer. `homeCoverTileHeight` must therefore cover the whole artwork band.

### Home input path (current, `HomeActivity::loop()` at `HomeActivity.cpp:1367-1973`)

Branches: `coverGridUi` (1386) → `usesMinimalHomeInteraction()` (1427) → carousel (1658) → generic
else (1918). `selectorIndex` is one reused int. There is **no 2-D navigation helper anywhere**
(`ButtonNavigator` is 1-D, `CoverGridHomeUi` is touch-only). New branch goes after the Minimal
branch, before the carousel branch.

### Layout math (D4, D7)

`W = renderer.getScreenWidth()` (480 X4/Sticky/X4 Pro, 528 X3), `H = renderer.getScreenHeight()` (800 / 792).

**Solum** (`homeTopPadding = 60`):

```text
coverH = min(metrics.homeCoverHeight /*600*/, (W - 2*sidePad) * 5 / 3)
coverW = coverH * 3 / 5                 // 360 on both panels
coverX = (W - coverW) / 2
titleY  = coverBottom + 46 - ascender(titleFont)
authorY = titleY + lineHeight(titleFont) - 4 - ascender(authorFont)
sidePad = max(W / 8, 40)                // 60 on X4, 66 on X3
```

Vertical budget: 60 + 600 + ~22 (title) + ~20 (author) ≈ 702 px, inside 792 − 40 (hints) on X3.

**Quartum** (`homeTopPadding = 60`):

```text
gridW  = 2 * 171 + 18                   // 360, same on both panels
gridX  = (W - gridW) / 2
topCoverY    = rect.y + 4 + titleAreaHeight /*35*/
bottomCoverY = topCoverY + 285 + 18
bottomTitleY = bottomCoverY + 285
```

Vertical budget: 60 + 4 + 35 + 285 + 18 + 285 + (9 + ≤34 for the bottom-row title) ≈ 730 px,
inside 792 − 40 on X3. Set `homeCoverTileHeight = 660` for both themes so the snapshot band
covers every cover plus the title bands.

### Memory

Cover-band snapshot: Solum ≈ 480×660/8 ≈ 39 KB, Quartum identical — in line with the existing
`Minimal` (`homeCoverTileHeight = 690` ≈ 41 KB), so **no PSRAM gate is needed**. `COVER_GRID`'s
`supportsCoverGrid()` gate stays as-is (`SettingsList.h:665`).

## Phases

| Phase | Name | Status |
| --- | --- | --- |
| 1 | [Theme scaffolding](./phase-01-theme-scaffolding.md) | Completed |
| 2 | [Solum home](./phase-02-solum-home.md) | Completed |
| 3 | [Quartum grid](./phase-03-quartum-grid.md) | Completed |
| 4 | [Docs and verification](./phase-04-docs-and-verification.md) | In Progress |

## Dependencies

None. No existing plan overlaps (`plans/` did not exist before this plan).

**Blocker to resolve before any build:** the `freeink-sdk` and `assets/tabler-icons` submodules are
not checked out in this working tree (`git submodule status` shows a leading `-`). Run
`git submodule update --init --recursive` first, otherwise `pio run` cannot resolve
`symlink://freeink-sdk/...` dependencies.

## Verification

- `pio run -e simulator` and `pio run -e simulator-X3` — both new themes selectable and rendering.
- `pio run -e default` (ESP32-C3, X3/X4) and `pio run -e sticky` / `-e x4-pro` (S3) — full matrix.
- `pio run -e simulator` + `scripts/run_simulator_smoke_test.py` — no regression in existing screens.
- `pio check -e default --fail-on-defect low` and `scripts/check_firmware_size.py`.
- Native unit test for the Quartum cursor math (`pio test` / CTest target).
- Visual check in the simulator per theme, on both panel profiles: 4 books, 3, 2, 1, 0 books;
  long titles; missing cover art; long-press/back/menu paths; tap on every slot on touch profiles.

## Risks

| Risk | Mitigation |
| --- | --- |
| `kMaxCachedBooks = 3` (`HomeActivity.h:29`) caps recents at 3 → Quartum would only ever show 3 covers | Raise to 4 and relax the `static_assert` at `HomeActivity.cpp:528` to cover `QuartumMetrics::homeRecentBooksCount`; verify heap on the C3 profile |
| Quartum's rotated author temporarily changes renderer orientation | Save/restore `getOrientation()` around every rotated draw (minuta does this); never rotate outside the theme's cover call |
| X3 is 528 wide, minuta assumes 480 | D4 derives everything from renderer dimensions; assert the layout on `simulator-X3` |
| Four 285 px thumbnails on a C3 build | Thumbnails are loaded one at a time through the existing `loadRecentCovers` path, not simultaneously; measure with `ESP.getFreeHeap()`/`getMaxAllocHeap()` |
| New home branch duplicating the Minimal branch could rot | Extract nothing prematurely; if the branch exceeds ~60 lines of near-duplicate code, factor a shared helper |

## Open questions

1. Vietnamese names for the two themes — recommend carrying the English names into all locales
   (brand-like proper nouns, like "Lyra"/"RoundedRaff" already are).
2. Whether to surface either theme in the Sticky/X4 Pro touch flow differently — recommend no:
   both work as-is, with covers becoming tap targets.
