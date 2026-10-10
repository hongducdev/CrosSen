#include "CalendarBlockScreen.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <LunarCalendar.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CalendarAlmanac.h"
#include "CrossPointSettings.h"
#include "fontIds.h"
#include "images/CalendarDigits.h"

namespace CalendarBlockScreen {
namespace {

// ---------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------

struct BookQuote {
  const char* text;
  const char* author;
};

// Short enough to wrap into at most three lines at UI_12 on a portrait page.
constexpr BookQuote kQuotes[] = {
    {"Không có người bạn nào trung thành bằng một cuốn sách.", "Ernest Hemingway"},
    {"Đọc sách, ta sống cả ngàn cuộc đời trước khi nhắm mắt.", "George R.R. Martin"},
    {"Tôi luôn hình dung Thiên đường là một thư viện.", "Jorge Luis Borges"},
    {"Sách là tấm gương: ta chỉ thấy điều mình đã có trong đó.", "Carlos Ruiz Zafón"},
    {"Sách là thứ phép thuật gọn nhẹ mang theo được.", "Stephen King"},
    {"Càng đọc nhiều, càng thấy mình biết ít.", "Voltaire"},
    {"Một căn phòng không có sách như một cơ thể không có linh hồn.", "Cicero"},
    {"Tôi không thể sống mà thiếu sách.", "Thomas Jefferson"},
    {"Sách là thứ gần nhất với một tôn giáo mà tôi có.", "Neil Gaiman"},
    {"Một cuốn sách phải là chiếc rìu đập vào biển đóng trong ta.", "Franz Kafka"},
    {"Đốt sách không phá nổi văn hoá bằng việc khiến người ta thôi đọc.", "Ray Bradbury"},
    {"Nếu chỉ đọc sách người khác đọc, ta chỉ nghĩ điều người khác nghĩ.", "Haruki Murakami"},
    {"Một kho vàng không bằng một nang chữ.", "Tục ngữ Việt Nam"},
    {"Học một biết mười.", "Tục ngữ Việt Nam"},
};
constexpr size_t kQuoteCount = sizeof(kQuotes) / sizeof(kQuotes[0]);

struct FixedHoliday {
  uint8_t day;
  uint8_t month;
  const char* name;
};

// Solar-calendar observances.
constexpr FixedHoliday kSolarHolidays[] = {
    {1, 1, "Tết Dương lịch"},
    {3, 2, "Thành lập Đảng Cộng sản Việt Nam"},
    {8, 3, "Ngày Quốc tế Phụ nữ"},
    {30, 4, "Ngày Giải phóng miền Nam"},
    {1, 5, "Ngày Quốc tế Lao động"},
    {7, 5, "Chiến thắng Điện Biên Phủ"},
    {19, 5, "Ngày sinh Chủ tịch Hồ Chí Minh"},
    {1, 6, "Ngày Quốc tế Thiếu nhi"},
    {28, 6, "Ngày Gia đình Việt Nam"},
    {27, 7, "Ngày Thương binh - Liệt sĩ"},
    {19, 8, "Cách mạng Tháng Tám"},
    {2, 9, "Quốc khánh"},
    {10, 10, "Ngày Giải phóng Thủ đô"},
    {20, 10, "Ngày Phụ nữ Việt Nam"},
    {20, 11, "Ngày Nhà giáo Việt Nam"},
    {22, 12, "Thành lập Quân đội Nhân dân Việt Nam"},
};

// Lunar-calendar observances.
constexpr FixedHoliday kLunarHolidays[] = {
    {1, 1, "Tết Nguyên Đán"},    {15, 1, "Rằm tháng Giêng"}, {3, 3, "Tết Hàn Thực"}, {10, 3, "Giỗ Tổ Hùng Vương"},
    {15, 4, "Lễ Phật Đản"},      {5, 5, "Tết Đoan Ngọ"},     {15, 7, "Lễ Vu Lan"},   {15, 8, "Tết Trung Thu"},
    {23, 12, "Ông Táo về trời"}, {15, 12, "Rằm tháng Chạp"},
};

// ---------------------------------------------------------------------------
// Layout
//
// A printed block calendar is a stack of bands separated by rules. The masthead
// is anchored to the top of the card and the lunar block plus the quote are
// anchored to the bottom, so the page keeps its shape whatever the page height
// and wherever the weekday and the last day of the month land.
// ---------------------------------------------------------------------------

constexpr int kCardInset = 18;    // card edge to page edge
constexpr int kCardPadding = 20;  // card edge to content
constexpr int kTopPadding = 24;   // card edge to the first band
constexpr int kBannerHeight = 46;
constexpr int kBandGap = 20;       // vertical rhythm between bands
constexpr int kRuleGap = 7;        // the two hairlines of a double rule
constexpr int kWeekdayGap = 14;    // weekday text to the rule under it
constexpr int kHeroGapAbove = 30;  // rule to the day digits
constexpr int kHeroCaptionGap = 8;
constexpr int kLunarLineGap = 7;
constexpr int kStackGap = 20;  // lunar block / holiday band / quote
constexpr int kHolidayBandHeight = 38;
constexpr int kQuoteRuleGap = 16;
constexpr int kQuoteLineGap = 2;
constexpr int kAuthorGap = 6;
constexpr int kFooterBottom = 18;

constexpr int kHeroDigitGap = 10;  // space between two day digits

// The lunar series is only plotted inside this window: an unset RTC can report a
// far-out date, and the page would then print an invented lunar date.
constexpr uint16_t kMinSolarYear = 1900;
constexpr uint16_t kMaxSolarYear = 2100;

void shiftDay(uint16_t& year, uint8_t& month, uint8_t& day, const int delta) {
  if (delta == 0) return;
  if (delta > 0) {
    if (static_cast<int>(day) < gregorianDaysInMonth(month, year)) {
      day++;
      return;
    }
    day = 1;
    if (month == 12) {
      month = 1;
      year++;
    } else {
      month++;
    }
    return;
  }
  if (day > 1) {
    day--;
    return;
  }
  if (month == 1) {
    month = 12;
    year--;
  } else {
    month--;
  }
  day = static_cast<uint8_t>(gregorianDaysInMonth(month, year));
}

// The RTC keeps UTC; the printed page must show the wall-clock day, which rolls
// over at local midnight exactly like HalClock::formatDate().
bool localDate(uint16_t& year, uint8_t& month, uint8_t& day) {
  uint16_t utcYear = 0;
  uint8_t utcMonth = 0;
  uint8_t utcDay = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getDateTime(utcYear, utcMonth, utcDay, hour, minute)) return false;

