---
phase: 2
title: Solum home
status: completed
priority: P2
effort: 6h
dependencies:
  - 1
---

# Phase 2: Solum home

## Overview

Give the `SOLUM` theme its real Home screen — one large 3:5 cover with a centred title and author
underneath and no menu rows — and give `HomeActivity` a new "cover-only home" render and input
branch that opens CrossInk's existing on-demand button menu. That branch is written generically
here so phase 3 only has to add Quartum's cursor to it.

## Requirements

- Functional:
  - One cover, horizontally centred, in a 3:5 box derived from the renderer, not hardcoded.
  - Title (word-boundary ellipsis) and author centred beneath the cover; author omitted when empty.
  - Empty state = a bare outline rect, no fill, no icon, no text.
  - Missing cover art = filled lower two-thirds + 32 px `CoverIcon`.
  - No menu rows on Home. A Menu button opens the existing full-screen button menu; `initialMenuItem`
    (Quick Actions entering Home on Library/Transfer/Settings) still works.
  - Left/Right or a horizontal swipe still rotates which recent book is shown (existing cover-swap
    behaviour, `homeBookSwapEnabled()` at `HomeActivity.cpp:940`).
  - On touch devices a cover tap opens the book.
- Non-functional: reuse the cover-band snapshot (`storeCoverBuffer`) instead of snapshotting 48 KB;
  no per-frame heap allocation; all text through `renderer`, no hardcoded `480`/`800`.

## Architecture

`SolumTheme::drawRecentBookCover()` is called from a new `usesMinutaHomeLayout()` branch in
`HomeActivity::render()`, shaped like the existing `usesMinimalHomeInteraction()` branch
(`HomeActivity.cpp:2085-2145`). The branch must keep the `coverRectX/Y/W/H` bookkeeping so
`storeCoverBuffer()` snapshots only the artwork band, and must end with the same `firstRenderDone` /
`recentsLoaded` tail so thumbnails still generate on the second render.

Layout (all derived; `W = renderer.getScreenWidth()`, `H = renderer.getScreenHeight()`):

```text
sidePad = max(W / 8, 40)                       // 60 on X4, 66 on X3
coverH  = min(metrics.homeCoverHeight /*600*/, (W - 2 * sidePad) * 5 / 3)
coverW  = coverH * 3 / 5                       // 360 on both panels
coverX  = (W - coverW) / 2
titleY   = coverBottom + 46 - getFontAscenderSize(titleFont)
authorY  = titleY + getLineHeight(titleFont) - 4 - getFontAscenderSize(authorFont)
```

Fonts per plan D3: title `LEXENDDECA_16_FONT_ID` REGULAR, author `UI_10_FONT_ID` REGULAR.

Home branch interaction (`HomeActivity::loop()`):

| Input | Action |
| --- | --- |
| Confirm (release) | open the selected recent book |
| Left / Right, or horizontal swipe | rotate to the previous/next recent book |
| Menu button (`Back` on X3/X4) | toggle `minutaMenuOpen`; the overlay draws `buildMinimalMenuItems` through `GUI.drawButtonMenu` and navigates with `minimalMenuIndex` |
| Long press | existing Quick Actions path — do not intercept |
| Touch tap on cover | open that book (register one `TouchRegistry::Cover` target) |

Guard against the stale Back release when Home is entered while Back is held — mirror
`minimalSuppressInitialFrontRelease` (`HomeActivity.cpp:828`, `usesMinimalHomeInteraction()`).

## Related Code Files

- Modify: `src/components/themes/minuta/SolumTheme.cpp` (replace the phase-1 passthrough body)
- Modify: `src/components/themes/minuta/MinutaTextLayout.h` (implement `truncateTitleAtWord`)
- Modify: `src/activities/home/HomeActivity.h` (`minutaMenuOpen`, `minutaMenuIndex`,
  `minutaSuppressInitialFrontRelease` fields; `isSolumTheme()` / `usesMinutaHomeLayout()` helpers)
- Modify: `src/activities/home/HomeActivity.cpp`
  - helpers near 319-338 (`isSolumTheme`, `usesMinutaHomeLayout`, reuse `buildMinimalMenuItems`)
  - `render()` — new branch after `usesMinimalHomeInteraction()` (ends at 2145)
  - `loop()` — new branch after the Minimal branch (ends at 1652)
  - `onEnter()` — reset the new state near 827-838
- Reference (read, do not edit): `src/components/themes/minimal/MinimalTheme.cpp:690-730`,
  `src/components/themes/lyra/Lyra3CoversTheme.cpp:22-110`, `src/components/themes/lyra/LyraCarouselTheme.cpp:425`

