---
phase: 2
title: "Hid Input Mapping"
status: pending
priority: P1
effort: "1.5d"
dependencies: [1]
---

# Phase 2: Hid Input Mapping

## Overview

Turn raw HID input reports into CrossInk actions: a BLE keyboard drives the existing
button paths, and a BLE page turner produces real page turns in the EPUB, TXT and XTC
readers — without adding a new input abstraction.

## Requirements

- Functional: a standard HID boot-protocol keyboard report (`[modifier, reserved,
  k1..k6]`) produces press/release edges for the right logical buttons.
- Functional: consumer-control reports (Play/Pause, Next/Prev track, Volume Up/Down)
  produce one page turn per press.
- Functional: the cheap page-turner profiles (Free2/Free3-style rolling keycodes,
  GameBrick/IINE A-B and Up/Down) produce the same page turns.
- Functional: a held/repeating key must not produce a burst of page turns; a release
  re-arms the key.
- Non-functional: parsing happens in `loop()` only; the NimBLE callback enqueues a POD.
- Non-functional: no `std::string`/`std::vector` growth per report; no `std::function`.

## Architecture

Two output paths, deliberately different:

| Input | Output | Why |
| --- | --- | --- |
| Keyboard keys (arrows, PageUp/PageDown, Space/Enter, Esc/Backspace) | virtual raw buttons via `HalGPIO::setVirtualButtonState(BTN_*)` | Inherits every existing mapping for free: `MappedInputManager::mapButton` (`src/MappedInputManager.cpp:132-181`) applies reader orientation swaps, front/side button remapping and `menuButton()` direction swaps. Arrow keys therefore navigate menus and PageUp/PageDown page the book through the *user's own* side-button configuration (`CrossPointSettings.h:548-550` defaults: Up = previous page, Down = page turn). |
| Page-turner keys (consumer control + profile matches) | `MappedInputManager::injectRelease(Button::Right or Left)` from `loop()` | Config-independent: the readers already treat an injected release as a page turn. This is the existing Chord Page Turn mechanism (`src/main.cpp:796-811`), consumed via `hasInjectedRelease` (`src/MappedInputManager.h:92-97`) and read by `ReaderUtils::detectPageTurn` (`src/activities/reader/ReaderUtils.h:216-235`, used by `EpubReaderActivity.cpp:3322` and `TxtReaderActivity.cpp:351`) and directly by `XtcReaderActivity.cpp:409,441`. |

Page-turn requests are applied **only while `activityManager.isReaderActivity()`** and
**only when the device is not Quick Locked or showing a modal**; otherwise they are logged
and dropped. Two reasons: a media key must not silently move a selection in Settings, and
BLE input must never page through a deliberately locked device. While
`buttonShortcutController.isQuickLocked()` is true the manager additionally clears virtual
buttons and discards pending requests, so a BLE keyboard cannot satisfy the quick-lock
unlock gesture (`src/main.cpp:1779-1802`).

Report handling rules:

- Keyboard reports are diffed against the previous report for the same device to get
  true press/release edges (real key-up releases the virtual button; a held key keeps
  the button down, which keeps the existing "hold" semantics honest without feeding
  virtual buttons into `getHeldTime()` — see Phase 1 step 6).
