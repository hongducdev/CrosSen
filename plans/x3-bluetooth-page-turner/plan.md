---
title: "X3 Bluetooth keyboard and page turner"
description: >-
  Port the BLE HID page-turner idea from hannah-nula/crosspoint-x3-ble-page-turner
  into CrossInk: pair a BLE keyboard or BLE page turner and let it drive reader
  page turns, menus and confirm/back, with a new Bluetooth screen under Settings.
status: pending
priority: P1
branch: "main"
tags:
  - bluetooth
  - input
  - settings
  - x3
blockedBy: []
blocks: []
created: "2026-10-09T20:13:36.538Z"
createdBy: "ck:plan"
source: skill
---

# X3 Bluetooth keyboard and page turner

## Overview

The user wants the CrossInk X3/X4 firmware to accept a **BLE keyboard** and a **BLE
page turner** (Free2/Free3-style, GameBrick/IINE-style, or any remote that emits
standard consumer-control or keyboard HID reports) as an input device, and to
expose it as a **Bluetooth** entry in Settings.

Reference fork studied for this plan: `hannah-nula/crosspoint-x3-ble-page-turner`
(read-only clone at `%TEMP%\crosspoint-x3-ble-page-turner`). That fork is a
community fork of upstream CrossPoint Reader built on **the same PlatformIO
platform pin CrossInk uses** (`pioarduino/platform-espressif32 @ 55.03.37`), so its
hardware-validated choices transfer directly. It adds:

| Fork file | Size | What it proves |
| --- | --- | --- |
| `lib/hal/BluetoothHIDManager.{h,cpp}` | 230 + 3281 lines | A NimBLE **central** can scan/pair/bond/subscribe to HID reports on the C3 X3 and inject page turns into the normal CrossPoint input path. |
| `lib/hal/DeviceProfiles.{h,cpp}` | 141 + 378 lines | Cheap page turners send non-standard keycodes; a small per-device table is required. |
| `lib/hal/BluetoothDiagnostics.cpp` | 274 lines | A persisted diagnostics ring was needed to debug real-device reconnect failures. |
| `src/activities/settings/BluetoothSettingsActivity.cpp` | 1133 lines | Pair-new / reconnect / debug-monitor UI on a 528×792 e-ink screen. |
| `src/main.cpp` diff (~96 lines) | — | BLE lifecycle in `setup()`, per-loop polling, Wi-Fi mutual exclusion, deep-sleep teardown, auto-sleep deferral while a reconnect worker runs. |
| `lib/hal/HalGPIO.cpp` diff | — | Virtual button injection straight into the existing `isPressed/wasPressed/wasReleased/getHeldTime` seam. |

The fork's release binary is `0x5b7630` bytes (`docs/x3-ble-pairfix1-validation.md:6`)
against the `0x640000` app partition, and its known limits are honest: validated on
one X3 plus one Free3/Free3-ER-style remote only, and its `Scan for devices` list is
"not the daily-driver path" (`docs/x3-ble-page-turner.md`, *Known Limits*).

**The fork is ~9,500 added lines including a learn wizard, crash-loop guards, dense/
sparse reconnect windows, a diagnostics ring, and 15 audit scripts.** This plan ports
the *mechanism* — not that whole apparatus — into CrossInk's architecture, sized for
a C3 with ~380 KB RAM, no PSRAM, ~48 KB framebuffer and a nearly full app partition.

## Scope

### In scope

- BLE HID **central** (client) role: scan, pair, bond, subscribe to HID input reports,
  reconnect, forget.
- **BLE keyboard**: arrow keys, PageUp/PageDown, Space/Enter, Back/Escape drive the
  existing app input paths (menus, lists, confirm/back, reader page turns).
- **BLE page turner**: forward/back page turns in the EPUB, TXT and XTC readers through
  the readers' existing page-turn entry points.
- Known-remote profile table, ported compactly from the fork: Free2/Free3-style rolling
  keycodes, GameBrick/IINE A-B + Up/Down, plus generic consumer-control
  (Play/Pause, Next/Prev track) and generic keyboard fallbacks.
- **Settings → System → Bluetooth**: on/off, Pair New Remote, Reconnect Remote,
  Forget Remote, live status line; new `STR_*` keys in English + Vietnamese (other
  languages fall back to English).
- Persistence of the enabled flag and the bonded device (address, name, address type)
  in the existing settings file; the bond keys themselves stay in NVS.
