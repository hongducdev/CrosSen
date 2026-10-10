#include "LunarCalendar.h"

#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Ho Ngoc Duc Astronomical Algorithm (UTC+7 for Vietnam) ───────────────────

double jdFromGregorian(int d, int m, int y) {
  int a = (14 - m) / 12;
  int yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

static double newMoon(int k) {
  const double dr = M_PI / 180.0;
  double T = k / 1236.85;
  double T2 = T * T;
  double T3 = T2 * T;

  double Jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
  Jd1 += 0.00033 * std::sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);

  double M = (359.2242 + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3) * dr;
  double Mpr = (306.0253 + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3) * dr;
  double F = (21.2964 + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3) * dr;
  double T_2000 = T - 1.0;
  double E = 1.0 - 0.002516 * T_2000 - 0.0000074 * T_2000 * T_2000;
  double Omega = (124.7746 - 1.56375588 * (k - 1236.85)) * dr;

  // Jean Meeus, Astronomical Algorithms (2nd ed), Chapter 49, Table 49.A (New Moon periodic terms)
  double C1 = -0.40720 * std::sin(Mpr) + 0.17241 * E * std::sin(M) + 0.01608 * std::sin(2.0 * Mpr) +
              0.01039 * std::sin(2.0 * F) + 0.00739 * E * std::sin(Mpr - M) - 0.00514 * E * std::sin(Mpr + M) +
              0.00208 * E * E * std::sin(2.0 * M) - 0.00111 * std::sin(Mpr - 2.0 * F) -
              0.00057 * std::sin(Mpr + 2.0 * F) + 0.00056 * E * std::sin(2.0 * Mpr + M) -
              0.00042 * std::sin(3.0 * Mpr) + 0.00042 * E * std::sin(M + 2.0 * F) +
              0.00038 * E * std::sin(M - 2.0 * F) - 0.00024 * E * std::sin(2.0 * Mpr - M) - 0.00017 * std::sin(Omega) -
              0.00007 * std::sin(Mpr + 2.0 * M) + 0.00004 * std::sin(2.0 * (Mpr - F)) + 0.00004 * std::sin(3.0 * M) +
              0.00003 * std::sin(Mpr + M - 2.0 * F) + 0.00003 * std::sin(2.0 * (Mpr + F)) -
              0.00003 * std::sin(Mpr + M + 2.0 * F) + 0.00003 * std::sin(Mpr - M + 2.0 * F) -
              0.00002 * std::sin(Mpr - M - 2.0 * F) - 0.00002 * std::sin(3.0 * Mpr + M) + 0.00002 * std::sin(4.0 * Mpr);

  double deltat;
  if (T < -11) {
    deltat = 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3;
  } else {
    deltat = -0.000278 + 0.000265 * T + 0.000262 * T2;
  }
  return Jd1 + C1 - deltat;
}

static double sunLongitudeDeg(double jdn) {
  const double dr = M_PI / 180.0;
  double T = (jdn - 2451545.0) / 36525.0;
  double T2 = T * T;
  double M = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
  double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
  double DL = (1.9146 - 0.004817 * T - 0.000014 * T2) * std::sin(dr * M) +
              (0.019993 - 0.000101 * T) * std::sin(dr * 2 * M) + 0.00029 * std::sin(dr * 3 * M);
  double L = L0 + DL;
  L = L / 360.0 - std::floor(L / 360.0);
  return L * 360.0;
}

// Returns solar term index (0-11) for given JDN in given timezone
static int solarTermAt(double jdn, double tz) {
  return static_cast<int>(sunLongitudeDeg(jdn - 0.5 - tz / 24.0) / 30.0);
}

// Returns JDN of k-th new moon (adjusted for timezone)
static int newMoonDay(int k, double tz) { return static_cast<int>(newMoon(k) + 0.5 + tz / 24.0); }

// JDN of the 11th lunar month start for given Gregorian year
static int lunarMonth11Start(int yy, double tz) {
  double off = jdFromGregorian(31, 12, yy) - 2415021.076998695;
  int k = static_cast<int>(off / 29.530588853);
  int nm = newMoonDay(k, tz);
  if (solarTermAt(nm, tz) >= 9) nm = newMoonDay(k - 1, tz);
  return nm;
}

// Offset of the leap month within the 13-month year starting at a11
static int leapMonthOffset(int a11, double tz) {
  int k = static_cast<int>((a11 - 2415021.076998695) / 29.530588853 + 0.5);
  int i = 1;
  int arc = solarTermAt(newMoonDay(k + i, tz), tz);
  int last;
  do {
    last = arc;
    i++;
    arc = solarTermAt(newMoonDay(k + i, tz), tz);
  } while (arc != last && i < 14);
  return i - 1;
}

