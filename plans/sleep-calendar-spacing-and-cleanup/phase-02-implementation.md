---
phase: 2
title: Implementation
status: completed
priority: P2
effort: 3h
dependencies:
  - 1
---

# Phase 2: Implementation

## Overview
Tiến hành chỉnh sửa mã nguồn cho hai file chính:
- `src/components/calendar/CalendarBlockScreen.cpp` (và helper liên quan nếu cần).
- `src/activities/boot_sleep/SleepActivity.cpp`.

## Requirements
- Loại bỏ các text label không cần thiết (`"Dương lịch"`, tiền tố `"Âm lịch: "`, `"Âm lịch"`, `"Lịch Âm"`).
- Định dạng ngày tháng âm lịch dạng số đồng nhất (`Ngày %u · Tháng %u%s`).
- Triển khai thuật toán dàn đều khoảng cách theo chiều dọc trên màn hình e-ink 480x800.

## Related Code Files
- Modify: `src/components/calendar/CalendarBlockScreen.cpp`
- Modify: `src/activities/boot_sleep/SleepActivity.cpp`
- (Optional) Modify: `src/components/calendar/CalendarAlmanac.h` & `CalendarAlmanac.cpp` (nếu cần bổ sung format helper)

## Implementation Steps

### Bước 1: Sửa đổi `CalendarBlockScreen.cpp`
1. **Xoá text `"Dương lịch"`**:
   - Xoá lệnh `drawCentered(renderer, SMALL_FONT_ID, centerX, y, "Dương lịch", true);` tại dòng ~310.
2. **Chuẩn hoá định dạng ngày tháng âm**:
   - Thay đổi logic định dạng tiêu đề âm lịch:
     Bỏ tiền tố `"Âm lịch: "`.
     Đồng bộ ngày và tháng đều dùng chữ số:
     ```cpp
     char lunarTitle[128];
     const char* leapSuffix = lunar.isLeapMonth ? " (nhuận)" : "";
     std::snprintf(lunarTitle, sizeof(lunarTitle), "Ngày %u · Tháng %u%s",
                   static_cast<unsigned>(lunar.day), static_cast<unsigned>(lunar.month), leapSuffix);
     ```
3. **Tính toán và dàn đều khoảng cách theo chiều dọc**:
   - Đo chiều cao các khối:
     - `h_banner = kBannerHeight (46)`
     - `h_weekday = titleLineHeight (18)`
     - `h_hero = CalendarDigitHeight (160)`
     - `h_double_rules = 2 * (kRuleGap + 1) = 16`
     - `h_almanac = titleLineHeight + 2 * bodyLineHeight + hoursLines.size() * bodyLineHeight`
     - `h_holiday = (holiday != nullptr) ? (kHolidayBandHeight + gap) : 0`
     - `h_quote = 1 + quoteLines.size() * quoteLineHeight + authorHeight`
   - Tính tổng chiều cao cố định và khoảng trống dư thừa trong card 764px.
   - Thay vì dồn một khoảng trống 160-200px ở giữa số ngày và footer, phân bổ khoảng cách đều đặn vào:
     - Khoảng cách giữa các band chính (`kBandGap` tính toán hoặc phân bổ hợp lý ~22-26px).
     - Khoảng cách kẹp đều trên và dưới số ngày lớn (hero day) với 2 đường kẻ đôi.
     - Tăng khoảng cách dòng trong almanac (`kLunarLineGap = 10`) và quote (`kQuoteLineGap = 6`).

### Bước 2: Sửa đổi `SleepActivity.cpp`
1. **Xoá text footer `"Lịch Âm"`**:
   - Xoá `renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 20, tr(STR_LUNAR_CALENDAR_SLEEP));` tại dòng ~1631.
2. **Xoá text `"Âm lịch"` trong Hero Card & căn giữa số ngày âm**:
   - Xoá `renderer.drawText(SMALL_FONT_ID, leftColCenterX - lblW / 2, heroCardY + 98, tr(STR_LUNAR_DATE));` tại dòng ~1500.
   - Căn giữa số ngày âm lớn theo chiều dọc bên trong cột trái của Hero Card:
     `const int numY = heroCardY + (heroCardH - 44) / 2;`
     `renderer.drawText(LEXENDDECA_16_FONT_ID, leftColCenterX - numW / 2, numY, lunarDayStr, true, EpdFontFamily::BOLD);`
3. **Cân đối khoảng cách màn hình**:
   - Điều chỉnh tọa độ Y cho solar date (Y=42), clock (Y=72), hero card (Y=142), calendar grid card (Y=302).
   - Tinh chỉnh vị trí ngày dương và ngày âm trong các ô lịch tháng để căn giữa ô đẹp mắt.

## Success Criteria
- [ ] Code biên dịch sạch sẽ, không có warning hoặc lỗi.
- [ ] Không còn xuất hiện chữ `"Dương lịch"`, `"Âm lịch"`, `"Lịch Âm"`.
- [ ] Dòng ngày tháng âm hiển thị đồng nhất dạng số: `"Ngày 2 · Tháng 9"`.
- [ ] Màn hình không còn khoảng hở rỗng quá lớn, các khối dàn đều hài hoà.
