---
phase: 3
title: Quartum grid
status: completed
priority: P2
effort: 8h
dependencies:
  - 1
  - 2
---

# Phase 3: Quartum grid

## Overview

Give the `QUARTUM` theme its real Home screen — a fixed 2×2 grid of the four most recent books
with a button-driven cursor — and add the 2-D cursor navigation, per-slot touch targets and
recents-capacity change that layout needs.

## Requirements

- Functional:
  - All four slots are always drawn; a slot with no book shows only an outline (no fill, no icon).
  - Only the cursor slot shows metadata: a title wrapped to at most two lines, drawn above the
    top-row cover or below the bottom-row cover, plus one italic author line rotated beside the
    cover (left column 270°, right column 90°).
  - Selection is a double inset outline (1 px and 2 px inset) around the cursor slot.
  - Left/Right move the cursor by ±1, Up/Down by ±2, wrapping; no-op when there is 0 or 1 book.
  - Confirm opens the cursor's book. On touch, tapping a slot opens that book directly.
  - The Menu button opens the same on-demand menu overlay as Solum.
  - Up to four recents must actually load (`kMaxCachedBooks` is currently 3).
- Non-functional: no heap churn per frame; renderer orientation is restored after every rotated
  draw; layout derived from renderer dimensions, identical on X3 (528×792) and X4 (480×800).

## Architecture

### Geometry (derived; `W = renderer.getScreenWidth()`)

```text
slotW = 171, slotH = 285, columnGap = rowGap = 18     // metrics, 3:5 slots
gridW = 2 * slotW + columnGap                          // 360 on both panels
gridX = (W - gridW) / 2                                // 60 on X4, 84 on X3
topCoverY    = rect.y + 4 + titleAreaHeight /*35*/
bottomCoverY = topCoverY + slotH + rowGap
bottomTitleY = bottomCoverY + slotH
slotX(col)   = gridX + col * (slotW + columnGap)
```

Vertical budget: 60 + 4 + 35 + 285 + 18 + 285 + (9 + ≤34 for the bottom-row title) ≈ 730 px,
inside `792 - 40` (X3, button hints) and `800 - 40` (X4). `homeCoverTileHeight = 660` keeps the
snapshot band over every cover plus both title bands.

### Rendering order in `QuartumTheme::drawRecentBookCover()`

```text
1. if (!coverRendered): for each of the 4 slots -> cover art into the band snapshot
                        (drawBitmap 171x285, or border-only for empty slots,
                         or fillBottom2/3 + CoverIcon when art is missing)
                        then coverBufferStored = storeCoverBuffer(); coverRendered = coverBufferStored
2. selected slot: double inset outline
3. cursor slot only: title lines (max 2, UI_12_FONT_ID) above/below the row
4. cursor slot only: italic author (LEXENDDECA_10_FONT_ID) rotated beside the cover
5. TouchRegistry::add() for each occupied slot
```

Step 4 must save `renderer.getOrientation()`, `setOrientation(LandscapeClockwise |
LandscapeCounterClockwise)`, draw, then restore — same technique as the reference
(`minuta-ref/.../quartum.cpp:190-330`). The right column uses `LandscapeCounterClockwise`, the
left column `LandscapeClockwise`, so both read outward from the cover.

### Cursor navigation

`src/components/themes/minuta/QuartumGridNav.h`, header-only and free of renderer/SDK types so it
can be unit-tested natively:

```text
constexpr int quartumNextIndex(int current, int bookCount, int delta);
// bookCount <= 1        -> current
// otherwise             -> ((current + delta) % bookCount + bookCount) % bookCount
// delta: Left = -1, Right = +1, Up = -2, Down = +2
```

`bookCount == 2` makes Up/Down a no-op by modular arithmetic; `bookCount == 3` wraps correctly.
This replaces the reference's hand-written special cases for 2 and 3 books.

## Related Code Files

- Modify: `src/components/themes/minuta/QuartumTheme.cpp` (replace the phase-1 passthrough body)
- Modify: `src/components/themes/minuta/MinutaTextLayout.h` (implement `wrapTitleAtWords`)
- Create: `src/components/themes/minuta/QuartumGridNav.h`
- Modify: `src/activities/home/HomeActivity.h:29` (`kMaxCachedBooks` 3 → 4)
- Modify: `src/activities/home/HomeActivity.cpp`
  - `static_assert` at 528 (must hold for Quartum, not only the carousel)
  - `isQuartumTheme()` alongside `isSolumTheme()`; extend `usesMinutaHomeLayout()`
  - `render()` — extend the phase-2 branch: header, hints `Menu / Read / Prev / Next`
  - `loop()` — extend the phase-2 branch with the 2×2 cursor and per-slot buttons
  - cover-touch handler for four slot indices
