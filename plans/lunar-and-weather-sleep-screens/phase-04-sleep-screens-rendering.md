---
phase: 4
title: "Sleep Screen UI Rendering"
status: pending
priority: P1
effort: "3h"
dependencies: ["1", "2", "3"]
---

# Phase 04: Sleep Screen UI Rendering

## Overview
Implement the two new rendering methods in `SleepActivity`:
1. `SleepActivity::renderLunarCalendarSleepScreen()`
2. `SleepActivity::renderWeatherSleepScreen()`
Wire them into `SleepActivity::onEnter()` switch and ensure graceful fallbacks when offline or un-synced.

## Requirements
- Functional:
  - `renderLunarCalendarSleepScreen()`:
    - Digital clock and date in current locale.
    - Large Lunar day display, Lunar month (with leap month notation "Nhuận"), and Lunar year with Can-Chi (e.g. "Bính Ngọ").
    - Can-Chi for current Day and Month (e.g. "Ngày Giáp Thân, Tháng Đinh Dậu").
    - Solar term (Tiết khí, e.g. "Hàn Lộ") and holiday banner (e.g. "Tết Nguyên Đán", "Rằm tháng Tám").
    - Monthly calendar grid with solar numbers and miniature lunar day annotations.
    - Battery indicator.
    - Un-synced placeholder if RTC/time is invalid.
  - `renderWeatherSleepScreen()`:
    - City/Location name & last updated timestamp.
    - Large temperature (respecting °C/°F setting), feels-like, and weather description.
    - 64x64 or 48x48 weather condition icon.
    - Details card: humidity (%) and wind speed (km/h).
    - Multi-day forecast row (3–5 days) with weekday labels, mini icons, high/low temperatures.
    - Un-cached / offline friendly placeholder if no weather data has been loaded yet.
  - Dispatch in `SleepActivity::onEnter()` for `LUNAR_CALENDAR_SLEEP` and `WEATHER_SLEEP`.
  - Update `SleepActivity::rendersBeforeExit()` appropriately.
- Non-functional:
  - 1-bit monochrome rendering with proper contrast and border radii.
  - Orientation-safe rendering (uses `renderer.getScreenWidth()` and `renderer.getScreenHeight()`).
  - No heap churn; static stack buffers.

## Related Code Files
- Modify: `src/activities/boot_sleep/SleepActivity.h`
- Modify: `src/activities/boot_sleep/SleepActivity.cpp`

## Success Criteria
- [ ] Both screens render correctly on simulator at 528x792 (X3) and 480x800 (X4).
- [ ] No ghosting or clipped layout elements.
