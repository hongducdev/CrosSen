---
phase: 5
title: "Docs Gates And Verification"
status: pending
priority: P1
effort: "1d"
dependencies: [1, 2, 3, 4]
---

# Phase 5: Docs Gates And Verification

## Overview

Close the feature with the artefacts this repo expects: documentation, a changelog
entry, a build-level symbol gate so BLE cannot leak into BLE-off firmware, the full
build/test matrix, and a hardware checklist the user can actually run on the X3.

## Requirements

- Functional: `docs/bluetooth.md` describes pairing, supported devices, limits and how to
  capture a report from an unsupported remote.
- Functional: `CHANGELOG.md` gains an `Added` entry describing the user-facing change.
- Functional: `scripts/check_app_ble_gate.py` fails a build that references BLE symbols
  while `CROSSINK_APP_CAP_BLE=0`.
- Non-functional: every claim in the docs matches what was actually verified; unverified
  devices and unverified combinations are listed as such.
- Non-functional: no CI/workflow files (this repo builds locally by policy).

## Architecture

- The gate mirrors `scripts/check_app_touch_gate.py`: read `CPPDEFINES` to find
  `CROSSINK_APP_CAP_BLE`, and when it is `0` scan the ELF for forbidden symbols
  (`BluetoothHIDManager`, `NimBLEDevice`, `DeviceProfiles`, `nimble_`), failing the build
  with the offending symbols listed. Registered in `platformio.ini` `extra_scripts`
  `post:` next to `check_firmware_size.py` and `check_app_touch_gate.py`.
- Documentation lives at `docs/bluetooth.md` and is linked from `docs/index.md` if that
  file lists features.

## Related Code Files

- Create: `docs/bluetooth.md`
- Create: `scripts/check_app_ble_gate.py`
- Modify: `platformio.ini` (`extra_scripts` `post:` list)
- Modify: `CHANGELOG.md` (new `Added` bullet under the current version section)
- Modify: `docs/index.md` (link the new doc if the index enumerates features)
- Verify only (no edits expected): `lib/I18n/I18nKeys.h`, `lib/I18n/I18nStrings.*`
  (generated), `src/components/icons/*.h` (generated), `src/network/html/*.generated.h`

## Implementation Steps

1. Write `docs/bluetooth.md` covering: what works (BLE keyboard, consumer-control page
   turners, known profiles), how to pair, how to reconnect, how to forget, what BLE costs
   (flash/RAM numbers measured in Phase 1), the Wi-Fi mutual exclusion rule, the
   sleep/reconnect behaviour, and an explicit *Known limits* section:
   - one HID device at a time;
   - third-party remotes that match no profile are unvalidated — the keyboard and
     consumer-control paths may still work, but that is not a promise;
   - no on-device learn wizard yet (a serial report capture is the way to add a device);
   - holding a remote button does **not** skip a chapter (holding a *keyboard* key does);
   - pairing uses Just Works bonding; a remote that demands a passkey will not pair;
   - Bluetooth reduces free RAM, so very large or image-heavy books are safer with it
     switched off (state the measured numbers, not a guess).
2. Add the changelog entry under `Added`, describing the user-visible feature only.
3. Write `scripts/check_app_ble_gate.py`, register it in `platformio.ini`, and prove it
   both ways: it passes on `default` (capability `1`) and passes on `x4-pro` (capability
   `0`, no BLE symbols). If it cannot fail, temporarily reference a BLE symbol in a
   cap-0 env and confirm the gate rejects the build, then revert.
4. Run the build matrix in WSL with a generous timeout and streamed logs:
   - `pio run -e default` → must pass `check_firmware_size.py`; record Flash/RAM and the
     `firmware.bin` size versus the Phase 1 baseline.
   - `pio run -e simulator` and `pio run -e simulator-X3` → native build must be unchanged.
   - `pio run -e x4-pro` → BLE-off env must show no size regression and no BLE symbols.
5. Run `pio check -e default --fail-on-defect low --fail-on-defect medium --fail-on-defect high`
   and record the result; fix anything the new code introduced (do not fix pre-existing
   findings in untouched files).
6. Run the simulator smoke test (`python3 scripts/run_simulator_smoke_test.py`) to prove
   the app-flow regression tripwire still passes.