- Deep-sleep teardown + reconnect on wake, auto-sleep deferral while a BLE worker runs,
  and Wi-Fi mutual exclusion.
- Docs (`docs/bluetooth.md`), `CHANGELOG.md`, capability gating and a build gate script.

### Out of scope (explicitly not doing in v1)

- **S3 envs** (`sticky`, `x4-pro`, `x4-classic`): capability stays `0` in v1. Turning them
  on later is one build flag plus a build; they have PSRAM and more flash headroom.
- **Simulator BLE**: capability `0`; the manager lives in `lib/hal`, which
  `simulator-base` already excludes (`platformio.ini`, `lib_ignore = hal, WebSockets`).
- The fork's **learn wizard** and **debug monitor** activities; v1 exposes serial logs
  for report capture instead.
- The fork's reconnect state machine (dense 20-minute scans, sparse 4-hour windows,
  `ble_auto_reconnect_guard` crash-loop file, `ble_diag.log` ring).
- The fork's **reader-menu** Bluetooth entry and `exitOnSuccessfulConnect` flow.
- BLE peripheral mode (CrossInk acting as a keyboard), BLE file transfer, and
  simultaneous multiple HID devices (v1 connects one remote at a time).
- Any change under `freeink-sdk/` (no submodule gitlink bump).

## Verified environment facts (no sdkconfig change is needed)

Checked on this machine, in the platform package CrossInk actually builds against:

- `framework-arduinoespressif32/package.json` → Arduino core **3.3.11**; there is **no
  bundled NimBLE-Arduino** in `libraries/`, but there **is** a `BLE` (Bluedroid) wrapper —
  which `platformio.ini` already ignores via `lib_ignore = BLE`.
- `framework-arduinoespressif32-libs/esp32c3/sdkconfig` (the sdkconfig the prebuilt C3
  libs were built with) already contains:
  `CONFIG_BT_ENABLED=y`, `CONFIG_BT_NIMBLE_ENABLED=y`, `CONFIG_BT_NIMBLE_ROLE_CENTRAL=y`,
  `CONFIG_BT_NIMBLE_GATT_CLIENT=y`, `CONFIG_BT_NIMBLE_SECURITY_ENABLE=y`,
  `CONFIG_BT_NIMBLE_NVS_PERSIST=y`, `CONFIG_BT_NIMBLE_MAX_BONDS=3`,
  `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=3`, `CONFIG_BT_NIMBLE_EXT_SCAN=y`,
  `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL=y`, `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=5120`,
  `CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`.
- `framework-arduinoespressif32-libs/esp32c3/lib/` already ships `libbt.a`,
  `libbtdm_app.a`, `libble_mesh.a`, `libesp_coex.a`, `libcoexist.a`.
- CrossInk's `custom_sdkconfig` fragment is merged on top of that file and touches only
  Wi-Fi IRAM placement, timer stacks and Arduino selective compilation — nothing BLE.

Consequence: adding NimBLE-Arduino as a **library dependency only** is enough for
firmware builds; do **not** add `custom_sdkconfig` BT lines (that would force a
multi-minute ESP-IDF library rebuild for no benefit). Extended **scanning** is already
enabled, which is what the fork needed for scan-response based discovery.

## Measured baseline (2026-10-10) — READ THE CAVEAT

Measured with `pio run -e default` in this checkout:

| Metric | Value |
| --- | --- |
| Verdict | `SUCCESS Took 864.56 seconds` |
| Flash | `96.6% (used 6329993 bytes from 6553600 bytes)` |
| Flash free | **209,472 bytes (~204 KB)** — size gate printed `Firmware image fits OTA app partition: 6344128 bytes <= 6553600 bytes (209472 bytes free)` |
| RAM (static) | `19.8% (used 64784 bytes from 327680 bytes)` |
| `firmware.bin` | 6,344,128 bytes |
| i18n cost inside the image | `440,372 B strings (deduped) + 53,200 B offset tables = 493,572 B` over 28 languages |
| `pio run -e simulator` | SUCCESS, exit 0 |
| `scripts/run_simulator_smoke_test.py` | exit 0, `[INF] [SMOKE] Simulator smoke test passed` |

