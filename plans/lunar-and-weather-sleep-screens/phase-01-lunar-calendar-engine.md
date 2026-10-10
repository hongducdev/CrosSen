---
phase: 1
title: "Lunar Calendar Engine & Tests"
status: in-progress
priority: P1
effort: "2h"
dependencies: []
---

# Phase 01: Lunar Calendar Engine & Tests

## Overview
Implement a lightweight, zero-heap Vietnamese Lunar Calendar library under `lib/LunarCalendar/` using Ho Ngoc Duc's astronomical framework with Jean Meeus Table 49.A new-moon series for UTC+7. Provide solar-to-lunar date conversion, leap month handling, Can-Chi for Year/Month/Day, solar terms (Tiết khí), and holiday detection. Add unit tests under `test/lunar_calendar/`.

## Requirements
- Functional:
  - `LunarDate solarToLunar(int day, int month, int year, double timeZone = 7.0)`
  - `CanChi getCanChiYear(int lunarYear)`, `getCanChiMonth(int lunarMonth, int lunarYear)`, `getCanChiDay(int day, int month, int year)`
  - `const char* getSolarTerm(int day, int month, int year, double timeZone = 7.0)`
  - `const char* getVietnameseHoliday(int solarDay, int solarMonth, const LunarDate& lunar)`
- Non-functional:
  - 100% stack/static const; no dynamic allocation or `std::string` heap churn.
  - Correctness verified by gtest / CMake test for historical and upcoming dates (2024–2030, leap months, Tết boundaries).

## Related Code Files
- Create: `lib/LunarCalendar/LunarCalendar.h`
- Create: `lib/LunarCalendar/LunarCalendar.cpp`
- Create: `lib/LunarCalendar/library.json`
- Create: `test/lunar_calendar/LunarCalendarTest.cpp`
- Create: `test/lunar_calendar/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

## Success Criteria
- [x] `lib/LunarCalendar` compiles with zero warnings under C++20.
- [x] Native tests in `test/lunar_calendar` pass all leap-month and Tết test cases.
