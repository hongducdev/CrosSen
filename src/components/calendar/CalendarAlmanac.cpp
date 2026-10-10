#include "CalendarAlmanac.h"

namespace CalendarAlmanac {

const char* lunarMonthName(const int lunarMonth) {
  static constexpr const char* kMonths[] = {"tháng Giêng", "tháng Hai",  "tháng Ba",       "tháng Tư",
                                            "tháng Năm",   "tháng Sáu",  "tháng Bảy",      "tháng Tám",
                                            "tháng Chín",  "tháng Mười", "tháng Mười Một", "tháng Chạp"};
  return (lunarMonth >= 1 && lunarMonth <= 12) ? kMonths[lunarMonth - 1] : "";
}

const char* lunarDayName(const int lunarDay) {
  if (lunarDay == 1) return "Mùng một";
  if (lunarDay == 15) return "Rằm";
  return nullptr;
}

const char* branchName(const int branch) {
  static constexpr const char* kBranches[] = {"Tý",  "Sửu", "Dần",  "Mão", "Thìn", "Tỵ",
                                              "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};
  return (branch >= 0 && branch <= 11) ? kBranches[branch] : "";
}

const uint8_t* auspiciousHours(const int dayBranch) {
  // Branches that share a cycle (Tý/Ngọ, Sửu/Mùi, ...) share the same six
  // auspicious hours, so six rows cover all twelve day branches.
  static constexpr uint8_t kHours[6][6] = {
      {0, 1, 3, 6, 8, 9},    // Tý, Ngọ
      {2, 3, 5, 8, 10, 11},  // Sửu, Mùi
      {0, 1, 4, 5, 7, 10},   // Dần, Thân
      {2, 3, 6, 7, 9, 0},    // Mão, Dậu
      {2, 4, 5, 8, 10, 11},  // Thìn, Tuất
      {1, 4, 6, 7, 10, 11},  // Tỵ, Hợi
  };
  int branch = dayBranch % 12;
  if (branch < 0) branch += 12;
  return kHours[branch % 6];
}

}  // namespace CalendarAlmanac
