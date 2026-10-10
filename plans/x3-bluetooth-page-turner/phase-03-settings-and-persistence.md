---
phase: 3
title: "Settings And Persistence"
status: pending
priority: P1
effort: "1.5d"
dependencies: [1, 2]
---

# Phase 3: Settings And Persistence

## Overview

Give the feature a user surface — **Settings → System → Bluetooth** — and make the
enabled flag and the remembered remote survive reboots, using CrossInk's existing
settings store and i18n pipeline.

## Requirements

- Functional: a `Bluetooth` row under Settings → System that opens a dedicated screen.
- Functional: the screen offers on/off, Pair New Remote, Reconnect Remote (when a bond
  exists), Forget Remote (with confirmation) and a live status line.
- Functional: `bluetoothEnabled`, bonded address, bonded name and bonded address type
  persist in `/.crosspoint/crossink-settings.json` and are restored on boot.
- Functional: all user-facing text goes through `tr(STR_*)`; Vietnamese is translated.
- Non-functional: settings are written only on explicit user actions — never per frame.
- Non-functional: the screen must render correctly on 528×792 (X3) and 480×800 (X4)
  with the existing `UITheme`/`GUI` helpers, including button-only devices (no touch).

## Architecture

- **Persistence** reuses `CrossPointSettings` (`src/CrossPointSettings.h:760-764`,
  `src/CrossPointSettings.cpp:41-43`, `:441-510`, `:511+`, `:918-960`) — the same choice
  the fork made in its `JsonSettingsIO` patch. The bond *keys* stay in NVS
  (`CONFIG_BT_NIMBLE_NVS_PERSIST=y`, `CONFIG_BT_NIMBLE_MAX_BONDS=3`); the settings file
  only remembers which device the user paired so the UI can show it and reconnect.
- **Menu plumbing** follows the repo's own pattern:
  `buildSystemSettingsParentList` (`src/SettingsList.h:1041-1054`) gains the row,
  `SettingAction` (`src/activities/settings/SettingsActivity.h:15-45`) gains the action,
  and `SettingsActivity::toggleCurrentSetting` (`src/activities/settings/SettingsActivity.cpp`
  action switch starting at line ~1130) gains the launch case using
  `makeUniqueNoThrow` + `startActivityForResult`, exactly like `KOReaderSync` /
  `Network` do at lines 1146-1166.
- **The screen itself** is a normal activity in `src/activities/settings/`, rendering a
  `UITheme` list with the existing status bar; it polls the manager each `loop()` and
  calls `requestUpdate()` only when the displayed state changes (e-ink: never repaint
  in a tight loop).

Screen content (v1, five rows):