  uint8_t offsetQuarterHours = SETTINGS.clockUtcOffsetQ;
  if (offsetQuarterHours > 104) offsetQuarterHours = 104;
  const int localMinutes =
      static_cast<int>(hour) * 60 + static_cast<int>(minute) + (static_cast<int>(offsetQuarterHours) - 48) * 15;
  const int dayDelta = localMinutes < 0 ? -1 : (localMinutes >= 1440 ? 1 : 0);

  year = utcYear;
  month = utcMonth;
  day = utcDay;
  shiftDay(year, month, day, dayDelta);
  if (month < 1 || month > 12) return false;
  return day >= 1 && static_cast<int>(day) <= gregorianDaysInMonth(month, year);
}

float configuredTimeZoneHours() {
  uint8_t offsetQuarterHours = SETTINGS.clockUtcOffsetQ;
  if (offsetQuarterHours > 104) offsetQuarterHours = 104;
  return static_cast<float>(static_cast<int>(offsetQuarterHours) - 48) / 4.0f;
}

const char* solarHoliday(const uint8_t month, const uint8_t day) {
  for (const FixedHoliday& holiday : kSolarHolidays) {
    if (holiday.month == month && holiday.day == day) return holiday.name;
  }
  return nullptr;
}

const char* lunarHoliday(const uint8_t lunarMonth, const uint8_t lunarDay) {
  for (const FixedHoliday& holiday : kLunarHolidays) {
    if (holiday.month == lunarMonth && holiday.day == lunarDay) return holiday.name;
  }
  return nullptr;
}

const char* weekdayName(const uint8_t weekdayIndex) {
  static constexpr StrId kWeekdays[7] = {StrId::STR_WEEKDAY_MON, StrId::STR_WEEKDAY_TUE, StrId::STR_WEEKDAY_WED,
                                         StrId::STR_WEEKDAY_THU, StrId::STR_WEEKDAY_FRI, StrId::STR_WEEKDAY_SAT,
                                         StrId::STR_WEEKDAY_SUN};
  return I18N.get(kWeekdays[weekdayIndex % 7]);
}

const char* solarMonthName(const uint8_t month) {
  static constexpr StrId kMonthNames[12] = {StrId::STR_MONTH_JAN, StrId::STR_MONTH_FEB, StrId::STR_MONTH_MAR,
                                            StrId::STR_MONTH_APR, StrId::STR_MONTH_MAY, StrId::STR_MONTH_JUN,
                                            StrId::STR_MONTH_JUL, StrId::STR_MONTH_AUG, StrId::STR_MONTH_SEP,
                                            StrId::STR_MONTH_OCT, StrId::STR_MONTH_NOV, StrId::STR_MONTH_DEC};
  return I18N.get(kMonthNames[(month - 1) % 12]);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void drawRule(const GfxRenderer& renderer, const int x, const int y, const int width) {
  renderer.drawRect(x, y, width, 1, true);
}

// Two hairlines, like the printed rules separating the bands of a calendar page.
void drawDoubleRule(const GfxRenderer& renderer, const int x, const int y, const int width) {
  drawRule(renderer, x, y, width);
  drawRule(renderer, x, y + kRuleGap, width);
}

void drawCentered(const GfxRenderer& renderer, const int fontId, const int centerX, const int y, const char* text,
                  const bool black, const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const int width = renderer.getTextWidth(fontId, text, style);
  renderer.drawText(fontId, centerX - width / 2, y, text, black, style);
}

// Centres one line inside `maxWidth`, ellipsising it when even the smallest UI
// font is too wide. Nothing drawn through this helper can leave the card.
void drawCenteredFitted(const GfxRenderer& renderer, const int fontId, const int centerX, const int y,
                        const int maxWidth, const char* text, const bool black = true,
                        const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const std::string fitted = renderer.truncatedText(fontId, text, maxWidth, style);
  drawCentered(renderer, fontId, centerX, y, fitted.c_str(), black, style);
}

void drawHeroDay(const GfxRenderer& renderer, const int centerX, const int top, const uint8_t day) {
  char digits[3] = {0, 0, 0};
  std::snprintf(digits, sizeof(digits), "%u", static_cast<unsigned>(day));
  const int count = static_cast<int>(std::strlen(digits));

  // Each digit has its own cell width (a '1' is far narrower than a '0'), so the
  // run is measured and centred as a whole instead of centring fixed-width cells.
  int totalWidth = (count > 1 ? (count - 1) * kHeroDigitGap : 0);
  for (int i = 0; i < count; i++) {
    const int digit = digits[i] - '0';
    if (digit >= 0 && digit <= 9) totalWidth += CalendarDigitWidths[digit];
  }

  int x = centerX - totalWidth / 2;
  for (int i = 0; i < count; i++) {
    const int digit = digits[i] - '0';
    if (digit < 0 || digit > 9) continue;
    renderer.drawImage(CalendarDigits[digit], x, top, CalendarDigitWidths[digit], CalendarDigitHeight);
    x += CalendarDigitWidths[digit] + kHeroDigitGap;
  }
}

}  // namespace

bool draw(const GfxRenderer& renderer) {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  if (!localDate(year, month, day)) {
    return false;
  }
  if (year < kMinSolarYear || year > kMaxSolarYear) {
    return false;
  }

  const LunarDate lunar = solarToLunar(day, month, year, configuredTimeZoneHours());
  const int julianDay = static_cast<int>(jdFromGregorian(day, month, year));

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const int centerX = screenWidth / 2;

  renderer.clearScreen();

  const int cardX = kCardInset;
  const int cardY = kCardInset;
  const int cardWidth = screenWidth - kCardInset * 2;
  const int cardHeight = screenHeight - kCardInset * 2;
  const int cardRight = cardX + cardWidth;
  const int cardBottom = cardY + cardHeight;
  const int contentX = cardX + kCardPadding;
  const int contentWidth = cardWidth - kCardPadding * 2;

  renderer.drawRect(cardX, cardY, cardWidth, cardHeight, 1, true);
  renderer.drawRect(cardX + 4, cardY + 4, cardWidth - 8, cardHeight - 8, 1, true);

  // --- Content strings -----------------------------------------------------
  char banner[64];
  std::snprintf(banner, sizeof(banner), "%s  ·  %u", solarMonthName(month), static_cast<unsigned>(year));

  const char* festiveDayName = CalendarAlmanac::lunarDayName(lunar.day);
  const char* lunarMonthName = CalendarAlmanac::lunarMonthName(lunar.month);
  const char* leapSuffix = lunar.isLeapMonth ? tr(STR_CALENDAR_LUNAR_LEAP) : "";
  char lunarTitle[128];
  if (festiveDayName != nullptr) {
    std::snprintf(lunarTitle, sizeof(lunarTitle), "%s: %s  ·  %s%s", tr(STR_CALENDAR_LUNAR), festiveDayName,
                  lunarMonthName, leapSuffix);
  } else {
    std::snprintf(lunarTitle, sizeof(lunarTitle), "%s: %d %s%s", tr(STR_CALENDAR_LUNAR), lunar.day, lunarMonthName,
                  leapSuffix);
  }

  char yearCanChi[24];
  formatCanChi(getCanChiYear(lunar.year), yearCanChi, sizeof(yearCanChi));
  char monthCanChi[24];
  formatCanChi(getCanChiMonth(lunar.month, lunar.year), monthCanChi, sizeof(monthCanChi));
  char dayCanChi[24];
  formatCanChi(getCanChiDay(day, month, year), dayCanChi, sizeof(dayCanChi));

  char pillarYear[64];
  std::snprintf(pillarYear, sizeof(pillarYear), "%s %s", tr(STR_CALENDAR_YEAR), yearCanChi);
  char pillarMonthDay[96];
  std::snprintf(pillarMonthDay, sizeof(pillarMonthDay), "%s %s  ·  %s %s", tr(STR_CALENDAR_MONTH), monthCanChi,
                tr(STR_CALENDAR_DAY), dayCanChi);

  // The day pillar and the auspicious hours both follow the civil day's branch.
  int dayBranch = (julianDay + 1) % 12;
  if (dayBranch < 0) dayBranch += 12;
  const uint8_t* auspicious = CalendarAlmanac::auspiciousHours(dayBranch);
  char hoursLine[96];
  std::snprintf(hoursLine, sizeof(hoursLine), "%s: %s, %s, %s, %s, %s, %s", tr(STR_CALENDAR_AUSPICIOUS_HOURS),
                CalendarAlmanac::branchName(auspicious[0]), CalendarAlmanac::branchName(auspicious[1]),
                CalendarAlmanac::branchName(auspicious[2]), CalendarAlmanac::branchName(auspicious[3]),
                CalendarAlmanac::branchName(auspicious[4]), CalendarAlmanac::branchName(auspicious[5]));

  const char* holiday = lunarHoliday(lunar.month, lunar.day);
  if (holiday == nullptr) {
    holiday = solarHoliday(month, day);
  }

  const int quoteCount = static_cast<int>(kQuoteCount);
  const int quoteIndex = ((julianDay % quoteCount) + quoteCount) % quoteCount;
  const BookQuote& quote = kQuotes[quoteIndex];
  const std::vector<std::string> quoteLines = renderer.wrappedText(UI_12_FONT_ID, quote.text, contentWidth, 3);
  const std::vector<std::string> hoursLines = renderer.wrappedText(UI_10_FONT_ID, hoursLine, contentWidth, 2);

  const int titleLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int bodyLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int quoteLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int authorHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int smallHeight = renderer.getLineHeight(SMALL_FONT_ID);

  // --- Masthead, anchored to the card top ---------------------------------
  int y = cardY + kTopPadding;

  renderer.fillRect(contentX, y, contentWidth, kBannerHeight, true);
  drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, y + (kBannerHeight - titleLineHeight) / 2, contentWidth, banner,
                     false, EpdFontFamily::BOLD);
  y += kBannerHeight + kBandGap;