**CAVEAT — this is not a pristine HEAD baseline.** The checkout was being edited
concurrently by a different workstream (a lunar-calendar + weather sleep-screen feature:
`lib/LunarCalendar/`, `lib/Weather/`, `plans/lunar-and-weather-sleep-screens/`, +441 lines
in `SleepActivity.cpp`, 42 new i18n keys per YAML). The generated i18n key count moved
from 874 to 876 *between two builds of the same env*, which is how the contamination was
detected. The true free-flash figure for the BLE work is therefore **lower** than
209,472 B, because that feature is still growing.

Consequence for R1: with ~204 KB free and an estimated 150–350 KB for the NimBLE stack,
the flash gate is the top risk of this plan. Plan to trim
`crossink.custom_i18n_builtin_langs` (the i18n tables are 493 KB of the image) and
re-measure in a clean worktree before writing BLE code.

**Do not run two PlatformIO builds at once in this repo.** A concurrent build that
regenerates `lib/I18n/I18nKeys.h` while another build compiles `main.cpp` produced
`error: multiple definition of 'enum class Language'` in a run of this battery. Build in
an isolated worktree instead.

## Decisions (recommended option marked ✅)

| # | Decision | Options | Recommendation |
| --- | --- | --- | --- |
| D1 | BLE stack | (a) `h2zero/NimBLE-Arduino @ 2.5.0`; (b) Arduino `BLE` (Bluedroid); (c) vendor NimBLE-Arduino into `lib/` | ✅ (a). Same pin the fork validated on the identical platform; Bluedroid costs roughly 400 KB more flash and ~40 KB more RAM, which the C3 cannot afford. Keep `lib_ignore = BLE`. |
| D2 | Envs that get BLE | (a) `default` + `debug` (C3 X3/X4) only; (b) all firmware envs; (c) C3 + S3 | ✅ (a). The user's device is the X3; the S3 envs stay untouched so their flash gates cannot regress. `CROSSINK_APP_CAP_BLE=0` explicitly in every other env, enforced by `#error` in `AppCapabilities.h`. |
| D3 | Where the manager lives | (a) `lib/hal/BluetoothHIDManager.*`; (b) `src/network/`; (c) `freeink-sdk` | ✅ (a). AGENTS.md puts HAL wrappers in `lib/hal`; that library is already ignored on the simulator; no submodule gitlink change (AGENTS.md requires one for SDK edits). |
| D4 | Injection seam | (a) raw virtual buttons in `HalGPIO` **plus** `MappedInputManager::injectRelease` for page turns; (b) raw virtual buttons only; (c) a new page-turn channel in `MappedInputManager` | ✅ (a). Raw buttons make keyboard keys inherit every existing mapping (orientation swap, side-button config, menu navigation) for free; the injected release is the pattern the Chord Page Turn already uses (`src/main.cpp:796-811`, consumed via `hasInjectedRelease`, `src/MappedInputManager.h:92-97`) and it makes a page turner work even when the user remapped or disabled the side buttons. |
| D5 | HID coverage | (a) generic boot-keyboard + consumer control + a compact profile table (Free2/Free3, GameBrick/IINE); (b) full fork profile set + learn wizard; (c) keyboard-only | ✅ (a). (c) cannot drive the cheap page turners the user is buying; (b) is 1,700+ lines of profile/learn UI for a v1. |
| D6 | Repeat handling per key | (a) true press/release edges for keyboard keys, one-shot per press for consumer/profile page-turn keys with a 250 ms re-press debounce; (b) uniform edges | ✅ (a). Many remotes re-send a latched keycode while held; without the one-shot rule one long press would fire a burst of page turns. 250 ms mirrors the fork's `VIRTUAL_BUTTON_REPRESS_DEBOUNCE_MS`. |
| D7 | Wi-Fi interplay | (a) explicit mutual exclusion, driven from one place in `loop()`; (b) rely on ESP SW coexistence | ✅ (a). Coexistence is compiled in, but the C3 is single-core with ~380 KB of internal RAM shared by the 48 KB framebuffer, EPUB scratch, Wi-Fi and BLE; the fork disabled Wi-Fi before enabling BLE on this exact board, and CrossInk's own Wi-Fi flows already `silentRestart()` to clear fragmentation (`src/SilentRestart.h:14-24`). One choke point in `loop()` beats editing 8 Wi-Fi activities. |
| D8 | Persistence | (a) fields on `CrossPointSettings` in `crossink-settings.json`; (b) a new store file; (c) NVS | ✅ (a), matching the fork's `JsonSettingsIO` change and CrossInk's single settings store (`src/CrossPointSettings.cpp:41-43`, `:441-510`, `:918-960`). Bond keys stay in NVS (`CONFIG_BT_NIMBLE_NVS_PERSIST=y`). |
| D9 | Deep sleep | (a) tear BLE down before sleep, reconnect on the next boot when enabled + bonded; (b) keep the radio alive across sleep; (c) no reconnect | ✅ (a). (b) is impossible without light sleep. (a) needs no extra marker: `SETTINGS.bluetoothEnabled` + a stored bond already means "this user wants a remote". |
| D10 | Auto-reconnect | (a) manual Reconnect plus one bounded auto-attempt after a disconnect while awake; (b) the fork's dense/sparse windows + guard file; (c) manual only | ✅ (a). Remotes power off and must come back; (b) is the fork's answer to failure modes we have not reproduced yet. Contingency: if auto-reconnect misbehaves on hardware, ship (c) and keep the manual path. |
| D11 | i18n | (a) new keys in `english.yaml` + `vietnamese.yaml`, other languages fall back; (b) all 28 languages | ✅ (a). `docs/i18n.md:5` and `scripts/gen_i18n.py:15-16,210-224` define exactly this fallback. Regenerate with `scripts/gen_i18n.py`; never hand-edit `I18nKeys.h` / `I18nStrings.*`. |