- Create: `test/quartum_grid_nav/{CMakeLists.txt,quartum_grid_nav_test.cpp}` (see phase 4 for the
  runner wiring, or register here with `scripts/register_unit_tests_target.py`)
- Reference (read, do not edit): `minuta-ref/src/components/themes/minuta/quartum.cpp`

## Implementation Steps

1. Implement `wrapTitleAtWords` in `MinutaTextLayout.h` (port `quartum.cpp:26-105`): split on
   spaces, greedily fill line 1, then if the remainder fits on line 2 push it without an ellipsis,
   otherwise fill line 2 with `"..."` appended while words remain; fall back to
   `renderer.truncatedText` for a single over-wide word. Cap at two lines.
2. Create `QuartumGridNav.h` with `constexpr int quartumNextIndex(int, int, int)` as specified.
3. Implement `QuartumTheme::drawRecentBookCover()` in the order above. For each occupied slot use
   `UITheme::getCoverThumbPath(book.coverBmpPath, metrics.homeCoverHeight)` (285) and the
   `HalFile`/`Bitmap::parseHeaders()` idiom; close every file handle on every path.
4. Draw the cursor slot's title: `isTopRow ? topCoverY - blockHeight - 9 : bottomTitleY + 9`, lines
   centred within `slotW`. Keep one-line titles close to the cover and let two-line titles expand
   away from it, as the reference does.
5. Draw the cursor slot's rotated italic author with the save/restore orientation pattern; anchor
   the top-row line at the cover top and the bottom-row line at the cover bottom.
6. Register one `TouchRegistry::Cover` target per occupied slot (id = slot index) so taps work on
   Sticky/X4 Pro.
7. Raise `HomeActivity::kMaxCachedBooks` to 4 and update the `static_assert` at 528 to cover both
   `LyraCarouselMetrics::values.homeRecentBooksCount` and
   `QuartumMetrics::values.homeRecentBooksCount`. Confirm `getVisibleRecentBookCount()` now returns
   4 so four thumbnails are generated and loaded.
8. Extend the `loop()` branch: when `isQuartumTheme()`, Left/Right/Up/Down call
   `quartumNextIndex(selectorIndex, bookCount, ±1 / ±2)` and `requestUpdate()`. Reuse the phase-2
   touch handler, mapping a touched slot index straight to the book. Clamp the cursor when the
   recents list shrinks (e.g. a book was deleted).
9. Extend the `render()` branch hints for Quartum: `Menu / Read / Prev / Next` via
   `mappedInput.mapLabels`.
10. Add `test/quartum_grid_nav/`: pure assertions over `quartumNextIndex` —
    `n = 0, 1, 2, 3, 4`; delta `±1, ±2`; full wrap-around from every start index; negative/large
    `current` clamping. Register the target and run it.
11. Build both simulators plus `default`, `sticky`, `x4-pro`.

## Success Criteria

- [ ] Quartum Home shows a 2×2 grid; with 4 recents all four covers appear.
- [ ] With 0, 1, 2 and 3 recents the empty slots render as outlines only and nothing crashes.
- [ ] Left/Right move ±1 and Up/Down ±2 with correct wrap-around for 1–4 books.
- [ ] Only the cursor slot shows a title (≤2 lines) and a rotated italic author on the correct side.
- [ ] The author on the left column reads bottom-to-top and on the right column top-to-bottom.
- [ ] Renderer orientation is restored after drawing (subsequent draws are not rotated).
- [ ] Covers are tap targets on Sticky and X4 Pro; tapping a slot opens that book.
- [ ] `test/quartum_grid_nav` passes.
- [ ] `pio run -e default` (ESP32-C3) succeeds with no heap-allocation failure at wake, and
      `pio run -e sticky` / `-e x4-pro` succeed.
- [ ] Existing themes are unaffected (spot-check Lyra, Lyra Carousel, Cover Grid).

## Risk Assessment

| Risk | Mitigation |
| --- | --- |
| `kMaxCachedBooks = 4` raises static RAM (`BookReadingStats` + float per slot) on the C3 | The arrays are small fixed structs; measure `ESP.getFreeHeap()` at wake against the documented ~85-90 KB baseline in `.claude/CONTEXT.md` |
| Four 285 px thumbnails generated on a C3 at Home entry | Thumbnails are generated once and cached by `loadRecentCovers`; verify the first-render path still paints before the SD work (the existing `firstRenderDone` dance) |
| Rotated author corrupts the renderer's orientation for later draws | Save/restore around every rotated draw; assert visually in the simulator with a second draw pass |
| Title text overlapping the neighbouring row | Draw titles only for the cursor slot, and clamp the two-line block into the reserved 35 px top band / the space below the bottom row |
| Long author strings overflow the visible band | `renderer.truncatedText(fontId, author, slotH, ITALIC)` — truncate to the slot height before rotating |
| `selectorIndex` out of range after the recents list shrinks | Clamp against `bookCount` at the top of the render and input branches |
