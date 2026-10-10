# Hardware verification — Lunar Calendar & Weather sleep screens

Target: **Xteink X3** (ESP32-C3, 528×792, DS3231 RTC at I2C 0x68).
Firmware: `pio run -e default` → `.pio/build/default/firmware-x3-x4.bin` (or `update.bin` on the SD card).

## 0. Before you start

- [ ] Battery above ~30% (the weather sync turns the radio on).
- [ ] SD card inserted (the weather cache and settings live on it).
- [ ] A WiFi network saved in **Settings → System → WiFi Networks** (needed once for the weather sync).
- [ ] Know your current calendar date, and check the screen against it.

## 1. Flash and boot

- [ ] Build: `pio run -e default` (in WSL) — expect `SUCCESS` with `Flash: 97.5%`, ~145 KB free.
- [ ] Copy `firmware-x3-x4.bin` → SD card `update.bin` (or use **Settings → System → SD Firmware Update**).
- [ ] After the update the device boots to Home with no crash and no `SLP` errors on serial.

## 2. Lunar Calendar sleep screen

- [ ] **Settings → Display → Sleep Screen → Lịch Âm** (the "Sleep Screen" row also offers "Thời tiết").
- [ ] Press Power to sleep (or wait for the timeout).
- [ ] Expected on screen, top to bottom:
  - [ ] `CrossInk` at top-left, battery percentage at top-right.
  - [ ] One line: weekday + `dd/mm/yyyy`, e.g. `Thứ Bảy, 10/10/2026`. **Must not overlap the clock digits.**
  - [ ] A large 7-segment `HH:MM` that matches wall-clock time (UTC+7 applied).
  - [ ] A rounded card: big lunar day number on the left (e.g. `1`), `Âm lịch` under it; on the right
        `Tháng 9 - Năm Bính Ngọ`, `Ngày Bính Tý`, `Tháng Mậu Tuất`, `Tiết khí: Hàn Lộ`, and a `★`
        holiday line when the day is one (`Mùng Một`, `Ngày Rằm`, `Tết…`).
  - [ ] A month grid: `THÁNG 10 / 2026`, weekday header `CN T2 T3 T4 T5 T6 T7`, each cell showing the
        solar day with the lunar day underneath, and **today's cell filled black**.
  - [ ] Footer `Lịch Âm`.
- [ ] Cross-check the lunar day/month against a reference (e.g. the Vietnamese calendar): Tết 2026 is
      Feb 17 → that day must read `Tháng 1 - Năm Bính Ngọ`, `1`.
- [ ] **Clock not synced case:** clear the RTC date (or use a device without an RTC) → the screen must
      show the title plus the sync hint instead of a wrong calendar, and must not hang.
- [ ] **Evening / timezone case (the one that used to be wrong):** check the screen after 17:00 local
      time. The lunar date must already show the *next* lunar day, not yesterday's.

## 3. Weather sleep screen

- [ ] First, populate the cache: **Settings → System → Device → Đồng bộ thời tiết**
      (`Sync Weather`). It connects to WiFi, shows `Đang đồng bộ thời tiết...`, then a success line
      with the detected city and temperature.
- [ ] **Settings → Display → Sleep Screen → Thời tiết**, then sleep the device.
- [ ] Expected on screen: `Thời tiết` top-left and battery top-right, then the city name, the condition
      text (`Có mây`, `Mưa`, …), a large weather icon, the temperature in the configured unit,
      `Cảm giác …`, a two-column card with **Độ ẩm %** and **Gió km/h**, a five-day forecast row
      (day label + icon + high/low), and `Cập nhật: HH:MM` at the bottom.
- [ ] All eight weather icons must render as clean shapes — **no solid black bar** under an icon.
      Check at least: clear (`0`), partly cloudy (`2`), rain (`61`), snow (`73`), thunder (`95`).
- [ ] **No cache / offline case:** rename `/.crosspoint/weather_cache.json` on the SD card, sleep, and
      expect the cloud icon + `Chưa có dữ liệu thời tiết` + the hint, and the current time/date at the
      bottom. No WiFi must be turned on by the sleep screen itself.
- [ ] **Stale cache case:** keep the file but leave the device asleep overnight; the screen must still
      render (using the older timestamp) rather than showing an error.
- [ ] Swap **Settings → Display → Temperature Unit** to `Độ F` and confirm the values convert.

## 4. Power / stability checks

- [ ] Sleep from the reader (Power while reading) — the sleep screen must still render, and the
      `Entering sleep` popup must not be left on screen.
- [ ] Wake the device 5 times in a row → no crash, no `Guru Meditation`, free heap after wake in the
      usual range (`ESP.getFreeHeap()` / `ESP.getMaxAllocHeap()` on serial).
- [ ] Weather screen + no SD card → must fall back cleanly (no cache ⇒ empty state).
- [ ] Confirm WiFi is **off** while sleeping (serial log or battery drain over an hour).

## 5. Regressions to watch

- [ ] Other sleep screens still correct: Dark/Light logo, Cover, Minimal, Dashboard, Quick Resume.
- [ ] The Sleep Screen row in **Settings → Display** lists `Lịch Âm` and `Thời tiết` in the expected
      position, and the web portal (`Settings` page) shows the same options.
- [ ] Existing saved settings survive the update: your previous sleep screen is still selected
      (the new modes are appended, so old values keep their meaning).
- [ ] `Temperature Unit` does not disturb the reader status bar or the clock settings.

## 6. Cache reset (only if a screen looks stale)

The weather screen reads `/.crosspoint/weather_cache.json`. Delete that one file (Settings →
System → Files & Cache → Clear Cache does **not** touch it) and re-run **Sync Weather**.
EPUB caches are unrelated and do not need clearing for these screens.