  // gregorianWeekday() counts from Sunday; the page lists Monday first.
  const int weekday = (gregorianWeekday(day, month, year) + 6) % 7;
  drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, y, contentWidth, weekdayName(weekday), true,
                     EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + kWeekdayGap;
  drawDoubleRule(renderer, contentX, y, contentWidth);

  y += kRuleGap + kHeroGapAbove;
  drawHeroDay(renderer, centerX, y, day);
  y += CalendarDigitHeight + kHeroCaptionGap;
  drawCentered(renderer, SMALL_FONT_ID, centerX, y, tr(STR_CALENDAR_SOLAR), true);
  const int mastheadBottom = y + smallHeight;

  // --- Footer, anchored to the card bottom --------------------------------
  // Sizes are known before drawing, so the block can be positioned as a whole
  // and the leftover space becomes one deliberate gap under the masthead.
  int footerHeight = kRuleGap + 1 + kBandGap;
  footerHeight += titleLineHeight + kLunarLineGap;
  footerHeight += (bodyLineHeight + kLunarLineGap) * 2;
  footerHeight += static_cast<int>(hoursLines.size()) * (bodyLineHeight + kLunarLineGap);
  footerHeight += kStackGap;
  if (holiday != nullptr) {
    footerHeight += kHolidayBandHeight + kStackGap;
  }
  footerHeight += kQuoteRuleGap + 1 + kQuoteRuleGap;
  footerHeight += static_cast<int>(quoteLines.size()) * (quoteLineHeight + kQuoteLineGap);
  footerHeight += kAuthorGap + authorHeight;