LunarDate solarToLunar(int day, int month, int year, double timeZone) {
  int dayNumber = static_cast<int>(jdFromGregorian(day, month, year));
  int k = static_cast<int>((dayNumber - 2415021.076998695) / 29.530588853);
  int monthStart = newMoonDay(k + 1, timeZone);
  if (monthStart > dayNumber) monthStart = newMoonDay(k, timeZone);

  int a11 = lunarMonth11Start(year, timeZone);
  int b11 = a11;
  int lunarYear;
  if (a11 >= monthStart) {
    lunarYear = year;
    a11 = lunarMonth11Start(year - 1, timeZone);
  } else {
    lunarYear = year + 1;
    b11 = lunarMonth11Start(year + 1, timeZone);
  }

  int lunarDay = dayNumber - monthStart + 1;
  int diff = static_cast<int>((monthStart - a11) / 29);
  int lunarMonth = diff + 11;
  bool isLeap = false;

  if (b11 - a11 > 365) {
    int leapOff = leapMonthOffset(a11, timeZone);
    if (diff >= leapOff) {
      lunarMonth = diff + 10;
      if (diff == leapOff) isLeap = true;
    }
  }

  if (lunarMonth > 12) lunarMonth -= 12;
  if (lunarMonth >= 11 && diff < 4) lunarYear -= 1;

  return {lunarDay, lunarMonth, lunarYear, isLeap};
}

bool isLunarMonthLong(int lunarMonth, int lunarYear, bool isLeap, double timeZone) {
  // Approximate a solar date in this lunar month, then test whether days=30
  (void)isLeap;
  // Convert 29th and 30th day to check if day 30 exists in this lunar month
  // Find Gregorian date of lunar month start
  int k = static_cast<int>((jdFromGregorian(1, 1, lunarYear) - 2415021.076998695) / 29.530588853) + (lunarMonth - 1);
  int mStart = newMoonDay(k, timeZone);
  int nextStart = newMoonDay(k + 1, timeZone);
  return (nextStart - mStart) >= 30;
}

// ── Gregorian helpers (timezone-free) ──────────────────────────────────────

int gregorianDaysInMonth(int month, int year) {
  switch (month) {
    case 2:
      return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    default:
      return 31;
  }
}

int gregorianWeekday(int day, int month, int year) {
  const int jdn = static_cast<int>(jdFromGregorian(day, month, year));
  // JDN 0 is a Monday, so JDN+1 makes 0 = Sunday.
  return (jdn + 1) % 7;
}

void shiftCivilMinutes(int& year, int& month, int& day, int& hour, int& minute, long deltaMinutes) {
  int totalMinutes = hour * 60 + minute + static_cast<int>(deltaMinutes);
  int dayShift = totalMinutes / 1440;
  int remainder = totalMinutes % 1440;
  if (remainder < 0) {
    remainder += 1440;
    dayShift -= 1;
  }
  hour = remainder / 60;
  minute = remainder % 60;

  while (dayShift > 0) {
    day += 1;
    if (day > gregorianDaysInMonth(month, year)) {
      day = 1;
      month += 1;
      if (month > 12) {
        month = 1;
        year += 1;
      }
    }
    dayShift -= 1;
  }
  while (dayShift < 0) {
    day -= 1;
    if (day < 1) {
      month -= 1;
      if (month < 1) {
        month = 12;
        year -= 1;
      }
      day = gregorianDaysInMonth(month, year);
    }
    dayShift += 1;
  }
}

// ── Can-Chi (Sexagenary cycle) ───────────────────────────────────────────────

static constexpr const char* CAN[10] = {"Giáp", "Ất", "Bính", "Đinh", "Mậu", "Kỷ", "Canh", "Tân", "Nhâm", "Quý"};

