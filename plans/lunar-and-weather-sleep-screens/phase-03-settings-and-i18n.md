---
phase: 3
title: "Settings, Enum & i18n Plumbing"
status: pending
priority: P1
effort: "1.5h"
dependencies: ["1", "2"]
---

# Phase 03: Settings, Enum and i18n Plumbing

## Overview
Extend `CrossPointSettings::SLEEP_SCREEN_MODE` with `LUNAR_CALENDAR_SLEEP = 12` and `WEATHER_SLEEP = 13`. Update the storage order, migration contract tests, `SettingsList.h`, and add localization keys to `lib/I18n/translations/*.yaml`.

## Requirements
- Functional:
  - Add `LUNAR_CALENDAR_SLEEP = 12` and `WEATHER_SLEEP = 13` before `SLEEP_SCREEN_MODE_COUNT`.
  - Add `LUNAR_CALENDAR_SLEEP` and `WEATHER_SLEEP` to `SLEEP_SCREEN_STORAGE_ORDER` in `CrossPointSettings.cpp`.
  - Update `verifySleepScreenMigrationContract()`.
  - Update `buildSleepScreenSetting()` in `src/SettingsList.h` with `StrId::STR_LUNAR_CALENDAR_SLEEP` and `StrId::STR_WEATHER_SLEEP`.
  - Add i18n keys to `lib/I18n/translations/vietnamese.yaml` and `english.yaml`.
  - Regenerate `I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp` via `scripts/gen_i18n.py`.
- Non-functional:
  - Enum remains append-only; existing stored values are unaffected.

## Related Code Files
- Modify: `src/CrossPointSettings.h`
- Modify: `src/CrossPointSettings.cpp`
- Modify: `src/SettingsList.h`
- Modify: `lib/I18n/translations/english.yaml`
- Modify: `lib/I18n/translations/vietnamese.yaml`
- Generate: `lib/I18n/I18nKeys.h`, `lib/I18n/I18nStrings.h`, `lib/I18n/I18nStrings.cpp`

## Success Criteria
- [ ] `verifySleepScreenMigrationContract()` passes.
- [ ] New options appear in the sleep screen selection menu.
