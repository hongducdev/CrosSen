#pragma once

#include <cstdint>

// Vietnamese lunar calendar (âm lịch) conversion.
//
// The conversion follows the astronomical algorithm published by Hồ Ngọc Đức
// (https://www.informatik.uni-leipzig.de/~duc/amlich/), which is also what the
// Vietnamese lunar calendars in common use are built on:
//   * new moon instants from the Meeus/Chapront series,
//   * solar longitude stops (the 12 "trung khí") define month boundaries,
//   * UTC+7 (Vietnam) decides which civil day a new moon belongs to.
//
// Everything is computed on demand from the date; there is no lookup table, so
// supporting any year costs no flash. The maths is a few dozen sin() calls, run
// once per sleep screen render.
namespace LunarCalendar {

// Vietnam Standard Time. The lunar day rolls over at local midnight, so the
// caller should pass the device's configured offset when it differs.
constexpr float kVietnamTimeZoneHours = 7.0f;

struct LunarDate {
  uint8_t day = 1;        // 1..30
  uint8_t month = 1;      // 1..12
  uint16_t year = 2000;   // lunar year (the year of Tết, not the solar year)
  bool leap = false;      // true when this month is the intercalary ("nhuận") month
  uint8_t dayStem = 0;    // 0..9  (Giáp..Quý) for the civil day's can-chi
  uint8_t dayBranch = 0;  // 0..11 (Tý..Hợi) for the civil day's can-chi
};

// Gregorian calendar validity, including leap years.
bool isValidSolarDate(uint16_t year, uint8_t month, uint8_t day);

// Days in a Gregorian month, or 0 when the month is out of range.
uint8_t daysInMonth(uint16_t year, uint8_t month);

// Day of week where 0 is Monday and 6 is Sunday.
uint8_t weekdayIndex(uint16_t year, uint8_t month, uint8_t day);

// Julian day number for a Gregorian date (proleptic Gregorian calendar).
int32_t julianDayNumber(uint16_t year, uint8_t month, uint8_t day);

// Convert a Gregorian date to its Vietnamese lunar date. Returns false and
// leaves `out` untouched when the input is not a valid date or the year is
// outside the range the lunar series is trustworthy for.
bool fromSolarDate(uint16_t year, uint8_t month, uint8_t day, LunarDate& out,
                   float timeZoneHours = kVietnamTimeZoneHours);

// Can-chi of the lunar year and month the given lunar month belongs to.
void yearStemBranch(uint16_t lunarYear, uint8_t& stem, uint8_t& branch);
void monthStemBranch(uint16_t lunarYear, uint8_t lunarMonth, uint8_t& stem, uint8_t& branch);

// "Giáp", "Ất", ... and "Tý", "Sửu", ...
const char* stemName(uint8_t stem);
const char* branchName(uint8_t branch);

// "tháng Giêng", "tháng Hai", ... "tháng Chạp".
const char* lunarMonthName(uint8_t lunarMonth);

// "Mùng một" / "Rằm" for the two festive days of a lunar month, else nullptr.
const char* lunarDayName(uint8_t lunarDay);

// The six auspicious hours ("giờ hoàng đạo") of a day whose branch is
// `dayBranch`, as branch indices into branchName().
const uint8_t* auspiciousHours(uint8_t dayBranch);

// Upper bound of the lunar series we trust; older dates still convert but the
// Vietnamese calendar's official rules only cover the modern era.
constexpr uint16_t kMinSupportedYear = 1800;
constexpr uint16_t kMaxSupportedYear = 2199;

}  // namespace LunarCalendar