## Implementation Steps

1. Implement `truncateTitleAtWord` in `MinutaTextLayout.h` (port `minuta-ref/.../solum.cpp:23-44`):
   return the text unchanged when it fits; otherwise drop trailing words until `shortened + "..."`
   fits; if no space remains, fall back to `renderer.truncatedText(...)`.
2. Implement `SolumTheme::drawRecentBookCover`. Order:
   cover art (snapshot-guarded by `!coverRendered` → `drawBitmap` in the 3:5 box, or the
   missing-art fill + `CoverIcon`) → `TouchRegistry::getInstance().add(tileRect, 0, TouchRegistry::Cover)`
   → title → author. Do **not** forward to `LyraTheme`.
   - `drawBitmapStretched` does not exist in this repo — use
     `renderer.drawBitmap(bitmap, x, y, w, h)`.
   - Read the thumbnail with the `[HEIGHT]` template:
     `UITheme::getCoverThumbPath(book.coverBmpPath, metrics.homeCoverHeight)`, then `HalFile` +
     `Storage.openFileForRead` + `Bitmap::parseHeaders()` — same idiom as
     `Lyra3CoversTheme.cpp:38-64`, closing the file on every path.
3. Add `isSolumTheme()` and `usesMinutaHomeLayout()` next to `isMinimalTheme()`
   (`HomeActivity.cpp:319-327`).
4. Add the `render()` branch. Start from the Minimal branch and remove the menu rows; keep
   `GUI.drawHeader`, the `coverRect*` assignment, the `drawRecentBookCover` call with the full
   12-argument CrossInk signature (pass `nullptr` stats, `-1.0f` progress — Solum shows no progress),
   the hints, and `displayHomeBuffer()`. Add a `minutaMenuOpen` overlay path modelled on
   `2087-2104`. Hints: `Menu / (empty) / (empty) / Read` via `mappedInput.mapLabels`.
5. Add the `onEnter()` resets and the stale-Back guard.
6. Add the `loop()` branch: the table above, using `mappedInput.wasReleased(...)`,
   `ButtonNavigator::onNext/onPrevious` for cover rotation, `mappedInput.wasCoverTapped(idx)` /
   `wasCoverTouchedDown(idx)` for touch, and the `minutaMenuOpen` sub-loop for menu navigation.
   Route selection through the existing `activateSelectedHomeItem()`.
7. Handle `initialMenuItem`: on `usesMinutaHomeLayout()` the theme has no persistent menu rows, so
   when `initialMenuItem != HomeMenuItem::NONE` open the menu overlay on the corresponding entry
   (reuse `findMenuActionIndex(menuItems, homeActionForInitialMenuItem(...))`).
8. Build and check on `simulator` and `simulator-X3`.

## Success Criteria

- [ ] Solum Home shows exactly one centred 3:5 cover with title + author beneath it.
- [ ] No menu rows are visible on Solum Home; the Menu button toggles the full-screen menu overlay.
- [ ] Quick Actions that open Home on Library/Transfer/Settings still land on that screen.
- [ ] 0 / 1 / 2+ recent books all render sanely; the 0-book state is a bare outline.
- [ ] A book with no cover art shows the filled placeholder + book icon, not a blank box.
- [ ] A very long title ellipsises at a word boundary and never overflows the text area.
- [ ] `pio run -e simulator` and `pio run -e simulator-X3` both build and render correctly.
- [ ] Left/Right and horizontal swipe still rotate the displayed recent book.
- [ ] No regression in any other theme's Home screen (spot-check Lyra, Minimal, Classic).

## Risk Assessment

| Risk | Mitigation |
| --- | --- |
| Duplicating the Minimal branch makes `HomeActivity` harder to maintain | Keep the branch tight; if it exceeds ~60 lines of near-duplicate code, extract a shared `drawCoverOnlyHome()` helper |
| Forgetting `coverRect*` bookkeeping corrupts `storeCoverBuffer` | Set all four fields before calling `drawRecentBookCover`, exactly as the other branches do |
| `LEXENDDECA_16` metrics differ from `UI_*` and break the vertical budget | Verify the title/author block is bottom-clipped correctly on X3 (792 px) in the simulator; fall back to `UI_12`/`UI_10` if the budget is tight |
| Stale Back release re-entering Home immediately opens the menu | Mirror the existing `minimalSuppressInitialFrontRelease` guard |
| Touch devices with no front buttons lose the Menu affordance | On touch, keep an on-screen way back to the menu (the header tap target or the existing touch hint path) |