  const int minimumFooterTop = mastheadBottom + kBandGap;
  int footerTop = cardBottom - kFooterBottom - footerHeight;
  if (footerTop < minimumFooterTop) footerTop = minimumFooterTop;

  int fy = footerTop;
  drawDoubleRule(renderer, contentX, fy, contentWidth);
  fy += kRuleGap + 1 + kBandGap;

  drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, fy, contentWidth, lunarTitle, true, EpdFontFamily::BOLD);
  fy += titleLineHeight + kLunarLineGap;
  drawCenteredFitted(renderer, UI_10_FONT_ID, centerX, fy, contentWidth, pillarYear);
  fy += bodyLineHeight + kLunarLineGap;
  drawCenteredFitted(renderer, UI_10_FONT_ID, centerX, fy, contentWidth, pillarMonthDay);
  fy += bodyLineHeight + kLunarLineGap;
  for (const std::string& line : hoursLines) {
    drawCentered(renderer, UI_10_FONT_ID, centerX, fy, line.c_str(), true);
    fy += bodyLineHeight + kLunarLineGap;
  }
  fy += kStackGap - kLunarLineGap;

  if (holiday != nullptr) {
    renderer.drawRect(contentX, fy, contentWidth, kHolidayBandHeight, 1, true);
    drawCenteredFitted(renderer, UI_12_FONT_ID, centerX,
                       fy + (kHolidayBandHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, contentWidth - 16,
                       holiday, true, EpdFontFamily::BOLD);
    fy += kHolidayBandHeight + kStackGap;
  }

  drawRule(renderer, contentX, fy, contentWidth);
  fy += kQuoteRuleGap;
  for (const std::string& line : quoteLines) {
    drawCentered(renderer, UI_12_FONT_ID, centerX, fy, line.c_str(), true);
    fy += quoteLineHeight + kQuoteLineGap;
  }
  fy += kAuthorGap;

  char author[64];
  std::snprintf(author, sizeof(author), "— %s", quote.author);
  const std::string fittedAuthor = renderer.truncatedText(UI_10_FONT_ID, author, contentWidth, EpdFontFamily::ITALIC);
  const int authorWidth = renderer.getTextWidth(UI_10_FONT_ID, fittedAuthor.c_str(), EpdFontFamily::ITALIC);
  renderer.drawText(UI_10_FONT_ID, cardRight - kCardPadding - authorWidth, fy, fittedAuthor.c_str(), true,
                    EpdFontFamily::ITALIC);

  return true;
}

}  // namespace CalendarBlockScreen