static constexpr const char* CHI[12] = {"Tý",  "Sửu", "Dần",  "Mão", "Thìn", "Tỵ",
                                        "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};

CanChi getCanChiYear(int lunarYear) {
  int canIdx = (lunarYear + 6) % 10;
  if (canIdx < 0) canIdx += 10;
  int chiIdx = (lunarYear + 8) % 12;
  if (chiIdx < 0) chiIdx += 12;
  return {CAN[canIdx], CHI[chiIdx]};
}

CanChi getCanChiMonth(int lunarMonth, int lunarYear) {
  // Chi của tháng 1 luôn là Dần (2), tháng 2 là Mão (3)...
  int chiIdx = (lunarMonth + 1) % 12;
  if (chiIdx < 0) chiIdx += 12;

  // Can của tháng theo Ngũ hổ độn
  int canYearIdx = (lunarYear + 6) % 10;
  if (canYearIdx < 0) canYearIdx += 10;
  int canIdx = (canYearIdx * 2 + lunarMonth + 1) % 10;
  if (canIdx < 0) canIdx += 10;

  return {CAN[canIdx], CHI[chiIdx]};
}

CanChi getCanChiDay(int day, int month, int year) {
  int jd = static_cast<int>(jdFromGregorian(day, month, year));
  int canIdx = (jd + 9) % 10;
  if (canIdx < 0) canIdx += 10;
  int chiIdx = (jd + 1) % 12;
  if (chiIdx < 0) chiIdx += 12;
  return {CAN[canIdx], CHI[chiIdx]};
}

void formatCanChi(const CanChi& cc, char* buf, size_t bufSize) {
  if (!buf || bufSize == 0) return;
  std::snprintf(buf, bufSize, "%s %s", cc.can, cc.chi);
}

// ── 24 Solar Terms (Tiết khí) ───────────────────────────────────────────────

static constexpr const char* SOLAR_TERMS[24] = {
    "Xuân Phân",    // 0: 0°
    "Thanh Minh",   // 1: 15°
    "Cốc Vũ",       // 2: 30°
    "Lập Hạ",       // 3: 45°
    "Tiểu Mãn",     // 4: 60°
    "Mang Chủng",   // 5: 75°
    "Hạ Chí",       // 6: 90°
    "Tiểu Thử",     // 7: 105°
    "Đại Thử",      // 8: 120°
    "Lập Thu",      // 9: 135°
    "Xử Thử",       // 10: 150°
    "Bạch Lộ",      // 11: 165°
    "Thu Phân",     // 12: 180°
    "Hàn Lộ",       // 13: 195°
    "Sương Giáng",  // 14: 210°
    "Lập Đông",     // 15: 225°
    "Tiểu Tuyết",   // 16: 240°
    "Đại Tuyết",    // 17: 255°
    "Đông Chí",     // 18: 270°
    "Tiểu Hàn",     // 19: 285°
    "Đại Hàn",      // 20: 300°
    "Lập Xuân",     // 21: 315°
    "Vũ Thủy",      // 22: 330°
    "Kinh Trập"     // 23: 345°
};

const char* getSolarTermName(int day, int month, int year, double timeZone) {
  double jdn = jdFromGregorian(day, month, year);
  double sunDeg = sunLongitudeDeg(jdn - 0.5 - timeZone / 24.0);
  int termIdx = static_cast<int>(sunDeg / 15.0);
  if (termIdx < 0) termIdx = 0;
  if (termIdx > 23) termIdx = 23;
  return SOLAR_TERMS[termIdx];
}

// ── Vietnamese Holidays (Ngày lễ truyền thống & quốc gia) ───────────────────

const char* getVietnameseHoliday(int solarDay, int solarMonth, const LunarDate& lunar) {
  // 1. Âm lịch: Tết Nguyên Đán & các ngày lễ lớn
  if (lunar.month == 1) {
    if (lunar.day == 1) return "Mùng 1 Tết Nguyên Đán";
    if (lunar.day == 2) return "Mùng 2 Tết";
    if (lunar.day == 3) return "Mùng 3 Tết";
    if (lunar.day == 15) return "Tết Nguyên Tiêu";
  } else if (lunar.month == 3 && lunar.day == 10) {
    return "Giỗ Tổ Hùng Vương";
  } else if (lunar.month == 4 && lunar.day == 15) {
    return "Lễ Phật Đản";
  } else if (lunar.month == 5 && lunar.day == 5) {
    return "Tết Đoan Ngọ";
  } else if (lunar.month == 7 && lunar.day == 15) {
    return "Lễ Vu Lan";
  } else if (lunar.month == 8 && lunar.day == 15) {
    return "Tết Trung Thu";
  } else if (lunar.month == 9 && lunar.day == 9) {
    return "Tết Trùng Cửu";
  } else if (lunar.month == 12) {
    if (lunar.day == 23) return "Ông Táo chầu trời";
    if (lunar.day >= 29) return "Đêm Giao Thừa";
  }

  // 2. Dương lịch: Các ngày lễ quốc gia
  if (solarMonth == 1 && solarDay == 1) return "Tết Dương Lịch";
  if (solarMonth == 2 && solarDay == 14) return "Lễ Tình Nhân";
  if (solarMonth == 3 && solarDay == 8) return "Quốc tế Phụ nữ";
  if (solarMonth == 4 && solarDay == 30) return "Giải phóng miền Nam";
  if (solarMonth == 5 && solarDay == 1) return "Quốc tế Lao động";
  if (solarMonth == 6 && solarDay == 1) return "Quốc tế Thiếu nhi";
  if (solarMonth == 9 && solarDay == 2) return "Quốc khánh";
  if (solarMonth == 10 && solarDay == 20) return "Phụ nữ Việt Nam";
  if (solarMonth == 11 && solarDay == 20) return "Nhà giáo Việt Nam";
  if (solarMonth == 12 && (solarDay == 24 || solarDay == 25)) return "Lễ Giáng sinh";

  // 3. Rằm & Mùng một thường lệ
  if (lunar.day == 15) return "Ngày Rằm";
  if (lunar.day == 1) return "Mùng Một";

  return nullptr;
}
