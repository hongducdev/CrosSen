// Covers the almanac helpers the tear-off calendar page adds on top of the
// shared lunar engine, plus the cross-check that matters: the auspicious hours
// are selected by the same day branch the engine reports, so the two cannot
// drift apart.

#include <CalendarAlmanac.h>
#include <LunarCalendar.h>

#include <cassert>
#include <cstring>

int main() {
  // Vietnamese month names, including the two that are not "tháng <number>".
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(1), "tháng Giêng") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(2), "tháng Hai") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(9), "tháng Chín") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(11), "tháng Mười Một") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(12), "tháng Chạp") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(0), "") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(13), "") == 0);

  // Festive day names.
  assert(std::strcmp(CalendarAlmanac::lunarDayName(1), "Mùng một") == 0);
  assert(std::strcmp(CalendarAlmanac::lunarDayName(15), "Rằm") == 0);
  assert(CalendarAlmanac::lunarDayName(2) == nullptr);

  // Branch names, printed on the auspicious-hours line.
  assert(std::strcmp(CalendarAlmanac::branchName(0), "Tý") == 0);
  assert(std::strcmp(CalendarAlmanac::branchName(8), "Thân") == 0);
  assert(std::strcmp(CalendarAlmanac::branchName(11), "Hợi") == 0);
  assert(std::strcmp(CalendarAlmanac::branchName(12), "") == 0);
  assert(std::strcmp(CalendarAlmanac::branchName(-1), "") == 0);

  // Every row stays inside the twelve branches, paired branches share one row,
  // and out-of-range input wraps instead of reading past the table.
  for (int branch = 0; branch < 12; ++branch) {
    const uint8_t* row = CalendarAlmanac::auspiciousHours(branch);
    for (int i = 0; i < 6; ++i) {
      assert(row[i] < 12);
    }
    const uint8_t* wrapped = CalendarAlmanac::auspiciousHours(branch + 12);
    for (int i = 0; i < 6; ++i) {
      assert(row[i] == wrapped[i]);
    }
    if (branch + 6 < 12) {
      const uint8_t* paired = CalendarAlmanac::auspiciousHours(branch + 6);
      for (int i = 0; i < 6; ++i) {
        assert(row[i] == paired[i]);
      }
    }
  }
  assert(CalendarAlmanac::auspiciousHours(-1) == CalendarAlmanac::auspiciousHours(11));

  // The reference page this feature copies is 21/03/2024 = 12/2, ngày Giáp Thân.
  // A Thân day uses the Tý, Sửu, Thìn, Tỵ, Mùi, Tuất row.
  const LunarDate reference = solarToLunar(21, 3, 2024);
  assert(reference.day == 12 && reference.month == 2 && !reference.isLeapMonth);
  const CanChi dayCanChi = getCanChiDay(21, 3, 2024);
  assert(std::strcmp(dayCanChi.can, "Giáp") == 0);
  assert(std::strcmp(dayCanChi.chi, "Thân") == 0);

  const int dayBranch = (static_cast<int>(jdFromGregorian(21, 3, 2024)) + 1) % 12;
  assert(dayBranch == 8);
  const uint8_t* hours = CalendarAlmanac::auspiciousHours(dayBranch);
  static constexpr uint8_t kExpectedHours[6] = {0, 1, 4, 5, 7, 10};
  static constexpr const char* kExpectedHourNames[6] = {"Tý", "Sửu", "Thìn", "Tỵ", "Mùi", "Tuất"};
  for (int i = 0; i < 6; ++i) {
    assert(hours[i] == kExpectedHours[i]);
    assert(std::strcmp(CalendarAlmanac::branchName(hours[i]), kExpectedHourNames[i]) == 0);
  }

  // The leap month the page prints for the 2023 knife-edge must be tháng Hai
  // nhuận, which is what the engine reports for 22/03/2023.
  const LunarDate leap = solarToLunar(22, 3, 2023);
  assert(leap.day == 1 && leap.month == 2 && leap.isLeapMonth);
  assert(std::strcmp(CalendarAlmanac::lunarMonthName(leap.month), "tháng Hai") == 0);

  return 0;
}
