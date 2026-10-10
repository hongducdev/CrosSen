#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../lib/LunarCalendar/LunarCalendar.h"

int main() {
  printf("Starting LunarCalendar tests...\n");

  // 1. Tết Giáp Thìn 2024: Feb 10, 2024 -> 1/1/2024 ÂL
  {
    LunarDate l2024 = solarToLunar(10, 2, 2024);
    assert(l2024.day == 1);
    assert(l2024.month == 1);
    assert(l2024.year == 2024);
    assert(!l2024.isLeapMonth);

    CanChi y = getCanChiYear(l2024.year);
    assert(std::strcmp(y.can, "Giáp") == 0);
    assert(std::strcmp(y.chi, "Thìn") == 0);

    CanChi m = getCanChiMonth(l2024.month, l2024.year);
    assert(std::strcmp(m.can, "Bính") == 0);
    assert(std::strcmp(m.chi, "Dần") == 0);

    CanChi d = getCanChiDay(10, 2, 2024);
    assert(std::strcmp(d.can, "Giáp") == 0);
    assert(std::strcmp(d.chi, "Thìn") == 0);

    char buf[32];
    formatCanChi(y, buf, sizeof(buf));
    assert(std::strcmp(buf, "Giáp Thìn") == 0);

    const char* hol = getVietnameseHoliday(10, 2, l2024);
    assert(hol != nullptr);
    assert(std::strcmp(hol, "Mùng 1 Tết Nguyên Đán") == 0);
  }

  // 2. Tết Ất Tỵ 2025: Jan 29, 2025 -> 1/1/2025 ÂL
  {
    LunarDate l2025 = solarToLunar(29, 1, 2025);
    assert(l2025.day == 1);
    assert(l2025.month == 1);
    assert(l2025.year == 2025);
    assert(!l2025.isLeapMonth);

    CanChi y = getCanChiYear(l2025.year);
    assert(std::strcmp(y.can, "Ất") == 0);
    assert(std::strcmp(y.chi, "Tỵ") == 0);
  }

  // 3. Tết Bính Ngọ 2026: Feb 17, 2026 -> 1/1/2026 ÂL
  {
    LunarDate l2026 = solarToLunar(17, 2, 2026);
    assert(l2026.day == 1);
    assert(l2026.month == 1);
    assert(l2026.year == 2026);
    assert(!l2026.isLeapMonth);

    CanChi y = getCanChiYear(l2026.year);
    assert(std::strcmp(y.can, "Bính") == 0);
    assert(std::strcmp(y.chi, "Ngọ") == 0);
  }

  // 4. Leap Month: 2025 has leap month 6 (tháng 6 nhuận). July 25, 2025 is 1/6 nhuận
  {
    LunarDate leap2025 = solarToLunar(25, 7, 2025);
    assert(leap2025.day == 1);
    assert(leap2025.month == 6);
    assert(leap2025.isLeapMonth == true);
  }

  // 5. Solar Terms (Tiết khí)
  {
    // Around March 20/21 is Spring Equinox (Xuân Phân)
    const char* termSpring = getSolarTermName(21, 3, 2026);
    // Around June 21/22 is Summer Solstice (Hạ Chí)
    const char* termSummer = getSolarTermName(22, 6, 2026);
    // Around Sept 23/24 is Autumn Equinox (Thu Phân)
    const char* termAutumn = getSolarTermName(24, 9, 2026);
    // Around Dec 22/23 is Winter Solstice (Đông Chí)
    const char* termWinter = getSolarTermName(23, 12, 2026);

    printf("Spring (21/3): %s\n", termSpring);
    printf("Summer (22/6): %s\n", termSummer);
    printf("Autumn (24/9): %s\n", termAutumn);
    printf("Winter (23/12): %s\n", termWinter);
    fflush(stdout);
    assert(std::strcmp(termSpring, "Xuân Phân") == 0);
    assert(std::strcmp(termSummer, "Hạ Chí") == 0);
    assert(std::strcmp(termAutumn, "Thu Phân") == 0);
    assert(std::strcmp(termWinter, "Đông Chí") == 0);
  }

  // 6. National Holidays
  {
    LunarDate lApr30 = solarToLunar(30, 4, 2026);
    const char* holApr30 = getVietnameseHoliday(30, 4, lApr30);
    assert(holApr30 != nullptr);
    assert(std::strcmp(holApr30, "Giải phóng miền Nam") == 0);

    LunarDate lSep2 = solarToLunar(2, 9, 2026);
    const char* holSep2 = getVietnameseHoliday(2, 9, lSep2);
    assert(holSep2 != nullptr);
    assert(std::strcmp(holSep2, "Quốc khánh") == 0);
  }

  // 7. Gregorian helpers used by the sleep screen
  {
    assert(gregorianDaysInMonth(1, 2026) == 31);
    assert(gregorianDaysInMonth(2, 2026) == 28);
    assert(gregorianDaysInMonth(2, 2024) == 29);
    assert(gregorianDaysInMonth(2, 2000) == 29);
    assert(gregorianDaysInMonth(2, 1900) == 28);
    assert(gregorianDaysInMonth(4, 2026) == 30);
    assert(gregorianDaysInMonth(12, 2026) == 31);

    // 2026-10-09 is a Friday (0 = Sunday).
    assert(gregorianWeekday(9, 10, 2026) == 5);
    assert(gregorianWeekday(1, 1, 2026) == 4);   // Thursday
    assert(gregorianWeekday(17, 2, 2026) == 2);  // Tuesday (Tết Bính Ngọ)
  }

  // 8. UTC -> local midnight rollover (the RTC stores UTC; Vietnam is UTC+7)
  {
    // 2026-02-16 18:30 UTC is 2026-02-17 01:30 in Vietnam: the lunar date must
    // move on to Mùng 1 Tết Bính Ngọ.
    {
      int y = 2026, m = 2, d = 16, h = 18, mi = 30;
      shiftCivilMinutes(y, m, d, h, mi, 7 * 60);
      assert(y == 2026 && m == 2 && d == 17 && h == 1 && mi == 30);
      LunarDate lunar = solarToLunar(d, m, y);
      assert(lunar.day == 1 && lunar.month == 1 && lunar.year == 2026);
    }

    // Local midnight must not shift the Gregorian date.
    {
      int y = 2026, m = 2, d = 17, h = 1, mi = 30;
      shiftCivilMinutes(y, m, d, h, mi, -7 * 60);
      assert(y == 2026 && m == 2 && d == 16 && h == 18 && mi == 30);
    }

    // Month and year boundaries roll over correctly.
    {
      int y = 2026, m = 12, d = 31, h = 20, mi = 0;
      shiftCivilMinutes(y, m, d, h, mi, 7 * 60);
      assert(y == 2027 && m == 1 && d == 1 && h == 3 && mi == 0);

      int y2 = 2026, m2 = 1, d2 = 1, h2 = 3, mi2 = 0;
      shiftCivilMinutes(y2, m2, d2, h2, mi2, -7 * 60);
      assert(y2 == 2025 && m2 == 12 && d2 == 31 && h2 == 20 && mi2 == 0);
    }

    // Leap-day boundary (2024-02-29 -> 2024-03-01).
    {
      int y = 2024, m = 2, d = 29, h = 20, mi = 0;
      shiftCivilMinutes(y, m, d, h, mi, 7 * 60);
      assert(y == 2024 && m == 3 && d == 1 && h == 3 && mi == 0);
    }

    // Negative and sub-day offsets keep the time in range.
    {
      int y = 2026, m = 6, d = 15, h = 0, mi = 30;
      shiftCivilMinutes(y, m, d, h, mi, -60);
      assert(y == 2026 && m == 6 && d == 14 && h == 23 && mi == 30);
    }
  }

  // 9. Historical leap month: 2023 leap month
  // Ground truth (Lịch Vạn Niên Việt Nam):
  // 22/01/2023 -> 1/1 (Tết Quý Mão)
  // 20/02/2023 -> 1/2 (Mùng 1 tháng Hai, NOT leap)
  // 21/03/2023 -> 30/2 (Ngày cuối tháng Hai)
  // 22/03/2023 -> 1/2 nhuận (Tháng 2 nhuận bắt đầu)
  // 19/04/2023 -> 29/2 nhuận (Ngày cuối tháng 2 nhuận)
  // 20/04/2023 -> 1/3 (Mùng 1 tháng Ba)
  {
    LunarDate lTet2023 = solarToLunar(22, 1, 2023);
    assert(lTet2023.day == 1 && lTet2023.month == 1 && !lTet2023.isLeapMonth);

    LunarDate lThangHai = solarToLunar(20, 2, 2023);
    assert(lThangHai.day == 1 && lThangHai.month == 2 && !lThangHai.isLeapMonth);

    LunarDate lCuoiThangHai = solarToLunar(21, 3, 2023);
    assert(lCuoiThangHai.day == 30 && lCuoiThangHai.month == 2 && !lCuoiThangHai.isLeapMonth);

    LunarDate lThangHaiNhuan = solarToLunar(22, 3, 2023);
    assert(lThangHaiNhuan.day == 1 && lThangHaiNhuan.month == 2 && lThangHaiNhuan.isLeapMonth);

    LunarDate lCuoiThangNhuan = solarToLunar(19, 4, 2023);
    assert(lCuoiThangNhuan.day == 29 && lCuoiThangNhuan.month == 2 && lCuoiThangNhuan.isLeapMonth);

    LunarDate lThangBa = solarToLunar(20, 4, 2023);
    assert(lThangBa.day == 1 && lThangBa.month == 3 && !lThangBa.isLeapMonth);
  }

  // 10. Major Vietnamese traditional holidays
  {
    // Giỗ Tổ Hùng Vương: 10/3 ÂL (2024-04-18)
    LunarDate lGioTo = solarToLunar(18, 4, 2024);
    assert(lGioTo.day == 10 && lGioTo.month == 3);
    const char* hGioTo = getVietnameseHoliday(18, 4, lGioTo);
    assert(hGioTo != nullptr && std::strcmp(hGioTo, "Giỗ Tổ Hùng Vương") == 0);

    // Tết Đoan Ngọ: 5/5 ÂL (2024-06-10)
    LunarDate lDoanNgo = solarToLunar(10, 6, 2024);
    assert(lDoanNgo.day == 5 && lDoanNgo.month == 5);
    const char* hDoanNgo = getVietnameseHoliday(10, 6, lDoanNgo);
    assert(hDoanNgo != nullptr && std::strcmp(hDoanNgo, "Tết Đoan Ngọ") == 0);

    // Lễ Vu Lan: 15/7 ÂL (2024-08-18)
    LunarDate lVuLan = solarToLunar(18, 8, 2024);
    assert(lVuLan.day == 15 && lVuLan.month == 7);
    const char* hVuLan = getVietnameseHoliday(18, 8, lVuLan);
    assert(hVuLan != nullptr && std::strcmp(hVuLan, "Lễ Vu Lan") == 0);

    // Tết Trung Thu: 15/8 ÂL (2024-09-17)
    LunarDate lTrungThu = solarToLunar(17, 9, 2024);
    assert(lTrungThu.day == 15 && lTrungThu.month == 8);
    const char* hTrungThu = getVietnameseHoliday(17, 9, lTrungThu);
    assert(hTrungThu != nullptr && std::strcmp(hTrungThu, "Tết Trung Thu") == 0);

    // Ông Táo chầu trời: 23/12 ÂL (2024-02-02)
    LunarDate lOngTao = solarToLunar(2, 2, 2024);
    assert(lOngTao.day == 23 && lOngTao.month == 12 && lOngTao.year == 2023);
    const char* hOngTao = getVietnameseHoliday(2, 2, lOngTao);
    assert(hOngTao != nullptr && std::strcmp(hOngTao, "Ông Táo chầu trời") == 0);

    // Đêm Giao Thừa: 30/12/2023 ÂL (2024-02-09)
    LunarDate lGiaoThua = solarToLunar(9, 2, 2024);
    assert(lGiaoThua.day == 30 && lGiaoThua.month == 12 && lGiaoThua.year == 2023);
    const char* hGiaoThua = getVietnameseHoliday(9, 2, lGiaoThua);
    assert(hGiaoThua != nullptr && std::strcmp(hGiaoThua, "Đêm Giao Thừa") == 0);

    // Mùng Một hàng tháng (2024-03-10 is 1/2 ÂL)
    LunarDate lMung1 = solarToLunar(10, 3, 2024);
    assert(lMung1.day == 1 && lMung1.month == 2);
    const char* hMung1 = getVietnameseHoliday(10, 3, lMung1);
    assert(hMung1 != nullptr && std::strcmp(hMung1, "Mùng Một") == 0);

    // Ngày Rằm hàng tháng (2024-03-24 is 15/2 ÂL)
    LunarDate lRam = solarToLunar(24, 3, 2024);
    assert(lRam.day == 15 && lRam.month == 2);
    const char* hRam = getVietnameseHoliday(24, 3, lRam);
    assert(hRam != nullptr && std::strcmp(hRam, "Ngày Rằm") == 0);
  }

  printf("All LunarCalendar tests passed successfully!\n");
  return 0;
}
