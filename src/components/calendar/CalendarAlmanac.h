#pragma once

#include <cstdint>

// Content the tear-off calendar page needs but the shared lunar engine
// (lib/LunarCalendar) deliberately does not carry: Vietnamese month names, the
// two festive day names, and the six auspicious hours of a day branch.
//
// This lives beside the screen instead of inside lib/LunarCalendar so that the
// engine stays byte-identical to the copy the lunar sleep screen uses: one
// engine, two consumers.
namespace CalendarAlmanac {

// "tháng Giêng", "tháng Hai", ... "tháng Chạp"; "" when out of range.
const char* lunarMonthName(int lunarMonth);

// "Mùng một" on day 1, "Rằm" on day 15, nullptr otherwise.
const char* lunarDayName(int lunarDay);

// "Tý", "Sửu", ... "Hợi" for a branch index; "" when out of range. The engine
// carries the same names inside its can-chi strings, but exposing them here
// keeps lib/LunarCalendar byte-identical to the lunar sleep screen's copy.
const char* branchName(int branch);

// The six auspicious hours ("giờ hoàng đạo") of a day whose branch index is
// `dayBranch` (0 = Tý .. 11 = Hợi), as branch indices into the same cycle.
// Out-of-range input wraps into 0..11 instead of reading past the table.
const uint8_t* auspiciousHours(int dayBranch);

}  // namespace CalendarAlmanac
