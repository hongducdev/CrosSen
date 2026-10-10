---
phase: 4
title: "Power Wifi And Reconnect"
status: pending
priority: P1
effort: "1.5d"
dependencies: [1, 2, 3]
---

# Phase 4: Power Wifi And Reconnect

## Overview

Make BLE behave in a battery-powered reader: it must not block or corrupt sleep, must
get out of the way of Wi-Fi, must survive the remote being switched off, and must never
be the reason the device stays awake.

## Requirements

- Functional: deep sleep tears BLE down first; the next boot reconnects when Bluetooth
  is enabled and a bond exists.
- Functional: while any Wi-Fi activity is running, BLE is released; when Wi-Fi stops,
  BLE comes back if the user left it enabled.
- Functional: a disconnect while awake triggers at most one bounded reconnect attempt
  per backoff window, capped per boot.
- Functional: auto-sleep is deferred only while a BLE worker is actually running, and
  never for longer than that worker's own timeout.
- Non-functional: the CPU stays at normal speed while a worker runs
  (`HalPowerManager::Lock`, `lib/hal/HalPowerManager.h:82-97`).
- Non-functional: no unbounded loops or waits anywhere in the sleep path.

## Architecture

- **Sleep** (`src/main.cpp:1087-1140`): before `APP_STATE.saveToFile()`, if BLE is
  enabled: stop the scan, disconnect, delete clients, `deinit`, log the heap released.
  Unlike the fork (`%TEMP%/.../src/main.cpp`, `enterDeepSleep`), **do not defer sleep on
  a BLE teardown failure** — deep sleep resets the radio anyway; log the error and
  continue. Only a *running worker* defers sleep.
- **Reconnect on boot**: after the Phase 1 `enable()` in `setup()`
  (`src/main.cpp:1346-1360`), if a bond exists, ask the manager to reconnect; the manager
  delays the first attempt a few seconds so the boot screen paints and the UI is
  responsive first.
- **Wi-Fi guard** — one choke point in `loop()` rather than edits in eight activities
  (the Wi-Fi users are `CrossPointWebServerActivity.cpp:232,287`,
  `WifiSelectionActivity.cpp:303-355`, `NearbyStatsSyncActivity.cpp:282,315`,
  `KOReaderSyncActivity.cpp:182`, `NearbyBookPositionSyncActivity.cpp:839,863`,
  `ClockSyncActivity.cpp:40`, `FontDownloadActivity.cpp:193,207`,
  `OtaUpdateActivity.cpp:98`, plus the settings cleanup at `SettingsActivity.cpp:1163`):

  ```text
  wifiActive = (WiFi.getMode() != WIFI_OFF)
  if (wifiActive && ble.isEnabled() && !ble.isWorkerRunning()):
      ble.disable(); bleSuspendedForWifi = true        // log once
  else if (!wifiActive && bleSuspendedForWifi && SETTINGS.bluetoothEnabled):
      bleSuspendedForWifi = false; ble.enable(); ble.startReconnect(6000)
  ```

  Note that several network flows already `silentRestart()` (a reboot clears everything,
  `src/SilentRestart.h:14-24`); this guard covers the in-process ones.
- **Auto-sleep deferral** (`src/main.cpp:1804-1810`): add
  `if (ble.isWorkerRunning()) { LOG_DBG("SLP", "Auto-sleep deferred: BLE worker active");
  delay(50); return; }` before the timeout check, mirroring the fork's approach, plus
  `powerManager.setPowerSaving(false)` while a worker runs (near the loop-delay block at
  `src/main.cpp:1890-1910`).
- **Auto-reconnect policy**: on a disconnect event with a bond and BLE enabled, schedule
  attempts with a 30 s scan window and a growing delay (e.g. 5 s → 30 s → 120 s), maximum
  3 attempts per boot; any local button press or reader page turn resets the attempt
  budget (the user is present again). After the cap, stay idle until the user asks for a
  reconnect, a Wi-Fi session ends, or the device wakes from sleep.