## Architecture

```text
BLE keyboard / page turner
        │  HID over GATT (input report notification)
        ▼
NimBLE host task (IDF, 5120 B stack, internal RAM)
        │  NimBLERemoteCharacteristic notify callback
        ▼
BluetoothHIDManager  (lib/hal, singleton)
   • enable/disable, scan, pair, bond, subscribe
   • HID report parsing → logical event {kind, key, pressed}
   • Fixed-size POD queue  ────────────┐  (no std::function, no heap per event)
   • worker task (scan/connect) 8 KB ✱ │
   • HalPowerManager::Lock while a worker is running
        │
        ▼  drained from the main loop only
src/main.cpp loop()
   • virtual raw buttons  → HalGPIO virtual button bitmap → mapButton() → every screen
   • page-turn events     → mappedInputManager.injectRelease(Button::Right/Left)
                            + activityManager.loop() + clearInjectedReleases()
```

✱ Stack size is a starting point to be validated with `uxTaskGetStackHighWaterMark`;
the fork used 12288 B for the same job, which is a large slice of a C3's internal RAM.

Key properties by construction:

- **The NimBLE callback never touches app state.** It copies at most
  `MAX_HID_REPORT_BYTES` into a queue slot. Everything else happens in `loop()`.
  This satisfies AGENTS.md's "do not mutate activity stack or renderer state from
  arbitrary tasks".
- **No new input abstraction.** A BLE keyboard key becomes a raw `HalGPIO::BTN_*` press,
  so `MappedInputManager::mapButton` (`src/MappedInputManager.cpp:132-181`) continues to
  own reader orientation swaps, front/side button remapping and label mapping. A page
  turner becomes an injected `Button::Left`/`Button::Right` release, which the EPUB, TXT
  and XTC readers already consume (`src/activities/reader/ReaderUtils.h:216-235`,
  `EpubReaderActivity.cpp:3322`, `TxtReaderActivity.cpp:351`, `XtcReaderActivity.cpp:409,441`).
- **Remotes never reach the long-press machinery.** Because page-turner keys use the injected-release path and not virtual buttons, `SideButtonShortcuts`' 700 ms long-action tracking (`src/activities/reader/SideButtonShortcuts.h:63-70`) never sees them. That removes the fork's `hadRecentBlePageTurnInput()` hack entirely (`%TEMP%/.../src/activities/reader/ReaderUtils.h:40`), which existed to stop a held remote from being read as a chapter-skip hold. A *keyboard* key does reach it, which is correct: holding PageDown should skip a chapter, exactly like the physical side button.
- **BLE input never bypasses Quick Lock.** While `buttonShortcutController.isQuickLocked()` is true the manager drops BLE input (virtual buttons cleared, pending page-turn request discarded), so a paired remote cannot unlock or page through a deliberately locked device.
- **Capability gating everywhere.** `CROSSINK_APP_CAP_BLE` in `include/AppCapabilities.h`
  follows the existing touch/USB-drive pattern (`#error` when undefined), so a BLE-off
  build compiles the manager out entirely and the simulator never sees it.
