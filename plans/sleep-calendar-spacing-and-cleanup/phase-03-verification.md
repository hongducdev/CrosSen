---
phase: 3
title: "Verification"
status: pending
priority: P2
effort: "1h"
dependencies: [2]
---

# Phase 3: Verification

## Overview
Xác minh toàn diện quá trình triển khai mã nguồn thông qua biên dịch PlatformIO, kiểm tra trực quan bố cục và rà soát git diff.

## Verification Steps

### 1. Build Verification
- Biên dịch firmware bằng PlatformIO môi trường `default` (ESP32-C3):
  `pio run -e default`
- Đảm bảo firmware build thành công 100%, không phát sinh lỗi hoặc cảnh báo mới.
- Kiểm tra kích thước firmware đảm bảo an toàn bộ nhớ DRAM/Flash.

### 2. Layout & Logic Verification
- Rà soát các phép tính tọa độ trên màn hình 480x800:
  - Màn hình Lịch Bloc: Card inset 18px (Y từ 18 đến 782). Tất cả các phần tử (banner, thứ, hero digit, almanac, holiday, quote, author) đều nằm an toàn bên trong card, không tràn đáy `cardBottom (782)`.
  - Màn hình Lịch Âm: Y từ 0 đến 800. Header, Clock, Hero Card, Calendar Grid Card đều nằm an toàn, cách đều viền và cách đều nhau.
- Xác nhận các chuỗi ký tự hiển thị:
  - Không còn nhãn `"Dương lịch"`.
  - Không còn nhãn `"Âm lịch"`.
  - Không còn nhãn `"Lịch Âm"`.
  - Định dạng ngày tháng âm chuẩn số đồng bộ: ví dụ ngày 2 tháng 9 hiển thị `"Ngày 2 · Tháng 9"` (hoặc `"Ngày 2 tháng 9"`).

### 3. Git Diff & Scope Audit
- Rà soát `git diff` để đảm bảo:
  - Chỉ chỉnh sửa các file liên quan đến mục tiêu.
  - Không chỉnh sửa file generated hoặc các file ngoài phạm vi.
  - Tuân thủ quy tắc lập trình nhúng C++20 của dự án.

## Success Criteria
- [ ] Build PlatformIO `default` thành công 100%.
- [ ] Tất cả yêu cầu người dùng được đáp ứng trọn vẹn và chính xác.
- [ ] Git status và git diff sạch sẽ, chuẩn Conventional Commits.
