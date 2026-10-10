---
title: "Lunar Calendar and Weather Sleep Screens"
description: >-
  Add two new sleep/screensaver screens to CrosSen: (1) Vietnamese Lunar Calendar
  (Âm lịch with Can-Chi, Tiết khí, Tết/holidays) and (2) Weather (Open-Meteo with
  cached SD data, WMO condition icons, temp, humidity, wind, and 5-day forecast),
  portable for ESP32-C3 and ESP32-S3 devices.
status: in-progress
priority: P1
branch: "main"
tags:
  - sleep
  - screensaver
  - calendar
  - weather
  - vietnamese
blockedBy: []
blocks: []
created: "2026-10-10T04:00:00.000Z"
createdBy: "ck:plan"
source: skill
---

# Lunar Calendar and Weather Sleep Screens

## Overview

Users want two new screensaver / sleep screen options on their e-reader:
1. **Vietnamese Lunar Calendar (Lịch Âm)**: A beautiful, dedicated e-ink calendar screen displaying the current date, large lunar day & month, Can-Chi for Year/Month/Day (e.g. Năm Bính Ngọ, Tháng Đinh Dậu, Ngày Giáp Thân), solar term (Tiết khí), lunar/solar holidays (Tết Nguyên Đán, Rằm, Giỗ Tổ...), and a monthly calendar grid. Works 100% offline via pure astronomical algorithms in flash (no WiFi or external APIs needed once clock is set).
2. **Weather (Thời tiết)**: Displays current weather (city name, large temperature in °C or °F, weather condition description, crisp monochrome condition icon, feels-like, humidity, wind speed) and a multi-day forecast row. Uses data cached on the SD card at `/.crosspoint/weather_cache.json` (fetched via Open-Meteo HTTP API to eliminate TLS memory overhead on the ESP32-C3). In sleep mode, never turns on WiFi or blocks on network calls—only renders cached data or a clean offline placeholder.

Reference studied: `trilwu/crosspet` (`src/util/LunarCalendar.{h,cpp}`, `src/activities/tools/WeatherActivity.{h,cpp}`, `src/components/icons/weather.h`).

## Scope

### In scope
- `lib/LunarCalendar/`: Pure C++ library implementing Ho Ngoc Duc's solar-to-lunar conversion algorithm for UTC+7, Can-Chi formulas for Year/Month/Day, 24 solar terms (Tiết khí), and major Vietnamese holiday detection.
- `lib/Weather/`: Data models (`WeatherData`, `DailyForecast`), WMO weather code string/icon mapping, and SD card cache serialization (`/.crosspoint/weather_cache.json`).
- `src/components/icons/`: Weather condition icons (32px / 48px monochrome bitmaps) compatible with `GfxRenderer::drawIcon()`.
- Settings & Persistence: Add `LUNAR_CALENDAR_SLEEP = 12` and `WEATHER_SLEEP = 13` to `CrossPointSettings::SLEEP_SCREEN_MODE`, update migration contracts and `SettingsList.h`.
- Localization: Add `STR_*` keys for lunar calendar and weather to `lib/I18n/translations/` (Vietnamese, English, and other locales).
- Sleep Screen Rendering: `SleepActivity::renderLunarCalendarSleepScreen()` and `SleepActivity::renderWeatherSleepScreen()` in `src/activities/boot_sleep/SleepActivity.cpp`.
- Native unit tests: `test/lunar_calendar/` for leap months, year boundaries, and Can-Chi cycle validation.
- User guide documentation & `CHANGELOG.md` entry.

### Out of scope
- Live network fetching inside `SleepActivity::onEnter()` (prohibited on e-ink deep sleep to avoid power drain and scheduler blocks).
- Big foreign fonts or TTF downloads (uses existing built-in Lexend Deca, Inter, and Small fonts).

## Architecture & Hardware Constraints

- **Platform**: ESP32-C3 (Xteink X3/X4, ~380 KB RAM, no PSRAM, 48 KB 1-bit framebuffer) and ESP32-S3 (Sticky, X4 Pro).
- **Memory discipline**:
  - All lunar tables, can-chi names, and weather icon bitmaps marked `static const` / `constexpr` in flash (DRAM cost ~0).
  - No heap churn (`std::string` avoided in hot render loops, stack buffers <= 128 bytes).
  - No TLS/wolfSSL overhead: Open-Meteo and IP-API communicate over plain HTTP (`http://api.open-meteo.com`, `http://ip-api.com`).
  - Safe fallbacks when RTC/NTP time is not yet synced or weather cache is absent.

## Phases