- **No `std::function` in the hot path.** The manager exposes a small
  `applyTo(HalGPIO&, MappedInputManager&)`-style drain instead of the fork's
  `setButtonInjector(std::function<...>)` callbacks (fork
  `lib/hal/BluetoothHIDManager.h:104-110`), per AGENTS.md's `std::function` rule.

## Red-team findings (self-review, applied to the phase files)

The Advisor review tool was unavailable (billing error) for this plan, so the design was
red-teamed against the actual call sites. Five concrete corrections were found and are
folded into the phases:

1. **Page turns must not be applied before the Quick Lock gate.** Draining BLE events at
the top of `loop()` is right for virtual buttons, but a page-turn request must be consumed
*after* the quick-lock and modal gates, in the same region as the chord page turn
(`src/main.cpp:1779-1811`), otherwise a remote could page through a locked device.
2. **Virtual buttons must not be able to unlock Quick Lock.** The quick-lock branch
accepts side-button presses as an unlock gesture; a BLE keyboard's Up/Down would satisfy
it. Rule added: while quick-locked, BLE input is suppressed.
3. **Bond accumulation.** `CONFIG_BT_NIMBLE_MAX_BONDS=3` is a device limit, not a policy;
pairing a second remote would leave the old bond in NVS while settings remember only one.
Rule added: Forget deletes all bonds, and a successful pair of a different device deletes
the previous bond.
4. **One-shot vs true edges.** An earlier draft called the consumer/profile page-turn path
a "pulse"; since page turns travel as injected releases, a pulse is unnecessary and would
only add latency. The rule is now "one action per press, suppressed until release".
5. **Capability definition per env.** `[simulator-base]` is a shared base section, so the
simulator side needs the `=0` define once there, not in four envs.

## Risks and mitigations

| # | Risk | Evidence | Mitigation |
| --- | --- | --- | --- |
| R1 | **Flash may not fit.** No recorded size for a CrossInk `default` build exists locally (`.pio/build` has only `simulator`), and CrossInk carries 28 languages (`lib/I18n/I18nStrings.cpp` is 2.1 MB), the web portal, wolfSSL/TLS and USB MSC that the fork does not. | Fork BLE build = `0x5b7630` vs `0x640000` limit → only ~8.5 % headroom on a *smaller* feature set. | Phase 1 starts with a baseline `pio run -e default` size measurement *before* the dependency is added. Hard gate: `scripts/check_firmware_size.py` must pass. If it fails: trim `crossink.custom_i18n_builtin_langs` (`platformio.ini:12-15`, already a supported mechanism per `CHANGELOG.md:138`), then re-measure. Reported in Phase 1. |
| R2 | **RAM on the C3 — the most likely real-world failure.** BLE-enabled cost is roughly the NimBLE host task stack (5120 B) + MSYS pools (12×256 + 24×320 = 10,752 B) + controller + bookkeeping ≈ 35–55 KB of internal heap, taken from the same pool that serves EPUB layout and covers. CrossInk's own history is already about memory pressure (`CHANGELOG.md:340-342`). | `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL=y`, `CONFIG_BT_NIMBLE_MSYS_1/2_*` in the C3 sdkconfig; the fork's last fix was precisely "do not lower CPU clock while BLE work runs". | BLE is **off by default**. Measure `ESP.getFreeHeap()` / `getMaxAllocHeap()` at boot, after enable, after connect, and with a large EPUB open (`logMemoryStats`, `src/main.cpp:326`). If the stress case fails, the honest v1 answer is: document "Bluetooth reduces free memory; switch it off for very large or image-heavy books", and treat "auto-suspend BLE during EPUB indexing" as a follow-up rather than shipping a fragile hack. |
| R3 | **Auto-sleep / battery regression.** BLE events reset the inactivity timer; a reconnect worker defers sleep. A bug could either keep the device awake forever or sleep mid-scan. | Fork's `idlefix15` exists because background reconnect kept the X3 awake (`docs/x3-ble-page-turner.md`, *Why Idlefix15 Exists*). | Deferral is allowed only while a worker actually runs, with a hard cap on worker lifetime; every deferral is logged; the unchanged path keeps `src/main.cpp:1804-1810`. Hardware gate: with BT on and no remote nearby, the device must still auto-sleep. |
| R4 | **Blocking work in the UI loop** would stall the e-ink refresh and input. | Fork moved scan/connect/subscribe into `bt_pair`/`bt_reconn` tasks with a `HalPowerManager::Lock` (`lib/hal/BluetoothHIDManager.cpp:1510,1600`). | All blocking BLE calls live in the worker task; `loop()` only drains a queue and never scans/connects. |
| R5 | **Cross-task input corruption.** | AGENTS.md: do not mutate activity stack or renderer state from arbitrary tasks. | Notify callback only enqueues a POD; virtual-button writes and `injectRelease` happen in `loop()`. |
| R6 | **Orphaned bonds / pairing loops.** | Fork's `pairfix1` exists because first pairing was unstable. | Forget clears the NVS bond *and* the settings fields, saving once (debounced); "Pair New Remote" is bounded in time and saves only after service discovery + subscribe succeed — the fork's own rule (`docs/x3-ble-page-turner.md`, *Pair New Remote Behavior*). |
| R7 | **Simulator / BLE-off builds regress.** | The repo already guards this class of bug with a post-build symbol gate for touch. | New `scripts/check_app_ble_gate.py` modelled on `scripts/check_app_touch_gate.py`: reject BLE symbols in ELFs built with `CROSSINK_APP_CAP_BLE=0`. |
| R8 | **Not all remotes will work in v1.** | The fork needed per-device profiles and a learn path, and still documents third-party remotes as unvalidated. | Ship the generic keyboard/consumer paths plus the compact profile table, and a documented serial report dump so an unsupported remote can be captured and added as a profile later. Say this plainly in `docs/bluetooth.md` rather than implying universal support. |

