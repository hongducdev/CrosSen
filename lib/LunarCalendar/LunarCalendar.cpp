#include "LunarCalendar.h"

#include <cmath>

namespace LunarCalendar {
namespace {

// Julian day of 1970-01-01, used as the epoch reference of the new moon series.
constexpr double kNewMoonEpoch = 2415021.076998695;
constexpr double kSynodicMonth = 29.530588853;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

int32_t floorDiv(const int64_t numerator, const int64_t denominator) {
  const int64_t quotient = numerator / denominator;
  const int64_t remainder = numerator % denominator;
  return static_cast<int32_t>(remainder != 0 && ((remainder < 0) != (denominator < 0)) ? quotient - 1 : quotient);
}

// Days from the Gregorian calendar reform reference used by the series.
int32_t referenceDayNumber(const int32_t julianDay) { return julianDay - 2415021; }

// The k-th new moon after 1900-01-01, as a (fractional) Julian day number.
double newMoonDay(const double k) {
  const double t = k / 1236.85;
  const double t2 = t * t;
  const double t3 = t2 * t;

  double jd = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 - 0.000000155 * t3;
  jd += 0.00033 * std::sin((166.56 + 132.87 * t - 0.009173 * t2) * kDegToRad);

  const double m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3;
  const double mPrime = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3;
  const double f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3;

  double c1 = (0.1734 - 0.000393 * t) * std::sin(m * kDegToRad) + 0.0021 * std::sin(2 * kDegToRad * m);
  c1 -= 0.4068 * std::sin(mPrime * kDegToRad) + 0.0161 * std::sin(2 * kDegToRad * mPrime);
  c1 -= 0.0004 * std::sin(3 * kDegToRad * mPrime);
  c1 += 0.0104 * std::sin(2 * kDegToRad * f) - 0.0051 * std::sin((m + mPrime) * kDegToRad);
  c1 -= 0.0074 * std::sin((m - mPrime) * kDegToRad) + 0.0004 * std::sin((2 * f + m) * kDegToRad);
  c1 -= 0.0004 * std::sin((2 * f - m) * kDegToRad) - 0.0006 * std::sin((2 * f + mPrime) * kDegToRad);
  c1 += 0.0010 * std::sin((2 * f - mPrime) * kDegToRad) + 0.0005 * std::sin((2 * mPrime + m) * kDegToRad);

  const double deltaT = t < -11 ? 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 0.000000081 * t * t3
                                : -0.000278 + 0.000265 * t + 0.000262 * t2;
  return jd + c1 - deltaT;
}

// Civil day (local) that holds the k-th new moon.
int32_t newMoonLocalDay(const double k, const float timeZoneHours) {
  return static_cast<int32_t>(std::floor(newMoonDay(k) + 0.5 + timeZoneHours / 24.0));
}

// Solar longitude in 1/12th turns (0..11). A new month starts on the day whose
// new moon is followed by a solar longitude that is a multiple of 30 degrees.
int32_t sunLongitudeSegment(const int32_t dayNumber, const float timeZoneHours) {
  const double t = (dayNumber - 2451545.5 - timeZoneHours / 24.0) / 36525.0;
  const double t2 = t * t;
  const double m = 357.52910 + 35999.05030 * t - 0.0001559 * t2 - 0.00000048 * t * t2;
  const double l0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2;

  double dl = (1.914600 - 0.004817 * t - 0.000014 * t2) * std::sin(m * kDegToRad);
  dl += (0.019993 - 0.000101 * t) * std::sin(2 * m * kDegToRad) + 0.000290 * std::sin(3 * m * kDegToRad);

  double l = (l0 + dl) * kDegToRad;
  l -= 2 * kPi * std::floor(l / (2 * kPi));
  return static_cast<int32_t>(std::floor(l / kPi * 6));
}

// Day number at which the lunar month containing November of `year` starts,
// i.e. the month holding the winter solstice.
int32_t lunarMonth11Day(const uint16_t year, const float timeZoneHours) {
  const int32_t offset = referenceDayNumber(julianDayNumber(year, 12, 31));
  const double k = std::floor(offset / kSynodicMonth);
  int32_t newMoon = newMoonLocalDay(k, timeZoneHours);
  if (sunLongitudeSegment(newMoon, timeZoneHours) >= 9) {
    newMoon = newMoonLocalDay(k - 1, timeZoneHours);
  }
  return newMoon;
}

// Offset (in months) of the leap month from the month-11 anchor.
int32_t leapMonthOffset(const int32_t month11Day, const float timeZoneHours) {
  const double k = std::floor((month11Day - kNewMoonEpoch) / kSynodicMonth + 0.5);
  int32_t arc = sunLongitudeSegment(newMoonLocalDay(k + 1, timeZoneHours), timeZoneHours);
  int32_t last = arc;
  int32_t i = 1;
  while (i < 14) {
    last = arc;
    i++;
    arc = sunLongitudeSegment(newMoonLocalDay(k + i, timeZoneHours), timeZoneHours);
    if (arc == last) break;
  }
  return i - 1;
}

}  // namespace

uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
  if (month < 1 || month > 12) return 0;
  static constexpr uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
    return 29;
  }
  return kDaysInMonth[month - 1];
}