- **No crash-loop guard file** in v1. If hardware testing shows reconnect loops are a
  real failure mode, add the fork's guard as a follow-up (recorded here so it is not
  forgotten).

## Related Code Files

- Modify: `src/main.cpp` (include gated by `#if CROSSINK_APP_CAP_BLE`; `enterDeepSleep`
  at line 1087; `setup()` BLE block at lines 1346-1360; `loop()` BLE poll at line 1629;
  Wi-Fi guard and auto-sleep deferral around lines 1680-1810; power-saving block at
  lines 1890-1910)
- Modify: `lib/hal/BluetoothHIDManager.h` / `.cpp` (worker lifecycle, backoff counters,
  `isWorkerRunning()`, disconnect handling, `armReconnectAfterWake`-equivalent state)
- Modify: `lib/hal/HalPowerManager.*` only if a lock scope helper is missing (do not
  change its semantics)

## Implementation Steps

1. Implement `disableForSleep()` (or make `disable()` safe to call from the sleep path):
   stop scan → disconnect → delete clients → `deinit(true)` → log
   `ESP.getFreeHeap()` before/after. Call it from `enterDeepSleep` before
   `APP_STATE.saveToFile()`. Verify the device still sleeps in the same time as before
   (measure wake-to-sleep latency in the log).
2. Implement reconnect-on-boot: after `enable()` in `setup()`, if a bond exists, start
   the worker with a short delay; make the delay skip when the boot route is a network
   boot target (Wi-Fi wins).
3. Add the Wi-Fi guard in `loop()` exactly as specified above, with a single log line per
   transition (not per frame) and no re-enable churn if `WiFi.getMode()` flaps during
   association.
4. Add the auto-sleep deferral and the power-saving override for a running worker.
5. Implement the backoff/attempt policy inside the manager, with the per-boot cap and the
   user-activity reset. Log every attempt with its outcome
   (`LOG_INF("BT", "reconnect attempt %u: ...")`).
6. Verify on hardware: sleep/wake with a remote, remote power-cycle, Wi-Fi session with
   BLE on, and the no-remote idle battery case.

## Success Criteria

- [ ] With BLE enabled and connected, the device still enters deep sleep at the configured
      timeout and wakes on the power button; BLE reconnects after wake without opening
      Settings.
- [ ] With BLE enabled and **no** remote nearby, the device still auto-sleeps at the
      timeout (log shows the sleep, not repeated deferrals).
- [ ] Starting File Transfer (Wi-Fi) with BLE on releases BLE, Wi-Fi works, and after
      leaving the activity BLE re-enables and reconnects.
- [ ] Turning the remote off and on again reconnects within the backoff budget; the
      attempt count never exceeds the per-boot cap.
- [ ] A page turn or button press during a reconnect window resets the attempt budget.
- [ ] No watchdog resets, no panics, and `logMemoryStats` shows no downward heap drift
      across a 10-minute idle session with BLE on.

## Risk Assessment

- **R3 (battery / sleep)**: this phase is where a mistake keeps the device awake. Test the
  no-remote idle case explicitly and compare against the same test with BLE off.
- **Deinit while connected**: deleting a NimBLE client while a notification is in flight
  is the classic hang; always stop the scan and disconnect first, and refuse `disable()`
  while a worker runs.
- **Wi-Fi flapping**: `WiFi.getMode()` can transition several times during association;
  the guard must be edge-triggered on the *activity* state, not on raw mode reads.
- **Reconnect storms**: a remote that advertises but never completes HID subscription
  would retry forever without the per-boot cap. Keep the cap and log the give-up.
- **Coexistence**: this design intentionally does not exercise Wi-Fi + BLE at the same
  time. If someone later removes the guard, the SW coexistence in the prebuilt sdkconfig
  (`CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`) is not a tested configuration on this board —
  say so in the docs rather than implying support.