## Phases

| Phase | Name | Status | Delivers |
| --- | --- | --- | --- |
| 1 | [Stack Bringup](./phase-01-stack-bringup.md) | Pending | Capability flag, NimBLE dependency, manager skeleton, `HalGPIO` virtual buttons, `main.cpp` lifecycle, baseline + after flash/RAM numbers |
| 2 | [Hid Input Mapping](./phase-02-hid-input-mapping.md) | Pending | Report parsing, profile table, keyboard → raw buttons, page turner → injected release, dedup/debounce |
| 3 | [Settings And Persistence](./phase-03-settings-and-persistence.md) | Pending | `BluetoothSettingsActivity`, System → Bluetooth row, settings JSON fields, i18n keys |
| 4 | [Power Wifi And Reconnect](./phase-04-power-wifi-and-reconnect.md) | Pending | Deep-sleep teardown, reconnect on boot, bounded auto-reconnect, Wi-Fi mutual exclusion, auto-sleep deferral |
| 5 | [Docs Gates And Verification](./phase-05-docs-gates-and-verification.md) | Pending | `docs/bluetooth.md`, `CHANGELOG.md`, BLE symbol gate, `default` + simulator builds, `pio check`, X3 hardware checklist |

## Follow-ups deliberately deferred (recorded so they are not lost)

- On-device **learn wizard** and debug monitor for remotes that match no profile.
- **Reader-menu** Bluetooth entry (reconnect without leaving the book) and the fork's
  `exitOnSuccessfulConnect` flow.
- **Crash-loop guard** file for auto-reconnect, if hardware shows reconnect loops.
- **Passkey** pairing support (v1 uses Just Works bonding; a remote that demands a
  passkey will not pair).
- **Auto-suspend BLE during EPUB indexing/layout** if the memory stress test fails.
- Enabling BLE for the **S3 envs** (`sticky`, `x4-pro`, `x4-classic`) once it is proven
  on the C3.

## Dependencies

No cross-plan dependencies. The only other plan in `plans/` (Minuta themes,
`plans/plan.md`) is `completed` and shares no files with this work beyond
`SettingsActivity`/`SettingsList` conventions.

## Conventions to follow while implementing

- Build in WSL, streamed: `wsl -d Ubuntu -e bash -lc 'export PATH="$HOME/.platformio/penv/bin:$PATH"; cd "/mnt/d/New folder/CrosSen" && pio run -e default 2>&1 | tee /tmp/pio-build.log; exit ${PIPESTATUS[0]}'`.
- `LOG_INF`/`LOG_DBG`/`LOG_ERR` with a `"BT"` tag; user-facing text only via `tr(STR_*)`.
- Fallible allocations via `makeUniqueNoThrow` (`lib/Memory/Memory.h`); no bare `new`.
- Files opened with `FsFile`, always closed; no exceptions, no `abort()`.
- Regenerate i18n with `scripts/gen_i18n.py`; never edit generated headers.
- `clang-format` the touched C++ files; keep changes scoped and revert-safe.