bool isValidSolarDate(const uint16_t year, const uint8_t month, const uint8_t day) {
  if (day < 1) return false;
  const uint8_t limit = daysInMonth(year, month);
  return limit != 0 && day <= limit;
}

int32_t julianDayNumber(const uint16_t year, const uint8_t month, const uint8_t day) {
  const int32_t a = (14 - static_cast<int32_t>(month)) / 12;
  const int32_t y = static_cast<int32_t>(year) + 4800 - a;
  const int32_t m = static_cast<int32_t>(month) + 12 * a - 3;
  return day + floorDiv(153 * m + 2, 5) + 365 * y + floorDiv(y, 4) - floorDiv(y, 100) + floorDiv(y, 400) - 32045;
}

uint8_t weekdayIndex(const uint16_t year, const uint8_t month, const uint8_t day) {
  if (!isValidSolarDate(year, month, day)) return 0;
  // Julian day 0 was a Monday, so the remainder indexes Monday-first weeks.
  const int32_t remainder = julianDayNumber(year, month, day) % 7;
  return static_cast<uint8_t>(remainder < 0 ? remainder + 7 : remainder);
}

bool fromSolarDate(const uint16_t year, const uint8_t month, const uint8_t day, LunarDate& out,
                   const float timeZoneHours) {
  if (year < kMinSupportedYear || year > kMaxSupportedYear) return false;
  if (!isValidSolarDate(year, month, day)) return false;

  const int32_t dayNumber = julianDayNumber(year, month, day);
  const double k = std::floor((dayNumber - kNewMoonEpoch) / kSynodicMonth);
  int32_t monthStart = newMoonLocalDay(k + 1, timeZoneHours);
  if (monthStart > dayNumber) {
    monthStart = newMoonLocalDay(k, timeZoneHours);
  }

  int32_t month11 = lunarMonth11Day(year, timeZoneHours);
  int32_t nextMonth11 = month11;
  uint16_t lunarYear = year;
  if (month11 >= monthStart) {
    month11 = lunarMonth11Day(static_cast<uint16_t>(year - 1), timeZoneHours);
  } else {
    lunarYear = static_cast<uint16_t>(year + 1);
    nextMonth11 = lunarMonth11Day(static_cast<uint16_t>(year + 1), timeZoneHours);
  }

  const int32_t lunarDay = dayNumber - monthStart + 1;
  const int32_t diff = floorDiv(monthStart - month11, 29);
  bool leap = false;
  int32_t lunarMonth = diff + 11;
  if (nextMonth11 - month11 > 365) {
    const int32_t leapOffset = leapMonthOffset(month11, timeZoneHours);
    if (diff >= leapOffset) {
      lunarMonth = diff + 10;
      leap = diff == leapOffset;
    }
  }
  if (lunarMonth > 12) lunarMonth -= 12;

  // A month numbered 11 or 12 that is closer to the previous month-11 belongs
  // to the previous lunar year.
  if (lunarMonth >= 11 && diff < 4) {
    if (lunarYear > 0) lunarYear -= 1;
  }

  out.day = static_cast<uint8_t>(lunarDay);
  out.month = static_cast<uint8_t>(lunarMonth);
  out.year = lunarYear;
  out.leap = leap;
  // The day's can-chi advances one step per civil day, independent of the
  // lunar month, so it comes straight from the Julian day number.
  out.dayStem = static_cast<uint8_t>((dayNumber + 9) % 10);
  out.dayBranch = static_cast<uint8_t>((dayNumber + 1) % 12);
  return true;
}