| Row | Type | Behaviour |
| --- | --- | --- |
| Bluetooth | toggle | On → `enable()` (failure clears the flag, logs, shows the error); Off → `disable()` (bond kept). Saves once per change. |
| Pair New Remote | action | Starts a bounded 20 s worker: scan → pick a high-confidence page-turner/keyboard candidate (or the single connectable HID candidate, the fork's fail-safe rule) → connect → discover HID → subscribe → only then save address/name/type and `saveToFile()`. |
| Reconnect Remote | action | Only shown when a bond exists; one bounded attempt with the saved address. |
| Forget Remote | action | `ConfirmationActivity`, then delete **all** bonds (`NimBLEDevice::deleteAllBonds()`, so NVS cannot accumulate the 3-bond limit) plus the settings fields, and one save. |
| Status | text | `Disabled` / `Idle` / `Scanning…` / `Pairing…` / `Reconnecting…` / `Connected: <name>` / `Error: <lastError>` |

## Related Code Files

- Create: `src/activities/settings/BluetoothSettingsActivity.h`
- Create: `src/activities/settings/BluetoothSettingsActivity.cpp`
- Modify: `src/CrossPointSettings.h` (new fields near the input/settings block around
  lines 540-560; keep them `uint8_t` / `char[]` so `SettingInfo::String` offset
  arithmetic keeps working)
- Modify: `src/CrossPointSettings.cpp` (`toJson` at line 441 and `fromJson` at line 511:
  add `bluetoothEnabled`, `bleBondedDeviceAddr`, `bleBondedDeviceName`,
  `bleBondedDeviceAddrType`; clamp and NUL-terminate on load, exactly as the fork's
  `JsonSettingsIO` patch does)
- Modify: `src/SettingsList.h` (`buildSystemSettingsParentList`, line 1041: add the row,
  raise `reserve(8)` to 9, gate with `#if CROSSINK_APP_CAP_BLE`)
- Modify: `src/activities/settings/SettingsActivity.h` (add `Bluetooth` to
  `SettingAction`)
- Modify: `src/activities/settings/SettingsActivity.cpp` (include + launch case, gated
  with `#if CROSSINK_APP_CAP_BLE`)
- Modify: `lib/I18n/translations/english.yaml` (new `STR_*` keys next to
  `STR_WIFI_NETWORKS` at line 37)
- Modify: `lib/I18n/translations/vietnamese.yaml` (Vietnamese translations next to
  `STR_WIFI_NETWORKS` at line 42)
- Generated (never hand-edit, regenerate): `lib/I18n/I18nKeys.h`, `I18nStrings.h`,
  `I18nStrings.cpp` via `scripts/gen_i18n.py`

## Implementation Steps

1. Add the four settings fields, wire them through `toJson`/`fromJson`, and confirm a
   save/load round-trip by inspecting `/.crosspoint/crossink-settings.json` on the SD
   card after changing the toggle.
2. Add the i18n keys to `english.yaml` and `vietnamese.yaml`. Reuse existing keys where
   they already exist (there are already `STR_SCANNING`, `STR_CONNECTED`-style strings —
   check `lib/I18n/I18nKeys.h` first and only add what is genuinely new, per the repo's
   "no duplicated strings" preference). Then run
   `python3 scripts/gen_i18n.py` and commit the regenerated files together with the YAML.
3. Add the `SettingAction::Bluetooth` case and the `System` row, both gated on
   `CROSSINK_APP_CAP_BLE`, so the simulator and BLE-off builds show nothing new.
4. Implement `BluetoothSettingsActivity` following the simplest existing settings screen
   (a `UITheme` list, `ButtonNavigator` for selection, `ConfirmationActivity` for the
   destructive row). Keep it under ~500 lines — no debug monitor, no learn wizard.
5. Wire the rows to the manager API from Phase 1/2 (`enable`, `disable`, `startPairNew`,
   `startReconnect`, `forgetBond`, status accessors). Use
   `makeUniqueNoThrow<BluetoothSettingsActivity>(...)` and check for `nullptr`
   (`lib/Memory/Memory.h`). When a *different* device is paired successfully, delete the
   previously stored bond so `CONFIG_BT_NIMBLE_MAX_BONDS=3` cannot fill up with stale
   entries while settings remember only one device. Remove the temporary enable path from
   Phase 1 step 8 in this same phase.
6. Poll the manager state in `loop()` and `requestUpdate()` only on change; make sure
   the screen does not block while a worker runs (the worker owns the radio work).
7. Verify persistence: enable, pair, reboot, and confirm the settings screen shows the
   remembered device and that BLE comes back enabled.

## Success Criteria

- [ ] Settings → System shows Bluetooth on `default`/`debug` and not on the simulator or
      on `x4-pro` (capability `0`).
- [ ] Toggle on/off works, the status line updates, and `crossink-settings.json` contains
      the four new keys with the expected values.
- [ ] Pair New Remote succeeds with the user's remote and the saved name/address survive
      a reboot.
- [ ] Forget Remote removes the bond (a subsequent Reconnect row disappears and the
      remote must be paired again), and pairing a different remote does not leave the old
      bond in NVS.
- [ ] The temporary enable path from Phase 1 step 8 is gone; the Settings toggle is the
      only way to turn Bluetooth on.
- [ ] Vietnamese UI shows translated strings; other languages fall back to English with
      a generator INFO line and no missing-key warning noise beyond that.
- [ ] No settings write happens while the screen is idle (verify by watching the log or
      file timestamp).
- [ ] `scripts/gen_i18n.py` was run; `lib/I18n/I18nKeys.h` and `I18nStrings.*` changes are
      generator output only.

## Risk Assessment

- **Generated-file edits**: touching `I18nKeys.h`/`I18nStrings.*` by hand is explicitly
  forbidden (AGENTS.md *Generated Files*); only the YAML sources change by hand.
- **Settings file growth**: four new keys in an already large JSON document. Keep the
  bonded name bounded (32 bytes) and do not add a device list.
- **Blocking UI**: any direct scan/connect call from `loop()` or `render()` would freeze
  the screen; all radio work stays in the worker task (Phase 4 owns the lock policy).
- **Row-count assumptions**: `buildSystemSettingsParentList` has a `reserve(8)` and other
  code counts system rows in places; update the reserve and check for any count
  assertion similar to the controls-menu counts (`SettingsActivity.cpp:330-345`).
