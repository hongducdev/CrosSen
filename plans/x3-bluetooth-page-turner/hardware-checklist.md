# X3 Bluetooth hardware verification checklist

**Status: NOT YET RUNNABLE.** The Bluetooth implementation this checklist verifies does
not exist in the tree yet (no `BluetoothHIDManager`, no `CROSSINK_APP_CAP_BLE`, no
`NimBLE-Arduino` dependency). This file is the executable form of the plan's Phase 5
checklist, ready for the moment `ck-cook` actually implements
`plans/x3-bluetooth-page-turner/plan.md`.

Run every row and record the result. A row that was not run is **not verified** — do not
mark it as passing.

## 0. Build and flash

```bash
# WSL, from the repo root. `tee` MUST be the last stage of the pipeline:
# a final `| tail` buffers all output and the live-log watchdog kills the command
# after ~12 minutes of silence (this already happened twice in this repo).
wsl -d Ubuntu -e bash -lc 'export PATH="$HOME/.platformio/penv/bin:$PATH"; export PYTHONUNBUFFERED=1; cd "/mnt/d/New folder/CrosSen" && pio run -e default 2>&1 | tee /tmp/pio-build.log'
```

- [ ] Build exits 0 and `scripts/check_firmware_size.py` passes.
- [ ] Record Flash/RAM from the PlatformIO summary and `ls -l .pio/build/default/firmware.bin`.
      Compare against the pre-Bluetooth baseline recorded in
      `plans/x3-bluetooth-page-turner/plan.md` (R1).
- [ ] Flash path: copy `.pio/build/default/firmware.bin` to the SD card as `update.bin`
      and use Settings → System → SD firmware update, **or** flash with esptool.
- [ ] Serial log open at 115200 with the `BT` tag visible.

## 1. Pairing and input

| # | Step | Expected | Result |
| --- | --- | --- | --- |
| 1 | Boot, open Settings → System | A `Bluetooth` row exists | |
| 2 | Open it, turn Bluetooth on | Status `Idle`/`Scanning…`; log shows NimBLE init and the free-heap delta | |
| 3 | `Pair New Remote`, put the remote in pairing mode | Status `Connected: <name>`; log shows connect → HID discovery → subscribe | |
| 4 | Open an EPUB, press forward/back on the remote | Exactly one page turn per press; page counter advances; holding the button does not burst | |
| 5 | Forget the remote, pair a BLE keyboard | Arrows navigate Home/lists; Enter/Space confirm; Esc/Backspace back; PageUp/PageDown turn pages | |
| 6 | Enable Quick Lock, then press BLE keys and a page-turn key | Nothing happens; only a physical gesture unlocks | |
| 7 | Reboot the device | Bluetooth comes back enabled and reconnects to the saved remote | |

## 2. Power, Wi-Fi and recovery

| # | Step | Expected | Result |
| --- | --- | --- | --- |
| 8 | With BLE connected, open File Transfer (Wi-Fi) | BLE is released (one log line); Wi-Fi works; after leaving, BLE re-enables and reconnects | |
| 9 | Let the sleep timeout expire with BLE on, remote nearby | Device sleeps; wake with Power reconnects without opening Settings | |
| 10 | Leave with BLE on and the remote switched off | Device still auto-sleeps at the timeout; log shows no repeated deferrals | |
| 11 | Switch the remote off and on | Reconnects within the attempt budget; attempts never exceed the per-boot cap | |
| 12 | Regression sweep with a remote connected | Physical buttons, tilt page turn, Quick Lock, Power+Down screenshot, bookmark/clipping holds unchanged | |
| 13 | BLE connected + a large EPUB open | `logMemoryStats` shows no OOM, no failed page load, no cover failure | |

## 3. Reset and recovery when it misbehaves

Try in this order:

1. **Settings → Bluetooth → Forget Remote** — clears the NVS bond *and* the settings
   fields; the remote must be paired again.
2. Delete the four `ble*` keys from `/.crosspoint/crossink-settings.json` while the device
   is powered off, if the toggle or the stored device is stuck.
3. The EPUB cache (`/.crosspoint/epub_<hash>/`) is **unrelated** to Bluetooth — no cache
   reset is needed for any BLE problem.
4. If the device will not boot with Bluetooth on: SD-card `update.bin` / esptool flash of
   a known-good build, the same recovery path as any firmware problem.

## 4. What to report back if the remote does not work

- The serial log lines with the `BT` tag containing the raw HID report bytes (the
  manager logs the first ~20 reports of each new device).
- The remote's advertised name (as shown in the pairing status or the log).
- Whether the remote is a keyboard-emulating device, a consumer-control device, or a
  Free2/Free3/GameBrick-style remote — that determines whether a new entry in
  `lib/hal/DeviceProfiles.*` is needed.