void yearStemBranch(const uint16_t lunarYear, uint8_t& stem, uint8_t& branch) {
  stem = static_cast<uint8_t>((lunarYear + 6) % 10);
  branch = static_cast<uint8_t>((lunarYear + 8) % 12);
}

void monthStemBranch(const uint16_t lunarYear, const uint8_t lunarMonth, uint8_t& stem, uint8_t& branch) {
  // The month pillar continues the year's stem sequence; month 1 (tháng Giêng)
  // of 2024 is Bính Dần, which anchors the +3 offset.
  stem = static_cast<uint8_t>((static_cast<uint32_t>(lunarYear) * 12u + lunarMonth + 3u) % 10u);
  branch = static_cast<uint8_t>((lunarMonth + 1u) % 12u);
}

const char* stemName(const uint8_t stem) {
  static constexpr const char* kStems[] = {"Giáp", "Ất", "Bính", "Đinh", "Mậu", "Kỷ", "Canh", "Tân", "Nhâm", "Quý"};
  return stem < 10 ? kStems[stem] : "";
}

const char* branchName(const uint8_t branch) {
  static constexpr const char* kBranches[] = {"Tý",  "Sửu", "Dần",  "Mão", "Thìn", "Tỵ",
                                              "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};
  return branch < 12 ? kBranches[branch] : "";
}

const char* lunarMonthName(const uint8_t lunarMonth) {
  static constexpr const char* kMonths[] = {"tháng Giêng", "tháng Hai",  "tháng Ba",       "tháng Tư",
                                            "tháng Năm",   "tháng Sáu",  "tháng Bảy",      "tháng Tám",
                                            "tháng Chín",  "tháng Mười", "tháng Mười Một", "tháng Chạp"};
  return (lunarMonth >= 1 && lunarMonth <= 12) ? kMonths[lunarMonth - 1] : "";
}

const char* lunarDayName(const uint8_t lunarDay) {
  if (lunarDay == 1) return "Mùng một";
  if (lunarDay == 15) return "Rằm";
  return nullptr;
}

const uint8_t* auspiciousHours(const uint8_t dayBranch) {
  // Branch pairs sharing a cycle (Tý/Ngọ, Sửu/Mùi, ...) share the same six
  // auspicious hours, so six rows cover all twelve day branches.
  static constexpr uint8_t kHours[6][6] = {
      {0, 1, 3, 6, 8, 9},    // Tý, Ngọ
      {2, 3, 5, 8, 10, 11},  // Sửu, Mùi
      {0, 1, 4, 5, 7, 10},   // Dần, Thân
      {2, 3, 6, 7, 9, 0},    // Mão, Dậu
      {2, 4, 5, 8, 10, 11},  // Thìn, Tuất
      {1, 4, 6, 7, 10, 11},  // Tỵ, Hợi
  };
  return kHours[dayBranch % 6];
}

}  // namespace LunarCalendar
