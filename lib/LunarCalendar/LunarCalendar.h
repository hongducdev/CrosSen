#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Vietnamese Lunar Calendar (Âm lịch) utility.
 * Based on Ho Ngoc Duc's astronomical algorithm for UTC+7
 * (https://www.informatik.uni-leipzig.de/~duc/amlich/)
 *
 * Provides:
 * - Solar to Lunar date conversion with leap month handling
 * - Can-Chi (Thiên Can, Địa Chi) for Year, Month, Day
 * - 24 Solar terms (Tiết khí)
 * - Major Vietnamese cultural and national holiday detection
 *
 * Designed for embedded systems: zero heap allocation, constant flash tables.
 */

struct LunarDate {
  int day = 1;
  int month = 1;
  int year = 2026;
  bool isLeapMonth = false;
};

struct CanChi {
  const char* can = "";
  const char* chi = "";
};

// Convert a Gregorian date to Vietnamese lunar date.
// timeZone: UTC offset in hours (Vietnam / ICT = 7.0)
LunarDate solarToLunar(int day, int month, int year, double timeZone = 7.0);

// Julian Day Number from Gregorian date
double jdFromGregorian(int day, int month, int year);

// Can-Chi getters (pure static strings, zero allocation)
CanChi getCanChiYear(int lunarYear);
CanChi getCanChiMonth(int lunarMonth, int lunarYear);
CanChi getCanChiDay(int day, int month, int year);

// Format Can-Chi into a destination buffer, e.g. "Bính Ngọ", "Giáp Thân"
void formatCanChi(const CanChi& cc, char* buf, size_t bufSize);

// Returns the name of the active 24 solar terms (Tiết khí)
const char* getSolarTermName(int day, int month, int year, double timeZone = 7.0);

// Returns a notable Vietnamese holiday if present, or nullptr
const char* getVietnameseHoliday(int solarDay, int solarMonth, const LunarDate& lunar);

// Returns true if the lunar month has 30 days (tháng đủ), false for 29 days (tháng thiếu)
bool isLunarMonthLong(int lunarMonth, int lunarYear, bool isLeap = false, double timeZone = 7.0);

// ── Gregorian helpers (timezone-free, no mktime/localtime) ──────────────────

// Days in a Gregorian month, accounting for leap years.
int gregorianDaysInMonth(int month, int year);

// Day of week for a Gregorian date: 0 = Sunday .. 6 = Saturday.
int gregorianWeekday(int day, int month, int year);

// Shift a civil date/time by a signed number of minutes, rolling the date over
// local midnight. Used to turn the RTC's UTC reading into local wall-clock time
// without depending on the platform's timezone database.
void shiftCivilMinutes(int& year, int& month, int& day, int& hour, int& minute, long deltaMinutes);
