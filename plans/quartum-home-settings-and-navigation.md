# Kế hoạch: Tối ưu điều hướng và tích hợp Cài đặt cho giao diện 4-cover (Quartum Theme)

## 1. Tổng quan & Đặt vấn đề
Giao diện "4-cover" (`QuartumTheme`) hiển thị 4 bìa sách gần đây dưới dạng lưới 2x2 cho các thiết bị máy đọc sách (như Xteink X3/X4). Hiện tại, giao diện này đang gặp 2 vấn đề trải nghiệm người dùng:
1. **Thiếu mục Cài đặt (Settings):** Trên màn hình chính 4-cover không có bất kỳ nút hay mục menu nào để mở Cài đặt firmware. Menu pop-up mở qua nút Menu (`BTN_BACK`) chỉ có Thư viện/Duyệt tệp, OPDS, Dấu trang, Thống kê, Truyền tệp mà thiếu hoàn toàn mục Cài đặt (`STR_SETTINGS_TITLE`).
2. **Lỗi tự động mở sách khi bấm nút điều hướng Left/Right:** Khi người dùng bấm nút Right (`BTN_RIGHT`) để di chuyển con trỏ highlight giữa 4 bìa sách, ngay khi nhả nút Right ra, hệ thống tự động gọi hàm mở sách (`openSelectedBook()`), làm cho người dùng không thể duyệt tự do giữa 4 bìa mà bị ép mở sách ngay lập tức.

---

## 2. Phân tích nguyên nhân kỹ thuật (Root Cause Analysis)

Tất cả logic điều khiển giao diện Minuta (gồm Solum 1-bìa và Quartum 4-bìa) nằm trong file `src/activities/home/HomeActivity.cpp`:

### 2.1. Về việc thiếu mục Cài đặt
- **Khởi tạo danh sách menu (`buildMinimalMenuItems`, dòng 263):** Danh sách các mục cho menu pop-up của Minuta được xây dựng thông qua `buildMinimalMenuItems()`, chỉ thêm các mục: Duyệt tệp / Thư viện, OPDS, Dấu trang / Trích dẫn, Thống kê đọc và Truyền tệp. Mục `Settings` (`HomeMenuAction::Settings`) bị bỏ sót hoàn toàn.
- **Xử lý kích hoạt menu (`activateMinutaMenuAction`, dòng 1740):** 
  ```cpp
  case HomeMenuAction::ContinueReading:
  case HomeMenuAction::Settings:
    break;
  ```
  Ngay cả khi người dùng kích hoạt `Settings`, hàm xử lý cũng đang để trống (`break;`) thay vì gọi `onSettingsOpen()`.

### 2.2. Về lỗi tự động mở sách khi bấm nút Right
- **Kế thừa code từ theme 1-bìa (Solum, dòng 1847-1850):**
  ```cpp
  if (releasedFrontButton == HalGPIO::BTN_RIGHT || releasedFrontButton == HalGPIO::BTN_CONFIRM ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSelectedBook();
    return;
  }
  ```
  - Trong theme Solum (chỉ có 1 bìa sách), nút `BTN_LEFT` được dùng để đổi sách hiển thị (`showNextRecentBookOnHome()`), còn nút `BTN_RIGHT` được gán nhãn là nút "Đọc" (`readLabel`), nên khi nhả nút Right (`releasedFrontButton == HalGPIO::BTN_RIGHT`), sách sẽ được mở.
  - Tuy nhiên, trong theme Quartum (4 bìa sách theo lưới 2x2), 4 nút mặt trước đã được quy hoạch khác:
    - Nút 1 (`BTN_BACK`): Menu
    - Nút 2 (`BTN_CONFIRM`): Đọc (Mở sách)
    - Nút 3 (`BTN_LEFT`): Di chuyển sang trái (`◀`)
    - Nút 4 (`BTN_RIGHT`): Di chuyển sang phải (`▶`)
  - Đoạn kiểm tra `releasedFrontButton == HalGPIO::BTN_RIGHT` áp dụng chung cho cả khối `usesMinutaHomeLayout()` mà không loại trừ Quartum. Do đó, khi người dùng bấm nút Right để chuyển bìa, `buttonNavigator` di chuyển con trỏ highlight sang bìa kế tiếp, nhưng ngay khi nhả nút Right ra, điều kiện `releasedFrontButton == HalGPIO::BTN_RIGHT` trở thành `true` và kích hoạt ngay `openSelectedBook()`.

---

## 3. Các phương án giải quyết & Đề xuất tối ưu

Dựa trên yêu cầu của bạn ("chuyển Cài đặt vào menu hoặc thêm nút mở Cài đặt thay thế Left/Right, và đề xuất giải pháp tối ưu cho cả 2 vấn đề"), dưới đây là 3 phương án:

### Phương án 1 (Khuyến nghị - Tối ưu và trực quan nhất) ⭐
- **Giải pháp:**
  1. **Sửa dứt điểm lỗi tự mở sách:** Loại bỏ kích hoạt mở sách từ `BTN_RIGHT` đối với Quartum:
     ```cpp
     if (releasedFrontButton == HalGPIO::BTN_CONFIRM ||
         mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
         (!isQuartumTheme() && releasedFrontButton == HalGPIO::BTN_RIGHT)) {
       openSelectedBook();
       return;
     }
     ```
     Lúc này, phím `Left` (`◀`) và `Right` (`▶`) ở mặt trước (cùng phím hông `Up`/`Down`) chỉ thuần túy làm nhiệm vụ di chuyển con trỏ highlight giữa 4 bìa sách. Chỉ khi bấm nút `Confirm` ("Đọc") hoặc chạm trực tiếp vào bìa thì sách mới mở.
  2. **Đưa Cài đặt vào Menu pop-up:** Thêm `tr(STR_SETTINGS_TITLE)` vào `buildMinimalMenuItems` và gọi `onSettingsOpen()` trong `activateMinutaMenuAction`.