| Phase | Name | Status |
| --- | --- | --- |
| 1 | [Lunar Calendar Engine & Tests](./phase-01-lunar-calendar-engine.md) | Completed |
| 2 | [Weather Model, Cache & Icons](./phase-02-weather-model-and-icons.md) | Completed |
| 3 | [Settings, Enum & i18n Plumbing](./phase-03-settings-and-i18n.md) | Completed |
| 4 | [Sleep Screen UI Rendering](./phase-04-sleep-screens-rendering.md) | Completed |
| 5 | [Verification, Tests & Documentation](./phase-05-verification-and-docs.md) | Completed |

## Files touched (as implemented)

| Area | Files |
| --- | --- |
| Lunar engine | `lib/LunarCalendar/LunarCalendar.{h,cpp}` (new) — Ho Ngoc Duc solar→lunar, leap months, Can-Chi Y/M/D, 24 tiết khí, Vietnamese holidays, timezone-free Gregorian helpers (`gregorianDaysInMonth`, `gregorianWeekday`, `shiftCivilMinutes`) |
| Weather data | `lib/Weather/WeatherTypes.h`, `lib/Weather/WeatherCache.{h,cpp}` (new) — model structs, `/.crosspoint/weather_cache.json` load/save, °C/°F helpers, WMO→text |
| Weather icons | `src/components/icons/weather.h` (new) — 8 hand-drawn 32x32 1-bit bitmaps + `getWeatherIcon()`, drawn through `GfxRenderer::drawIcon`/`fillRect` |
| Weather fetch | `src/activities/settings/WeatherSyncActivity.{h,cpp}` (new) — WiFi selection reuse, IP geolocation + Hà Nội fallback, Open-Meteo over plain HTTP, cache write |
| Sleep screens | `src/activities/boot_sleep/SleepActivity.{h,cpp}` — `renderLunarCalendarSleepScreen()`, `renderWeatherSleepScreen()`, dispatch + `rendersBeforeExit()` cases, `readLocalDateTime()` |
| Settings | `src/CrossPointSettings.{h,cpp}` (`LUNAR_CALENDAR_SLEEP=12`, `WEATHER_SLEEP=13`, `temperatureUnit`, storage order, migration contract), `src/SettingsList.{h,cpp}` (labels, raw values, Temperature Unit), `src/activities/settings/SettingsActivity.{h,cpp}` (`SettingAction::WeatherSync`) |
| i18n | `lib/I18n/translations/{english,vietnamese}.yaml` + regenerated `I18nKeys.h`/`I18nStrings.{h,cpp}` via `scripts/gen_i18n.py` |
| Tests | `test/lunar_calendar/`, `test/weather_cache/` (new) + `test/CMakeLists.txt` |
| Docs | `docs/user-guide.md` (sleep screen table), `CHANGELOG.md` |

### Deviations from the phase plan

- Weather icons were **not** generated through `scripts/generate_icons.py`. That generator emits
  `freeink::Icon` assets for FreeInkUI widget trees, while these icons are painted straight into the
  framebuffer by the sleep screen with `drawIcon`/`fillRect`; crosspet's hand-drawn 32x32 bitmaps fit
  that path directly and cost 1 KB total. No `weatherIcons.manifest` was added.
- A weather **fetch** activity was added (not a separate phase in this plan). Without it the cache
  would never be written and the Weather screen would only ever show its empty state.
- No web-portal setting change was needed: `web/pages/settings.js` only special-cases Quick Resume,
  and the two new enum options are always available.

## Verification (measured, not estimated)

| Metric | Value |
| --- | --- |
| `pio run -e default` | `SUCCESS Took 709.11 seconds`, **0** `error:` lines |
| Flash | `97.5% (used 6390593 bytes from 6553600 bytes)` |
| Flash free | **148,880 bytes (~145 KB)** — size gate: `Firmware image fits OTA app partition: 6404720 bytes <= 6553600 bytes (148880 bytes free)` |
| RAM (static) | `19.8% (used 64928 bytes from 327680 bytes)` |
| `pio run -e simulator` | `SUCCESS Took 363.29 seconds` |
| Simulator smoke test | `Simulator smoke test passed` (exit 0, `--no-build`) |
| `ctest -R lunar_calendar` | Passed — Tết 2024/2025/2026, leap months (6/2025, 2/2023), solar terms, Vietnamese holidays, UTC+7 rollover, leap-day and year boundaries |
| `ctest -R weather_cache` | Passed — °C/°F, WMO mapping, forecast structs, save/load round-trip, and missing / empty / corrupt cache files |
| Full native suite | `cmake --build … -j 8` → 0 errors; `ctest` → **918/918 passed, 0 failed** (12.07 s) — no regression in the other 89 test targets |

### On-screen evidence (simulator, headless BMP capture)

All four captures live in `evidence/` and were taken with `CROSSPOINT_SIM_SCREENSHOTS` against
`.pio/build/simulator/program` with the sleep-screen setting seeded in `crossink-settings.json`:

