// Reference values are published Vietnamese lunar dates: the Tết (1/1) dates,
// Trung thu (15/8), the Giỗ Tổ Hùng Vương (10/3) holidays, and the lunar date
// printed on the tear-off calendar page that this feature is modelled on
// (21/03/2024 -> 12/2 Giáp Thìn, tháng Đinh Mão, ngày Giáp Thân).

#include <LunarCalendar.h>

#include <cassert>
#include <cstring>

using LunarCalendar::LunarDate;

namespace {

void expectLunar(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t lunarDay,
                 const uint8_t lunarMonth, const uint16_t lunarYear, const bool leap = false) {
  LunarDate lunar;
  assert(LunarCalendar::fromSolarDate(year, month, day, lunar));
  assert(lunar.day == lunarDay);
  assert(lunar.month == lunarMonth);
  assert(lunar.year == lunarYear);
  assert(lunar.leap == leap);
}

bool yearHasLeapMonth(const uint16_t year, uint8_t& leapMonth) {
  for (uint8_t month = 1; month <= 12; ++month) {
    for (uint8_t day = 1; day <= 28; ++day) {
      LunarDate lunar;
      if (LunarCalendar::fromSolarDate(year, month, day, lunar) && lunar.leap) {
        leapMonth = lunar.month;
        return true;
      }
    }
  }
  return false;
}

}  // namespace

int main() {
  // Tết Nguyên Đán falls on lunar 1/1 of the new year.
  expectLunar(2023, 1, 22, 1, 1, 2023);
  expectLunar(2024, 2, 10, 1, 1, 2024);
  expectLunar(2025, 1, 29, 1, 1, 2025);
  expectLunar(2026, 2, 17, 1, 1, 2026);

  // Tết Trung thu is lunar 15/8.
  expectLunar(2023, 9, 29, 15, 8, 2023);
  expectLunar(2024, 9, 17, 15, 8, 2024);

  // Giỗ Tổ Hùng Vương is lunar 10/3.
  expectLunar(2024, 4, 18, 10, 3, 2024);
  expectLunar(2025, 4, 7, 10, 3, 2025);

  // The tear-off calendar page this feature copies: 21/03/2024.
  expectLunar(2024, 3, 21, 12, 2, 2024);

  LunarDate reference;
  assert(LunarCalendar::fromSolarDate(2024, 3, 21, reference));
  assert(reference.dayStem == 0 && std::strcmp(LunarCalendar::stemName(reference.dayStem), "Giáp") == 0);
  assert(reference.dayBranch == 8 && std::strcmp(LunarCalendar::branchName(reference.dayBranch), "Thân") == 0);

  uint8_t yearStem = 0;
  uint8_t yearBranch = 0;
  LunarCalendar::yearStemBranch(reference.year, yearStem, yearBranch);
  assert(std::strcmp(LunarCalendar::stemName(yearStem), "Giáp") == 0);
  assert(std::strcmp(LunarCalendar::branchName(yearBranch), "Thìn") == 0);

  uint8_t monthStem = 0;
  uint8_t monthBranch = 0;
  LunarCalendar::monthStemBranch(reference.year, reference.month, monthStem, monthBranch);
  assert(std::strcmp(LunarCalendar::stemName(monthStem), "Đinh") == 0);
  assert(std::strcmp(LunarCalendar::branchName(monthBranch), "Mão") == 0);
  assert(std::strcmp(LunarCalendar::lunarMonthName(reference.month), "tháng Hai") == 0);

  // Tết months keep the Vietnamese naming.
  assert(std::strcmp(LunarCalendar::lunarMonthName(1), "tháng Giêng") == 0);
  assert(std::strcmp(LunarCalendar::lunarMonthName(11), "tháng Mười Một") == 0);
  assert(std::strcmp(LunarCalendar::lunarMonthName(12), "tháng Chạp") == 0);
  assert(std::strcmp(LunarCalendar::lunarMonthName(0), "") == 0);

  // Festive lunar days.
  assert(std::strcmp(LunarCalendar::lunarDayName(1), "Mùng một") == 0);
  assert(std::strcmp(LunarCalendar::lunarDayName(15), "Rằm") == 0);
  assert(LunarCalendar::lunarDayName(2) == nullptr);

  // Giờ hoàng đạo for a Thân day, as printed on the reference page:
  // Tý, Sửu, Thìn, Tỵ, Mùi, Tuất.
  const uint8_t* hours = LunarCalendar::auspiciousHours(8);
  static constexpr uint8_t kExpectedHours[6] = {0, 1, 4, 5, 7, 10};
  for (int i = 0; i < 6; ++i) {
    assert(hours[i] == kExpectedHours[i]);
  }
  assert(std::strcmp(LunarCalendar::branchName(hours[0]), "Tý") == 0);
  assert(std::strcmp(LunarCalendar::branchName(hours[5]), "Tuất") == 0);
  // Paired branches share their hours, and every published row stays in range.
  for (uint8_t branch = 0; branch < 12; ++branch) {
    const uint8_t* row = LunarCalendar::auspiciousHours(branch);
    for (int i = 0; i < 6; ++i) {
      assert(row[i] < 12);
    }
    if (branch + 6 < 12) {
      const uint8_t* paired = LunarCalendar::auspiciousHours(static_cast<uint8_t>(branch + 6));
      for (int i = 0; i < 6; ++i) {
        assert(row[i] == paired[i]);
      }
    }
  }

  // Intercalary months: 2024 has none, 2025 repeats month 6, 2023 a month.
  uint8_t leapMonth = 0;
  assert(!yearHasLeapMonth(2024, leapMonth));
  assert(yearHasLeapMonth(2025, leapMonth) && leapMonth == 6);

  // Day can-chi advances by exactly one step per civil day.
  for (uint8_t day = 1; day < 28; ++day) {
    LunarDate today;
    LunarDate tomorrow;
    assert(LunarCalendar::fromSolarDate(2026, 6, day, today));
    assert(LunarCalendar::fromSolarDate(2026, 6, static_cast<uint8_t>(day + 1), tomorrow));
    assert(tomorrow.dayStem == (today.dayStem + 1) % 10);
    assert(tomorrow.dayBranch == (today.dayBranch + 1) % 12);
  }

  // Gregorian validity and the Julian day anchor.
  assert(LunarCalendar::isValidSolarDate(2024, 2, 29));
  assert(!LunarCalendar::isValidSolarDate(2023, 2, 29));
  assert(!LunarCalendar::isValidSolarDate(2024, 4, 31));
  assert(!LunarCalendar::isValidSolarDate(2024, 0, 1));
  assert(!LunarCalendar::isValidSolarDate(2024, 13, 1));
  assert(LunarCalendar::julianDayNumber(2024, 3, 21) == 2460391);

  // Out-of-range years are refused instead of returning invented dates.
  LunarDate untouched;
  assert(!LunarCalendar::fromSolarDate(1700, 1, 1, untouched));
  assert(!LunarCalendar::fromSolarDate(2300, 1, 1, untouched));
  assert(!LunarCalendar::fromSolarDate(2024, 2, 30, untouched));

  return 0;
}