- Consumer/profile keys are one-shot: a press produces exactly one action, repeats are
  suppressed until the key is released, and re-presses within 250 ms are ignored
  (the fork's `VIRTUAL_BUTTON_REPRESS_DEBOUNCE_MS`, `%TEMP%/.../lib/hal/HalGPIO.cpp`).
- Report discovery subscribes to the HID service (0x1812) report characteristics
  (0x2A4D) and, when the Report Reference descriptor (0x2908) identifies input reports,
  prefers those. Cheap remotes often expose only one broad characteristic, so a bounded
  fallback subscribes to every notifiable characteristic — the fork documented this as
  the "broad HID report characteristic enumeration" that made Free3 work.
- Reports whose length exceeds `MAX_HID_REPORT_BYTES` (16) are truncated for the queue
  but parsed from the queue copy, never from the callback buffer.

## Related Code Files

- Create: `lib/hal/DeviceProfiles.h` (compact port of
  `%TEMP%/crosspoint-x3-ble-page-turner/lib/hal/DeviceProfiles.h`: a POD
  `DeviceProfile { const char* name; uint8_t pageUpCode; uint8_t pageDownCode;
  const uint8_t* forwardCodes; size_t forwardCount; const uint8_t* backCodes;
  size_t backCount; }` plus keycode constants)
- Create: `lib/hal/DeviceProfiles.cpp` (static tables; no heap)
- Modify: `lib/hal/BluetoothHIDManager.h` (event struct, `PageTurnRequest`, profile
  lookup, `pressedKeycodes` state, `lastReport` per device)
- Modify: `lib/hal/BluetoothHIDManager.cpp` (report parsing, key mapping, dedup)
- Modify: `src/main.cpp` (drain + apply: virtual buttons already handled by Phase 1;
  add the page-turn request branch next to the existing chord pattern at lines 796-811,
  and the reader-context check via `activityManager.isReaderActivity()`)

## Implementation Steps

1. Port the keycode constants and the profile table from the fork's `DeviceProfiles.*`
   into `lib/hal/DeviceProfiles.{h,cpp}`, keeping only: generic keyboard usages,
   consumer-control usages, Free2/Free3-style rolling code groups, GameBrick/IINE.
   Tables are `static const` so they stay in flash (AGENTS.md rule 3).
2. Add the event type to the manager: a fixed-size POD
   `BleInputEvent { uint8_t slot; uint8_t kind; uint8_t key; bool pressed; }` in a
   queue of depth 16. `onHIDNotify()` copies at most 16 report bytes plus the slot id.
3. Implement `processEvents()`: drain the queue, resolve the device slot to a profile,
   parse the report (keyboard array vs consumer vs profile), map to either a raw button
   or a page-turn request, apply the dedup/debounce rules, and write the result:
   `gpio.setVirtualButtonState(...)` for keyboard keys, `_pendingPageTurn` for
   page-turner keys.
4. Implement `consumePageTurnRequest()` returning `-1/0/+1` (single consumer, cleared
   on read). Drain and apply in **two places**, matching the existing loop structure:
   - **Top of `loop()`** (before `mappedInputManager.update()`, line 1629):
     `processEvents()` only — virtual keyboard buttons are latched here so
     `wasPressed`/`wasReleased` are true on this frame.
   - **After the Quick Lock and modal gates** (in the same region as the chord page turn,
     `src/main.cpp:1779-1811`): consume the pending page-turn request and apply it:
     - `+1` → `mappedInputManager.injectRelease(MappedInputManager::Button::Right)`
     - `-1` → `mappedInputManager.injectRelease(MappedInputManager::Button::Left)`
     - then `activityManager.loop()` and `mappedInputManager.clearInjectedReleases()`
       exactly like `dispatchButtonShortcut` does at `src/main.cpp:796-811`, set
       `lastActivityTime = millis()`, and **return** so the same iteration does not run
       the activity loop twice.
   Drop the request (with a `LOG_DBG`) when
   `!activityManager.isReaderActivity() || activityManager.blocksGlobalInput()` or when
   the device is quick locked.
5. Add bounded diagnostics: for the first 20 reports of each newly connected device,
   `LOG_INF("BT", "report len=%u bytes=%02X ...")`, so an unsupported remote can be
   captured from serial. Keep the counter per device and stop after the cap.
6. Test on hardware with a keyboard and the user's page turner; for an unmapped remote,
   use the diagnostics from step 5 to add its codes to `DeviceProfiles`.

## Success Criteria

- [ ] `pio run -e default` succeeds; `pio run -e simulator` unchanged.
- [ ] A BLE keyboard: arrows navigate Home/lists, Enter/Space confirm, Esc/Backspace go
      back, PageUp/PageDown turn pages in an EPUB.
- [ ] A BLE page turner: forward/back turn pages in EPUB, TXT and XTC readers; exactly
      one page per press; holding a key does not produce a burst.
- [ ] A page turner key pressed while in Settings does not move the selection.
- [ ] With Quick Lock enabled, BLE keys and BLE page turns do nothing until it is unlocked
      by a physical gesture, and no BLE press can unlock it.
- [ ] A single BLE page turn produces exactly one `activityManager.loop()` call per frame
      (no double-dispatch) and one page turn on the panel.
- [ ] Physical buttons, tilt page turn, and the Power+Down screenshot chord still behave
      exactly as before with a remote connected.
- [ ] Serial diagnostics show the raw report bytes for an unmapped device (verified with
      at least one report dump captured during testing).

## Risk Assessment

- **Unknown remote protocol** (R8): if the user's remote is neither a keyboard-emulating
  nor a known-profile device, this phase cannot make it work. The deliverable then is the
  captured report dump plus a profile entry — say so plainly instead of shipping a
  half-working mapping.
- **Repeat storms**: cheap remotes can re-send a latched code every ~50 ms. Without the
  one-shot + 250 ms debounce this floods the e-ink panel; test with a deliberately held
  button.
- **Double input**: a keyboard that also emits consumer-control reports could map one
  keypress to two actions. Dedup is per device *and* per keycode; verify with a
  consumer-capable keyboard if one is available.
- **Latch ordering**: `HalGPIO::update()` is called from `MappedInputManager::update()` and
  also directly in `main.cpp` (lines 1599-1601, wake-absorb path). If a desired virtual
  state is latched more than once inside one frame, a key could produce two edges. Watch
  for duplicate page turns in the serial log and, if it happens, latch only when the
  desired state differs from the current state.
- **Injected-release leakage**: an injected release that is never cleared would fire on
  a later frame. Always clear in the same iteration (the existing pattern does).
