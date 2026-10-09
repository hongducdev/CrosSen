---
phase: 1
title: Theme scaffolding
status: completed
priority: P2
effort: 2h
dependencies: []
---

# Phase 1: Theme scaffolding

## Overview

Register two new theme slots — `SOLUM` and `QUARTUM` — end to end so they appear in
**Settings > Display > UI Theme** and can be selected, while their Home rendering is still
inherited from `LyraTheme`. This phase changes no visible layout; it exists so every later phase
has a build-green, independently testable base.

## Requirements

- Functional: both names selectable in Settings; selecting either keeps every screen working and
  looking like Lyra; the choice persists across reboot and `UITheme::reload()`.
- Non-functional: existing persisted theme raw values must not shift; no new heap in the static
  theme instance; both themes must be valid on ESP32-C3 (no PSRAM assumption).

## Architecture

A theme is four coupled pieces: an enum value, a `ThemeMetrics` constant, a `BaseTheme` subclass,
and a registration case. Both new themes derive from `LyraTheme`, so only
`drawRecentBookCover()` will be overridden later — header, list, tab bar, popups, reader status
bar and sleep screens come from Lyra for free.

```text
src/components/themes/minuta/
  SolumTheme.h/.cpp        namespace SolumMetrics + class SolumTheme : public LyraTheme
  QuartumTheme.h/.cpp      namespace QuartumMetrics + class QuartumTheme : public LyraTheme
  MinutaTextLayout.h       header-only word-boundary truncate/wrap helpers (used from phase 2/3)
```

Metrics (from the reference, `minuta-ref/src/components/themes/minuta/*.h`), each a copy of
`LyraMetrics::values` with only the home fields changed:

| Metric | Solum | Quartum |
| --- | --- | --- |
| `homeTopPadding` | 60 | 60 |
| `homeCoverHeight` | 600 | 285 |
| `homeCoverTileHeight` | 660 | 660 |
| `homeRecentBooksCount` | 1 | 4 |
| `homeContinueReadingInMenu` | false | false |
| `homeMenuTopOffset` | 0 | 0 |

`homeCoverHeight` doubles as the thumbnail generation height passed to
`HomeActivity::loadRecentCovers(metrics.homeCoverHeight)`.

`MinutaTextLayout.h` exposes two free functions in an anonymous-namespace-free header (so both
`.cpp` files can share them without linkage tricks):

```text
std::string truncateTitleAtWord(renderer, fontId, text, maxWidth);           // Solum
std::vector<std::string> wrapTitleAtWords(renderer, fontId, text, maxWidth); // Quartum, max 2 lines
```

Both ported verbatim in behaviour from `solum.cpp:23` / `quartum.cpp:26`, with the final fallback
still `renderer.truncatedText(...)`.

## Related Code Files

- Modify: `src/CrossPointSettings.h` (enum `UI_THEME` at 352-363, `UI_THEME_COUNT`)
- Modify: `src/components/UITheme.cpp` (includes near 23-30, `setTheme()` at 86-135)
- Modify: `src/SettingsList.cpp:37-46` (theme option list + `withEnumRawValues`)
- Modify: `src/activities/settings/ButtonRemapActivity.cpp:218-227` (`usesLyraValueBadge()`)
- Modify: `lib/I18n/translations/english.yaml` (near 524), `lib/I18n/translations/vietnamese.yaml` (near 358/569)
- Regenerate: `lib/I18n/I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` via `scripts/gen_i18n.py` (generated — never hand-edit)
- Create: `src/components/themes/minuta/SolumTheme.h`, `SolumTheme.cpp`
- Create: `src/components/themes/minuta/QuartumTheme.h`, `QuartumTheme.cpp`
- Create: `src/components/themes/minuta/MinutaTextLayout.h`

## Implementation Steps

1. **Unblock the build.** The `freeink-sdk` and `assets/tabler-icons` submodules are not checked
   out in this tree (`git submodule status` shows a leading `-`). Run
   `git submodule update --init --recursive` and confirm `freeink-sdk/libs/ui/FreeInkUI` exists.
