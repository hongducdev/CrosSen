---
phase: 1
title: Audit and Layout Design
status: completed
priority: P2
effort: 2h
dependencies: []
---

# Phase 1: Audit and Layout Design

## Overview
Khảo sát toàn diện mã nguồn, tọa độ pixel thực tế và thiết kế lại phân bổ không gian (vertical layout & spacing) cho hai màn hình ngủ: Lịch Bloc (`CalendarBlockScreen.cpp`) và Lịch Âm (`SleepActivity.cpp`).

## Requirements
- **Functional**:
  - Xác định chính xác vị trí và dòng code của các nhãn thừa cần loại bỏ:
    - `"Dương lịch"` ở `CalendarBlockScreen.cpp:310`.
    - Tiền tố `"Âm lịch: "` ở `CalendarBlockScreen.cpp:244, 246`.
    - Nhãn `"Âm lịch"` (`STR_LUNAR_DATE`) ở `SleepActivity.cpp:1500`.
    - Nhãn `"Lịch Âm"` (`STR_LUNAR_CALENDAR_SLEEP`) ở `SleepActivity.cpp:1631`.
  - Thiết kế lại công thức định dạng ngày tháng âm trên lịch bloc:
    - Hiện tại: ngày dùng `%d` (số), tháng dùng `lunarMonthName()` (chữ: "tháng Chín") -> lệch kiểu `Ngày 2 · tháng Chín`.
    - Thiết kế mới: đồng bộ hoàn toàn dạng số: `Ngày %u · Tháng %u%s` (ví dụ: `Ngày 2 · Tháng 9`, nếu nhuận thêm ` (nhuận)`).
  - Phân bổ khoảng cách theo chiều dọc trên màn hình 480x800:
    - Lịch bloc: Thay thế cơ chế neo cứng footer vào đáy card và bỏ mặc khoảng trống rỗng ~150-200px ở giữa bằng thuật toán phân bổ gap cân đối, giãn đều từ banner, thứ, số ngày, âm lịch, hoàng đạo, ngày lễ đến quote.
    - Lịch âm: Giảm độ nén phần trên (header, date, clock, hero card), căn giữa số ngày âm trong card, giãn khoảng cách giữa hai card và bỏ footer text sát mép màn hình.
- **Non-functional**:
  - Không thay đổi logic tính toán âm lịch hay can chi.
  - Không cấp phát heap động mới, không dùng exception.

## Detailed Layout Audit & Solutions

### 1. Màn hình Lịch Bloc (`CalendarBlockScreen.cpp`)
- **Kích thước Card**: 444 x 764px (inset 18px từ viền màn hình 480x800).
- **Vấn đề hiện tại**:
  - Masthead neo ở trên (`y = 42`), Hero day ở `y = 177..337`.
  - Text `"Dương lịch"` ở `y = 345`.
  - Footer neo ở đáy card (`footerTop = cardBottom - 18 - footerHeight`), `footerTop` dao động từ 494 đến 554px tuỳ theo có ngày lễ hay không.
  - Kết quả: Giữa số ngày và footer có một khoảng trống khổng lồ 157px - 217px bị bỏ trống hoàn toàn, trong khi các dòng bên trong footer lại bị nén rất chật (`kLunarLineGap = 7`, `kQuoteLineGap = 2`).
- **Giải pháp thiết kế mới**:
  1. Bỏ text `"Dương lịch"`. Số ngày lớn (160px) được kẹp giữa 2 đường kẻ đôi phân cách một cách trang trọng, khoảng cách trên và dưới số ngày đều đặn (~22-26px).
  2. Bỏ tiền tố `"Âm lịch: "`. Định dạng ngày tháng âm là `Ngày %u · Tháng %u%s`.
  3. Tăng khoảng cách dòng trong khối âm lịch (`kLunarLineGap` từ 7px lên 10px).
  4. Tăng khoảng cách dòng trong khối trích dẫn (`kQuoteLineGap` từ 2px lên 6px).
  5. Tính toán tổng chiều cao nội dung (`contentHeight`) và phân bổ phần khoảng trống còn lại (`slack`) đều vào các khoảng cách chính giữa các dải nội dung (vertical rhythm cân đối). Nhờ đó, cả khi có ngày lễ và không có ngày lễ, trang lịch đều tự động dàn đều cân xứng tuyệt đối, không có vùng trống rỗng dị thường.

### 2. Màn hình Lịch Âm (`SleepActivity.cpp` - `renderLunarCalendarSleepScreen`)
- **Vấn đề hiện tại**:
  - Header, thứ ngày dương, và đồng hồ số 7 đoạn bị ép chật ở dải Y=30..122 (gap chỉ 8px, 14px, 12px).
  - Khung Hero Card ở `Y=134..280` (cao 146px). Cột trái có chữ `"Âm lịch"` ở đáy cột làm số ngày bị đẩy lên cao, không nằm giữa card.
  - Khung lịch tháng ở `Y=294..764` (cao 470px). Mỗi ô cao tới 68px trong khi text chỉ cao ~30px, khiến các ô bị dãn quá mức so với phần trên.
  - Text footer `"Lịch Âm"` ở `Y=780` bị ép sát mép dưới màn hình.
- **Giải pháp thiết kế mới**:
  1. Bỏ text `"Lịch Âm"` ở footer đáy màn hình (`Y=780`).
  2. Bỏ text `"Âm lịch"` ở cột trái Hero Card. Căn giữa số ngày âm lớn theo chiều dọc bên trong cột trái của Hero Card.
  3. Điều chỉnh khoảng cách phần trên cho thông thoáng:
     - Dòng thứ ngày dương: Y=42 (cách đường header line 12px).
     - Đồng hồ số: Y=72..128 (cách ngày dương 14px).
     - Hero Card: Y=142..284 (cao 142px, cách đồng hồ 14px).
     - Calendar Grid Card: Y=302..782 (cách Hero Card 18px, lề dưới cách mép màn hình 18px cân xứng với lề trên/bên).
     - Căn chỉnh vị trí text trong từng ô lịch tháng để ngày dương và ngày âm nằm cân đối giữa ô.

## Success Criteria
- [x] Đã định vị chính xác tất cả vị trí code cần sửa trong `CalendarBlockScreen.cpp` và `SleepActivity.cpp`.
- [x] Đã có công thức tính toán tọa độ chi tiết đảm bảo không tràn màn hình 480x800.
- [x] Đã thống nhất quy tắc định dạng ngày tháng âm "số là số hết" (`Ngày %u · Tháng %u`).