7. Run `clang-format` on every touched C++ file.
8. Produce the hardware checklist for the X3 (below) and hand it to the user with the
   actual numbers from steps 4-6 — no claim of success without the command output.

## Hardware verification checklist (X3)

Prerequisite: `.pio/build/default/firmware.bin` built from this work, copied to the SD
card as `update.bin` (or flashed with esptool); serial log at 115200 with the `BT` tag
visible.

| # | Step | Expected |
| --- | --- | --- |
| 1 | Boot, open Settings → System | `Bluetooth` row is present |
| 2 | Open it, turn Bluetooth on | Status becomes `Idle`/`Scanning…`; log shows the NimBLE init and the free-heap delta |
| 3 | `Pair New Remote`, put the remote in pairing mode | Status becomes `Connected: <name>`; log shows the connect, HID discovery and subscribe |
| 4 | Open an EPUB, press forward/back on the remote | Exactly one page turn per press; page counter advances; no burst when held |
| 5 | Pair a BLE keyboard (forget the remote first if only one device is supported) | Arrows navigate Home/lists, Enter/Space confirm, Esc/Backspace back, PageUp/PageDown turn pages |
| 6 | With BLE connected, open File Transfer (Wi-Fi) | BLE is released (log line), Wi-Fi works; after leaving, BLE re-enables and reconnects |
| 6b | Enable Quick Lock, then press BLE keys and a page-turn key | Nothing happens; the lock can only be released with a physical gesture |
| 7 | Let the sleep timeout expire with BLE on and the remote nearby | Device sleeps; wake with Power reconnects BLE without opening Settings |
| 8 | Leave with BLE on and the remote switched off | Device still auto-sleeps at the timeout; log shows no repeated deferrals |
| 9 | Switch the remote off and on | Reconnects within the attempt budget; attempt count never exceeds the cap |
| 10 | Regression sweep with a remote connected | Physical buttons, tilt page turn, Quick Lock, Power+Down screenshot, bookmark/clipping holds unchanged |
| 11 | Memory check with BLE connected and a large EPUB open | `logMemoryStats` shows no OOM, no failed page load, no cover failure |
| 12 | Settings → Bluetooth → Forget Remote | Bond cleared; remote must be paired again; `crossink-settings.json` fields emptied |

Reset/recovery when something misbehaves:

- Forget Remote (clears the NVS bond and the settings fields) — the first thing to try.
- Delete the four `ble*` keys from `/.crosspoint/crossink-settings.json` while the device
  is off, if the toggle or the stored device is stuck.
- The EPUB cache (`/.crosspoint/epub_<hash>/`) is unrelated to Bluetooth and does **not**
  need clearing for any BLE issue.
- If the device will not boot with BLE on, the recovery path is the same as any firmware
  problem: SD-card `update.bin` / esptool flash of a known-good build.

What to report back if the remote does not work: the serial log lines with the `BT` tag
containing the raw report bytes (Phase 2 step 5), plus the remote's advertised name.

## Success Criteria

- [ ] `docs/bluetooth.md` exists, is accurate, and states the known limits.
- [ ] `CHANGELOG.md` has the `Added` entry.
- [ ] `scripts/check_app_ble_gate.py` is registered and demonstrably rejects BLE symbols
      in a capability-0 build.
- [ ] `pio run -e default` passes the size gate, with before/after numbers reported.
- [ ] `pio run -e simulator`, `-e simulator-X3`, `-e x4-pro` all succeed.
- [ ] `pio check -e default ...` result reported, with no new findings attributable to
      this work.
- [ ] `scripts/run_simulator_smoke_test.py` passes.
- [ ] Touched C++ files are `clang-format`ed.
- [ ] The hardware checklist is handed over with the measured numbers, and the unchecked
      rows are explicitly marked as not yet verified on hardware.

## Risk Assessment

- **Overclaiming**: the strongest temptation in this phase is to write "works with BLE
  page turners" in the docs after testing one remote. Only devices actually tested may be
  listed as validated; everything else is "may work via keyboard/consumer emulation".
- **Gate false negatives**: a symbol-based gate only catches what the linker kept. Pair it
  with the capability check in `CPPDEFINES` (the touch gate does the same).
- **Size drift**: record the size in the changelog-adjacent note or the completion note so
  the next feature has a reference point; `scripts/firmware_size_history.py --commits
  HEAD~1 HEAD --env default` can produce the before/after table.
