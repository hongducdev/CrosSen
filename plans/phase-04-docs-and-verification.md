---
phase: 4
title: Docs and verification
status: in-progress
priority: P2
effort: 4h
dependencies:
  - 2
  - 3
---

# Phase 4: Docs and verification

## Overview

Document the two new themes for users, record the change in the changelog, and run the full
device/simulator verification matrix with visual proof per theme and per panel profile.

## Requirements

- Functional: `docs/user-guide.md` and `docs/touch-navigation.md` describe both themes; `CHANGELOG.md`
  gets an `Added` entry under the current unreleased version.
- Non-functional: verification runs on every build environment the change can affect, and the
  result is captured as evidence (build logs, screenshots), not asserted.

## Architecture

No production code changes. The verification gate is the `platformio.ini` environment matrix: the
two new themes touch shared Home code, so both the C3 (`default`, `debug`) and the S3
(`sticky`, `x4-pro`, `x4-classic`) targets must build, and both simulator panel profiles
(`simulator` = 480×800, `simulator-X3` = 528×792) must render correctly.

## Related Code Files

- Modify: `docs/user-guide.md` (UI Theme list, currently 245-258)
- Modify: `docs/touch-navigation.md` (cover-tap behaviour section, near 40-56)
- Modify: `CHANGELOG.md` (top `Added` section)
- Read-only: `docs/development/architecture.md`, `.claude/CONTEXT.md`
- Verify: `src/components/themes/minuta/**`, `src/activities/home/HomeActivity.*`,
  `plans/phase-01..03-*.md`

## Implementation Steps

1. **Submodules first.** `git submodule update --init --recursive`, then confirm
   `git submodule status` shows no leading `-`.
2. Document in `docs/user-guide.md` under **UI Theme**, appended after the "Cover Grid" entry:
   - `"Solum"` — a one-cover theme showing the cover of your most recent book with its title and
     author underneath; no menu rows, press Menu for the usual actions.
   - `"Quartum"` — a fixed 2×2 grid of your four most recent books; use the front buttons to move
     between them (Left/Right for the next book, Up/Down to jump a row); only the highlighted book
     shows its title and author.
   - Note that neither theme shows reading progress on Home, and that they are available on all
     devices (unlike Cover Grid, which needs PSRAM).
   - Add the MIT/`hiitsalice/minuta` derivation note where the docs discuss theme origins, if such a
     place exists; otherwise keep the attribution in the source headers only.
3. Document in `docs/touch-navigation.md`: tapping a cover opens that book; add Solum/Quartum to the
   list of themes that support swiping between recent books (Solum only — Quartum uses a cursor).
4. Add a `CHANGELOG.md` entry under `## [Unreleased]` → `### Added`:
   - "Add Solum and Quartum Home themes: a single large cover with title and author, and a fixed
     2×2 grid of the four most recent books with a button-driven cursor."
   Recommended: one entry per theme, so a reader can tell them apart.
5. Format the touched C++ exactly as the repo requires — the generated `lib/I18n/*` files must not
   be manually reformatted:
   `find src lib include test -name "*.cpp" -o -name "*.h" | xargs clang-format -i`
6. Run and **capture output** for:
   - `pio run -e simulator`
   - `pio run -e simulator-X3`
   - `pio run -e default`
   - `pio run -e sticky`
   - `pio run -e x4-pro`
   - `python3 scripts/run_simulator_smoke_test.py`
   - `pio check -e default --fail-on-defect low --fail-on-defect medium --fail-on-defect high`
   - `python3 scripts/check_firmware_size.py`
   - the native unit test target from phase 3
   Build with `wsl -d Ubuntu -e bash -lc 'export PATH="$HOME/.platformio/penv/bin:$PATH"; cd "/mnt/d/New folder/CrosSen" && pio run -e <env> 2>&1 | tee /tmp/pio-<env>.log; exit ${PIPESTATUS[0]}'`
   and a generous tool timeout (3600 s).
7. Visual evidence, captured from the simulator for **each** theme × panel profile: 4 books,
   3 books, 2 books, 1 book, 0 books, a book with no cover art, and a very long title/author.
   Record the screenshot paths in the phase file or the journal entry.
8. Check for regressions in the existing themes on the same screens (Home for Classic, Minimal,
   Dashboard, Lyra, Lyra Extended, Lyra Carousel, RoundedRaff, Cover Grid) — the new
   `usesMinutaHomeLayout()` branch must not intercept their paths.
9. On real hardware (the user's X3), verify: both themes selectable, Home renders, button navigation
   works, the choice persists after reboot, and the device returns to reading without a crash. Note
   that `update.bin` must be copied to the SD card for a device test.
10. Update `.claude/CONTEXT.md` only if a durable, reusable gotcha was discovered (for example the
    `kMaxCachedBooks`/recents-capacity coupling, or the X3 528-wide layout constraint).

## Success Criteria

- [ ] `docs/user-guide.md` documents both themes and states that they show no progress on Home.
- [ ] `docs/touch-navigation.md` covers cover taps and Solum's recent-book swipe.
- [ ] `CHANGELOG.md` has an `Added` entry for each theme.
- [ ] Every environment in step 6 builds; `pio check` reports no new defects.
- [ ] `check_firmware_size.py` passes; the size delta is recorded.
- [ ] Simulator screenshots exist for both themes on both panel profiles across all the listed
      book-count/art/title cases, and show no clipped text or overlapping rows.
- [ ] No existing theme regressed.
- [ ] Verified on the real X3 device (theme selectable, Home renders, navigation works, survives
      reboot), or an explicit note if hardware verification was not possible.

## Risk Assessment

| Risk | Mitigation |
| --- | --- |
| Declaring success from a simulator build alone | The criterion is explicit screenshots plus, where possible, a real-device check — a clean build is not proof the layout is right |
| Firmware size regression on the flash-tight C3 | Run `check_firmware_size.py` and report the delta; the new code is two small theme classes with no new tables |
| Docs drifting from behaviour (e.g. documenting a menu that does not exist) | Write the docs from the actual implemented interaction, re-reading the new `loop()` branch before writing |
| Missing a shared Home regression because the new branch shadows another | Explicitly re-test every other theme's Home after the branch is added |