2. `src/CrossPointSettings.h`: append to `enum UI_THEME`
   ```text
   SOLUM = 8,
   QUARTUM = 9,
   UI_THEME_COUNT = 10
   ```
   Leave every existing value untouched — the file's own comment at line 352 requires it, and
   `CrossPointSettings.cpp:1103` clamps loaded values against `UI_THEME_COUNT`.
3. Create `SolumTheme.h` / `QuartumTheme.h`: `namespace SolumMetrics { constexpr ThemeMetrics values = makeValues(); }`
   modelled on `MinimalMetrics` (`MinimalTheme.h:9-27`), plus the class declaration with the exact
   CrossInk override signature (7 params + the four CrossInk-only ones — copy it verbatim from
   `Lyra3CoversTheme.h:23-28`).
4. Create the two `.cpp` files. Phase 1 body: `drawRecentBookCover(...)` forwards to
   `LyraTheme::drawRecentBookCover(...)` with all arguments passed through. Add the MIT
   attribution header comment naming `hiitsalice/minuta`.
5. Create `MinutaTextLayout.h` with the two helpers ported from the reference (they are unused
   until phase 2/3; mark the header `// Phase 1: declared for phase 2/3 use`).
6. `src/components/UITheme.cpp`: add the two includes and two `setTheme()` cases before `default`:
   ```text
   case UI_THEME::SOLUM:    currentTheme = make_unique<SolumTheme>();    currentMetrics = &SolumMetrics::values;    break;
   case UI_THEME::QUARTUM:  currentTheme = make_unique<QuartumTheme>();  currentMetrics = &QuartumMetrics::values;  break;
   ```
   Do **not** touch the `COVER_GRID` PSRAM fallback at 88-90.
7. `src/SettingsList.cpp:37-46`: append `StrId::STR_THEME_SOLUM, StrId::STR_THEME_QUARTUM` to the
   label list and `SOLUM, QUARTUM` to `withEnumRawValues` in the same order.
8. `src/activities/settings/ButtonRemapActivity.cpp:218-227`: add `SOLUM` and `QUARTUM` to
   `usesLyraValueBadge()` — they are Lyra-derived and must keep Lyra's selected-value badge.
9. i18n: add `STR_THEME_SOLUM: "Solum"` and `STR_THEME_QUARTUM: "Quartum"` to `english.yaml`
   (source of truth) and the Vietnamese file, then run `python3 scripts/gen_i18n.py` and commit the
   regenerated `lib/I18n/*` files. Recommend carrying the English names into all locales since they
   are proper nouns, matching how "Lyra"/"RoundedRaff" are already handled.
10. Build and select: `pio run -e simulator`, then verify both entries appear in the theme list.

## Success Criteria

- [ ] `git submodule status` shows no leading `-`; `pio run -e simulator` succeeds.
- [ ] **Settings > Display > UI Theme** lists "Solum" and "Quartum" after "Cover Grid".
- [ ] Selecting either keeps all screens working; Home still renders Lyra's layout (expected at this phase).
- [ ] The selection survives a restart and is not clamped back by `CrossPointSettings.cpp:1103`.
- [ ] `pio run -e default` succeeds and firmware size does not regress beyond the size gate.
- [ ] No existing theme's raw value changed (diff `CrossPointSettings.h` shows only appended entries).

## Risk Assessment

| Risk | Mitigation |
| --- | --- |
| Reordering enum values breaks saved settings on existing devices | Only append; never renumber. Verified by diff and by the load clamp. |
| Hand-editing generated i18n files | Always run `scripts/gen_i18n.py`; confirm the generated files are the only I18n diff. |
| Extra theme objects increase the static/runtime footprint | `UITheme` holds one `unique_ptr` at a time; `ThemeMetrics` is `constexpr` flash data. Measure with the size gate. |
| Missing a `UI_THEME` switch site | Cross-check `UITheme.cpp` `setTheme`, `SettingsList.{h,cpp}`, `ButtonRemapActivity`, `UIThemeTokens.h`, `HomeActivity.cpp:321/325` — this list is exhaustive as of `v1.6.1`. |