| Capture | What it proves |
| --- | --- |
| [`evidence/lunar-calendar-sleep.png`](./evidence/lunar-calendar-sleep.png) | 528×792 render: header + battery, non-overlapping date line, 7-segment clock, lunar hero card (Tháng 9 - Năm Bính Ngọ, Ngày Bính Tý, Tháng Mậu Tuất, Tiết khí: Hàn Lộ, ★ Mùng Một), month grid with today filled |
| [`evidence/weather-sleep-cached.png`](./evidence/weather-sleep-cached.png) | Weather with a seeded cache: city, condition, 64 px icon, 28°C, feels-like, humidity/wind card, 5-day forecast, `Cập nhật: 08:30` |
| [`evidence/weather-sleep-no-cache.png`](./evidence/weather-sleep-no-cache.png) | Empty-cache fallback: cloud icon, `Chưa có dữ liệu thời tiết`, hint, and clock/date — no crash, no WiFi |
| [`evidence/control-dark-sleep.png`](./evidence/control-dark-sleep.png) | Control for the untouched default screen: logo + `CrossInk` + `ĐANG NGỦ` still renders identically (no regression) |

### Defects found by this verification pass (and fixed)

1. **Date line overlapped the clock digits.** `drawCenteredText(…, 36, …)` sat inside the 7-segment
   block that started at `y = 56`. Fixed by moving the header rule to 30, the date to 38 and the clock
   to 66 (and shrinking the digit height to 56).
2. **Every weather icon except rain/thunder drew a solid black bar underneath.** The bitmaps were
   declared `uint8_t[128]` but only 112–124 bytes were initialised, so the compiler zero-filled the
   tail — and bit `0` is a *black* pixel in this format. Fixed by explicitly padding all eight icons
   to 128 bytes with `0xFF` (verified: every icon is now exactly 128 bytes).
3. **The lunar day number was drawn with `drawCenteredText`, i.e. centred on the page** instead of in
   its column, so the `1` landed on the card divider. Fixed by measuring the text and centring it on
   the left column (`leftColCenterX`).
4. **Lunar day Can-Chi was wrong because the RTC is UTC** (found while writing the sleep screen, then
   covered by a regression test): the screen fed RTC UTC fields straight in as local time. Fixed with
   `shiftCivilMinutes()` and covered by the UTC+7 rollover cases in `test/lunar_calendar`.
5. **Lunar engine mis-assigned the Vietnamese 2023 leap month (severity: correctness bug on the lunar
   sleep screen, not a failing test).** The Ho Ngoc Duc abridged 13-term new-moon series placed
   2023's leap month on tháng Giêng (month 1, starting 20/02/2023) instead of the correct tháng Hai
   (month 2, running 22/03–19/04/2023). The true new moon on 2023-03-21 17:24 UTC falls at
   22/03 00:24 ICT — only 24 minutes past local midnight, a knife-edge case that the old formula
   mishandled. The unit test at `test/lunar_calendar/LunarCalendarTest.cpp:169-176` had locked in
   the engine's wrong output (`20/02/2023 → 1/1 nhuận`, `22/03/2023 → 2/2`), which is why `ctest`
   reported PASS instead of FAIL. Fixed by replacing `newMoon()` with the full Jean Meeus Table 49.A
   (25 periodic terms + E eccentricity + Omega), and updating the test to assert the correct
   ground-truth values verified against the Vietnamese lịch vạn niên.

### Baseline cost of the feature

The two new sleep screens plus the lunar library, the weather cache, the 8 weather icons and 29 new
i18n keys cost **~21 KB of the 6.5 MB app partition** (6,369,469 B before the screens →
6,390,593 B after), with no new framebuffer and no growth in static RAM.

### Not verified on this machine

- **Real X3 hardware.** No device is attached to this session, so nothing was flashed or measured on
  the target. Follow [`hardware-checklist.md`](./hardware-checklist.md) — it lists the exact steps and
  the expected on-screen result for both screens, including the 17:00–24:00 local-time case and all
  eight weather icons.
- **Real WiFi fetch.** `WeatherSyncActivity` was compile-verified only; no live request to
  `ip-api.com` / `api.open-meteo.com` was made from a device. The cache path itself is covered by the
  round-trip unit test.

## Deferred / follow-up work

- **Location choice.** Weather sync always uses IP geolocation with a Hà Nội fallback. A city picker
  (like crosspet's 63-province table) is a follow-up if the detected city is wrong on a hotspot/carrier NAT.
- **Auto-refresh.** Weather is only fetched from `Settings → System → Device → Sync Weather`.
  An opportunistic refresh on an already-connected Wi-Fi session (no extra radio power) is a follow-up.
- **Lunar day Can-Chi for the grid.** The month grid shows lunar day numbers only; per-cell Can-Chi
  would need a wider cell than 528 px allows at the current font sizes.
- **Hour (giờ) Can-Chi.** Only year/month/day Can-Chi is shown today.

