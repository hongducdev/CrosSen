---
title: Sleep Calendar Spacing and Cleanup
description: >-
  Cân đối khoảng cách màn hình ngủ lịch âm và lịch bloc, bỏ nhãn thừa và chuẩn
  hoá định dạng ngày tháng
status: completed
priority: P2
branch: goal/sua-giup-toi-khoang-cach-giua-cu-20261011-1205
tags:
  - ui
  - sleep-screen
  - calendar
  - layout
  - i18n
blockedBy: []
blocks: []
created: '2026-10-11T05:10:15.076Z'
createdBy: ck-cli
source: skill
---

# Sleep Calendar Spacing and Cleanup

## Overview
Kế hoạch tái cấu trúc bố cục và tinh chỉnh giao diện cho hai màn hình ngủ: **Lịch Âm (Lunar Calendar)** và **Lịch Bloc (Tear-off Calendar Block)** trên CrosSen (màn hình e-ink 480x800):
1. **Dàn đều khoảng cách theo chiều dọc**: Khắc phục tình trạng các phần tử bị dồn cục hoặc để lại khoảng trống quá rộng, bất hợp lý (đặc biệt khoảng trống ~150-200px giữa hero day và footer trên lịch bloc, và các khoảng cách nén ép ở phần trên màn hình lịch âm).
2. **Loại bỏ các nhãn thừa không cần thiết**:
   - Màn hình lịch bloc: Bỏ text `"Dương lịch"` dưới số ngày dương lớn; bỏ tiền tố `"Âm lịch: "` trong dòng thông tin âm lịch.
   - Màn hình lịch âm: Bỏ text `"Âm lịch"` (STR_LUNAR_DATE) dưới số ngày âm trong Hero Card; bỏ text `"Lịch Âm"` (STR_LUNAR_CALENDAR_SLEEP) ở footer đáy màn hình.
3. **Chuẩn hoá định dạng ngày tháng âm lịch trên lịch bloc**: Sửa tình trạng hiển thị lệch kiểu `"ngày 2 tháng chín"` (ngày là số `2` nhưng tháng lại là chữ `"chín"`) thành đồng nhất kiểu số: `"Ngày %u · Tháng %u%s"` (ví dụ: `"Ngày 2 · Tháng 9"` hoặc `"Ngày 2 tháng 9"`).

## Target Files
- `src/components/calendar/CalendarBlockScreen.cpp`: Bố cục lịch bloc, bỏ nhãn thừa, chuẩn hoá ngày tháng âm, dàn đều vertical rhythm.
- `src/components/calendar/CalendarAlmanac.h` & `CalendarAlmanac.cpp`: Các helper hiển thị lịch âm bloc.
- `src/activities/boot_sleep/SleepActivity.cpp`: Bố cục màn hình ngủ lịch âm (`renderLunarCalendarSleepScreen`), bỏ text footer "Lịch Âm", bỏ text "Âm lịch" trong card, căn giữa số ngày âm, giãn đều khoảng cách.

## Phases

| Phase | Name | Status |
|-------|------|--------|
| 1 | [audit-and-layout-design](./phase-01-audit-and-layout-design.md) | Completed |
| 2 | [implementation](./phase-02-implementation.md) | Completed |
| 3 | [verification](./phase-03-verification.md) | Completed |

## Dependencies
- Màn hình e-ink 480x800 1-bit, font UI_12, UI_10, SMALL, LexendDeca_16, CalendarDigits bmp (160px).
- PlatformIO build môi trường `default` (ESP32-C3) và `simulator`.
