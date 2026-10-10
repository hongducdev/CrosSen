---
phase: 1
title: "Stack Bringup"
status: pending
priority: P1
effort: "1d"
dependencies: []
---

# Phase 1: Stack Bringup

## Overview

Make the C3 firmware able to bring a NimBLE HID central up and down behind a build
capability, with a measurement of what it costs in flash and RAM, and with the
`HalGPIO` virtual-button seam in place. No HID semantics yet — this phase ends with
"Bluetooth can be switched on from a temporary code path, the stack initialises, and
we know the flash/RAM numbers".

## Requirements

- Functional: `BluetoothHIDManager::getInstance().enable()` / `disable()` /
  `isEnabled()` initialise and tear down the NimBLE stack on `default`/`debug`;
  every other env compiles it out.
- Functional: `HalGPIO` exposes virtual raw-button injection that behaves like a real
  press/release edge for `isPressed`, `wasPressed`, `wasAnyPressed`, `wasReleased`,
  `wasAnyReleased`.
- Non-functional: zero flash/RAM change for envs where the capability is `0`.
- Non-functional: the NimBLE callback path never touches app state (queue only).
- Non-functional: no bare `new`, no exceptions, no `std::function` in the input path.

## Architecture

- `include/AppCapabilities.h` gains `CROSSINK_APP_CAP_BLE` with the same `#error`
  contract as `CROSSINK_APP_CAP_TOUCH` / `CROSSINK_APP_CAP_USB_DRIVE`.
- `lib/hal/BluetoothHIDManager.{h,cpp}` owns the NimBLE lifecycle, a fixed-size POD
  event queue, and a bounded worker task. It exposes plain methods — no
  `std::function` setters — so `src/main.cpp` calls `processEvents(...)` and the
  manager writes virtual buttons / page-turn requests directly.
- `lib/hal/HalGPIO.{h,cpp}` gains `virtualButtonState` / `desiredVirtualButtonState` /
  `previousVirtualButtonState` bitmaps latched in `update()`, OR-ed into the four
  button query methods. This is the fork's proven design
  (`%TEMP%\crosspoint-x3-ble-page-turner/lib/hal/HalGPIO.cpp`, virtual button diff).
- `lib/hal` is already excluded from the simulator (`platformio.ini`,
  `simulator-base.lib_ignore`), so nothing here can break native builds.

## Related Code Files

- Create: `lib/hal/BluetoothHIDManager.h`
- Create: `lib/hal/BluetoothHIDManager.cpp`
- Modify: `include/AppCapabilities.h` (add the BLE capability block next to the
  existing two, ~lines 8-30)
- Modify: `platformio.ini` (`[base].lib_deps` → add `h2zero/NimBLE-Arduino @ 2.5.0`;
  `-DCROSSINK_APP_CAP_BLE=1` in `[env:default]` and `[env:debug]`; `=0` in
  `[simulator-base].build_flags` (inherited by all four simulator envs), and `=0`
  explicitly in `[env:sticky]`, `[env:sticky-debug]`, `[env:x4-pro]`,
  `[env:x4-pro-debug]`, `[env:x4-classic]`, `[env:x4-classic-debug]`)
- Modify: `lib/hal/HalGPIO.h` (virtual button members + declarations; the existing
  button query block is at lines 76-84, the `inputMgr` member at line 44)
- Modify: `lib/hal/HalGPIO.cpp` (`update()` at line 152; the delegating query methods
  at lines 158-172; `getHeldTime()` at line 172)
- Modify: `src/main.cpp` (include + lifecycle in `setup()` after
  `SETTINGS.loadFromFile()` at line 1346; a `#if CROSSINK_APP_CAP_BLE` guarded poll in
  `loop()` before `mappedInputManager.update()` at line 1629)

## Implementation Steps

1. **Baseline first.** In WSL, build the untouched tree:
   `pio run -e default 2>&1 | tee /tmp/pio-baseline.log`. Record the reported
   Flash/RAM usage **and** the raw `firmware.bin` size
   (`ls -l .pio/build/default/firmware.bin`) plus `scripts/check_firmware_size.py`
   output. This number is the reference for R1 and must be written into this phase's
   completion note.
2. Add `CROSSINK_APP_CAP_BLE` to `include/AppCapabilities.h` following the existing
   `#error`-when-undefined pattern, and define it explicitly in every env in
   `platformio.ini` (default/debug `1`, everything else `0`). Build `default` again —
   it must still compile with no BLE code present.
3. Add `h2zero/NimBLE-Arduino @ 2.5.0` to `[base].lib_deps` (firmware envs only; the
   simulator envs have their own `lib_deps`). Keep `lib_ignore = BLE` unchanged.
   This step needs network access for `pio pkg install`; confirm the resolved version is
   exactly `2.5.0` (`pio pkg list -e default`) and that the simulator envs are unaffected.