- **Bố cục nhãn nút mặt trước:** `[Menu]` `[Đọc]` `[◀]` `[▶]`
- **Ưu điểm:**
  - Giữ trọn vẹn khả năng điều hướng 2 chiều (`◀` và `▶`) trên màn hình chính, thao tác cực kỳ mượt mà.
  - Menu pop-up chứa đầy đủ các tính năng hệ thống (Thư viện, Truyền tệp, Cài đặt...), đúng chuẩn thiết kế chung của firmware.
  - Đúng với nhãn hiển thị và không gây nhầm lẫn nút bấm.

---

### Phương án 2 (Thay thế nút Left mặt trước bằng Cài đặt)
- **Giải pháp:**
  1. Thay đổi sơ đồ nút bấm mặt trước của Quartum:
     - Nút 1 (`BTN_BACK`): Menu
     - Nút 2 (`BTN_CONFIRM`): Đọc
     - Nút 3 (`BTN_LEFT`): **Cài đặt** (Bấm để vào thẳng màn hình Cài đặt)
     - Nút 4 (`BTN_RIGHT`): **Chuyển bìa** (Bấm để chuyển xoay vòng qua 4 bìa: 0 -> 1 -> 2 -> 3 -> 0)
  2. Vẫn đưa Cài đặt vào Menu pop-up để người dùng có thể mở bằng cả 2 cách.
- **Bố cục nhãn nút mặt trước:** `[Menu]` `[Đọc]` `[Cài đặt]` `[Chuyển]`
- **Ưu điểm:** Có nút mở Cài đặt trực tiếp 1 chạm ngay từ màn hình chính.
- **Nhược điểm:** Mất nút lùi `◀` ở mặt trước. Khi duyệt bìa chỉ có thể bấm nút Right xoay vòng tiến tới, nếu muốn lùi lại phải dùng phím hông.

---

### Phương án 3 (Kết hợp: Phương án 1 + Phím tắt nhấn giữ vào Cài đặt)
- **Giải pháp:**
  1. Giữ nguyên điều hướng 2 chiều `[Menu]` `[Đọc]` `[◀]` `[▶]` và sửa dứt điểm lỗi tự mở sách như Phương án 1.
  2. Đưa Cài đặt vào Menu pop-up (bấm nhả nút Menu -> chọn Cài đặt).
  3. **Thêm phím tắt:** Nhấn giữ nút Menu (`BTN_BACK`) trong 800ms sẽ mở thẳng màn hình Cài đặt mà không cần thông qua menu pop-up.
- **Bố cục nhãn nút mặt trước:** `[Menu]` `[Đọc]` `[◀]` `[▶]`
- **Ưu điểm:**
  - Vừa có đầy đủ 2 phím điều hướng `◀` và `▶` tiện lợi.
  - Vừa có Cài đặt trong menu pop-up.
  - Vừa có thao tác mở Cài đặt siêu nhanh bằng 1 nút bấm (nhấn giữ nút Menu 1 giây).

---

## 4. Kế hoạch triển khai kỹ thuật (Implementation Plan)

### File bị tác động:
- `src/activities/home/HomeActivity.cpp`

### Các bước cụ thể:
1. **Bổ sung Settings vào Menu Minuta (`buildMinimalMenuItems`):**
   - Thêm `items.push({tr(STR_SETTINGS_TITLE), Settings, HomeMenuAction::Settings});` vào cuối `buildMinimalMenuItems()`.
2. **Kích hoạt Settings trong callback Menu (`activateMinutaMenuAction`):**
   - Thêm `onSettingsOpen();` vào `case HomeMenuAction::Settings:` trong `activateMinutaMenuAction()`.
   - Đồng thời bổ sung `onSettingsOpen();` vào `case HomeMenuAction::Settings:` trong `activateMinimalMenuAction()` để đồng bộ.
3. **Khắc phục lỗi tự mở sách khi nhả nút Right trong Quartum:**
   - Điều chỉnh logic kiểm tra `releasedFrontButton` trong `usesMinutaHomeLayout()`:
     Chỉ cho phép `BTN_RIGHT` mở sách nếu là theme Solum (`!isQuartumTheme()`). Với Quartum, `BTN_RIGHT` chỉ đóng vai trò chuyển vị trí sang phải.
4. **Nếu áp dụng Phương án 2 hoặc 3 (theo lựa chọn của bạn):**
   - Triển khai nhãn nút và logic nhấn giữ tương ứng.
5. **Kiểm thử biên dịch:**
   - Biên dịch firmware ESP32-C3: `pio run -e default`
   - Biên dịch Simulator: `pio run -e simulator`
   - Chạy test kiểm tra logic điều hướng `test/quartum_grid_nav`.

---

## 5. Tiêu chí hoàn thành (Success Criteria)
- [ ] Không còn hiện tượng tự động mở sách khi nhấn/nhả nút Right (`▶`) trên giao diện 4-cover.
- [ ] Con trỏ di chuyển chính xác và mượt mà giữa 4 bìa bằng phím `Left` / `Right` (và `Up` / `Down` phím hông).
- [ ] Phím Confirm ("Đọc") mở đúng cuốn sách đang được highlight.
- [ ] Mục "Cài đặt" xuất hiện đầy đủ trong menu pop-up và mở đúng màn hình Settings khi bấm chọn.
- [ ] Theme Solum (1-bìa) và các theme khác giữ nguyên tính ổn định, không bị ảnh hưởng.
- [ ] Biên dịch thành công trên cả `default` và `simulator`, không phát sinh lỗi hoặc cảnh báo.