4. Create `BluetoothHIDManager` with the minimum surface:
   `getInstance()`, `enable()`, `disable()`, `isEnabled()`, `lastError()`,
   `processEvents(HalGPIO&, MappedInputManager&)`, `isWorkerRunning()`,
   `startScan()/stopScan()`, `startPairNew(timeoutMs)`, `startReconnect(timeoutMs)`,
   `forgetBond()`, plus state accessors for the settings screen. Private internals:
   `ensureQueues()`, `workerEntry()` (static), `runWorker()`, `onHIDNotify()` (static
   C-compatible callback that only enqueues).
5. Implement `enable()`: no Wi-Fi check here (that belongs to Phase 4) but a clear
   `LOG_ERR` + `return false` when `NimBLEDevice::init()` cannot proceed;
   `NimBLEDevice::setPower(ESP_PWR_LVL_P9)`, 1M PHY, `setSecurityIOCap(NO_INPUT_OUTPUT)`,
   `setSecurityAuth(true, false, false)` — the fork's validated configuration
   (`%TEMP%/.../lib/hal/BluetoothHIDManager.cpp:744-750`). `disable()` stops the scan,
   disconnects, deletes clients and calls `NimBLEDevice::deinit(true)` so the heap comes
   back; it must refuse (and log) while a worker is running.
6. Implement `HalGPIO` virtual buttons: three bitmaps, `setVirtualButtonState()`,
   `injectButtonPress()`, `clearVirtualButtons()`, a 250 ms re-press debounce, and
   OR-ing into `isPressed` / `wasPressed` / `wasAnyPressed` / `wasReleased` /
   `wasAnyReleased`. Deliberately **do not** feed virtual buttons into `getHeldTime()`:
   the fork had to add per-button held-time tracking to stop synthetic presses from
   reading as long presses, and v1 avoids that class of bug by leaving held-time
   physical-only. Document the consequence (no hold-to-skip-chapter from BLE).
7. Wire the lifecycle in `src/main.cpp`: right after `SETTINGS.loadFromFile()`
   (line 1346), if `SETTINGS.bluetoothEnabled` then `enable()`; on failure log, clear
   the setting and `SETTINGS.saveToFile()`. In `loop()`, before
   `mappedInputManager.update()` (line 1629), call `processEvents(gpio,
   mappedInputManager)` inside `#if CROSSINK_APP_CAP_BLE`. Nothing else in this phase.
8. Measure: with a temporary `#if CROSSINK_APP_CAP_BLE` debug block (or by enabling via
   the not-yet-existing setting using a serial command), log
   `ESP.getFreeHeap()`, `ESP.getMaxAllocHeap()` and `heap_caps_get_largest_free_block`
   before enable and after enable. Record the delta in the completion note. **This
   temporary enable path must be removed in Phase 3**, when the Settings toggle becomes
   the only way to turn Bluetooth on — do not leave dead test hooks behind.
9. Rebuild `default` and record the after-change flash/RAM usage; run
   `pio run -e simulator` to prove the native build is untouched.

## Success Criteria

- [ ] Baseline `default` firmware size recorded before any BLE code lands.
- [ ] `pio run -e default` succeeds with `CROSSINK_APP_CAP_BLE=1` and
      `scripts/check_firmware_size.py` passes; after-change size recorded with the delta.
- [ ] `pio run -e simulator` (and `-e simulator-X3`) still build; no `lib/hal` symbol
      referenced from native code.
- [ ] `enable()` then `disable()` returns the heap to within a few hundred bytes of the
      pre-enable value (no leak in the manager itself).
- [ ] A physical button press still behaves identically with BLE compiled in but off
      (no regression in `wasPressed`/`wasReleased`/held-time paths).
- [ ] BLE-disabled envs (e.g. `x4-pro`) build with no size change beyond noise.

## Risk Assessment

- **R1 (flash)** is the phase's main gate: if `default` + NimBLE exceeds the partition,
  stop and trim `custom_i18n_builtin_langs` before writing any further code, then
  re-measure and report. Do not proceed on a failing size gate.
- **R2 (RAM)**: the enable/disable delta is the number that decides whether reading with
  BLE on is viable on the X3. If the delta is far above the 35–55 KB estimate, reduce
  worker stack size (start at 8192, verify with `uxTaskGetStackHighWaterMark`) and
  confirm the NimBLE host stack from the prebuilt sdkconfig (5120) is what we think.
- **Deinit correctness**: `NimBLEDevice::deinit(true)` while a client or scan is live can
  hang. Always stop scan, disconnect and delete clients first, and refuse `disable()`
  while `isWorkerRunning()`.
- **Silent capability drift**: a missing `-DCROSSINK_APP_CAP_BLE` must fail the build via
  the `#error`, not default to off — keep the AppCapabilities pattern exact.
