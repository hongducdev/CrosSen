#include "CalendarBlockScreen.h"

#include <GfxRenderer.h>
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
constexpr int kBannerHeight = 46;
constexpr int kRuleGap = 7;  // the two hairlines of a double rule
constexpr int kHolidayBandHeight = 38;
constexpr int kHeroDigitGap = 10;  // space between two day digits

// The lunar series is only plotted inside this window: an unset RTC can report a
// far-out date, and the page would then print an invented lunar date.
constexpr uint16_t kMinSolarYear = 1900;
constexpr uint16_t kMaxSolarYear = 2100;

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
  static constexpr const char* kWeekdays[7] = {"Thứ Hai", "Thứ Ba",  "Thứ Tư",  "Thứ Năm",
                                               "Thứ Sáu", "Thứ Bảy", "Chủ Nhật"};
  return kWeekdays[weekdayIndex % 7];
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
  char digits[8] = {0};
  std::snprintf(digits, sizeof(digits), "%u", static_cast<unsigned>(day));
  const int count = static_cast<int>(std::strlen(digits));

  // Each digit has its own cell width (a '1' is far narrower than a '0'), so the
  // run is measured and centred as a whole instead of centring fixed-width cells.
  int totalWidth = (count > 1 ? (count - 1) * kHeroDigitGap : 0);
  for (int i = 0; i < count; i++) {
    const int digit = digits[i] - '0';
    if (digit >= 0 && digit <= 9) totalWidth += CalendarDigitWidths[digit];
  }

  // Draw pixel-by-pixel via renderer.drawPixel() so that GfxRenderer::rotateCoordinates()
  // rotates the glyphs to match the screen's logical orientation (portrait/landscape).
  // renderer.drawImage() lacks bit-rotation (TODO in GfxRenderer.cpp) and draws raw
  // panel-oriented rows, which left the digits rotated 90 degrees sideways in portrait mode.
  int startX = centerX - totalWidth / 2;
  for (int i = 0; i < count; i++) {
    const int digit = digits[i] - '0';
    if (digit < 0 || digit > 9) continue;
    const int w = CalendarDigitWidths[digit];
    const int stride = (w + 7) / 8;
    const uint8_t* bmp = CalendarDigits[digit];
    for (int r = 0; r < CalendarDigitHeight; r++) {
      const int py = top + r;
      for (int c = 0; c < w; c++) {
        const uint8_t byte = bmp[r * stride + (c / 8)];
        const bool isWhite = (byte >> (7 - (c % 8))) & 1;
        if (!isWhite) {
          renderer.drawPixel(startX + c, py, true);
        }
      }
    }
    startX += w + kHeroDigitGap;
  }
}

}  // namespace

bool draw(const GfxRenderer& renderer, const uint16_t year, const uint8_t month, const uint8_t day) {
  // The caller hands over a local date; validate it before printing anything, so
  // an unset RTC falls back to the default screen instead of inventing a page.
  if (year < kMinSolarYear || year > kMaxSolarYear) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || static_cast<int>(day) > gregorianDaysInMonth(month, year)) return false;

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

  // --- Content strings (100% tiếng Việt cho lịch bloc) --------------------
  char banner[64];
  std::snprintf(banner, sizeof(banner), "Tháng %u  ·  %u", static_cast<unsigned>(month), static_cast<unsigned>(year));

  const char* leapSuffix = lunar.isLeapMonth ? " (nhuận)" : "";
  char lunarTitle[128];
  std::snprintf(lunarTitle, sizeof(lunarTitle), "Ngày %u  ·  Tháng %u%s", static_cast<unsigned>(lunar.day),
                static_cast<unsigned>(lunar.month), leapSuffix);

  char yearCanChi[24];
  formatCanChi(getCanChiYear(lunar.year), yearCanChi, sizeof(yearCanChi));
  char monthCanChi[24];
  formatCanChi(getCanChiMonth(lunar.month, lunar.year), monthCanChi, sizeof(monthCanChi));
  char dayCanChi[24];
  formatCanChi(getCanChiDay(day, month, year), dayCanChi, sizeof(dayCanChi));

  char pillarYear[64];
  std::snprintf(pillarYear, sizeof(pillarYear), "Năm %s", yearCanChi);
  char pillarMonthDay[96];
  std::snprintf(pillarMonthDay, sizeof(pillarMonthDay), "Tháng %s  ·  Ngày %s", monthCanChi, dayCanChi);

  // The day pillar and the auspicious hours both follow the civil day's branch.
  int dayBranch = (julianDay + 1) % 12;
  if (dayBranch < 0) dayBranch += 12;
  const uint8_t* auspicious = CalendarAlmanac::auspiciousHours(dayBranch);
  char hoursLine[96];
  std::snprintf(hoursLine, sizeof(hoursLine), "Giờ hoàng đạo: %s, %s, %s, %s, %s, %s",
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

  // --- Layout height calculations & dynamic vertical spacing --------------
  const int bannerH = kBannerHeight;
  const int weekdayH = titleLineHeight;
  const int doubleRuleH = kRuleGap + 1;
  const int heroH = CalendarDigitHeight;

  constexpr int lunarLineGap = 9;
  const int hoursLinesCount = static_cast<int>(hoursLines.size());
  const int almanacH = titleLineHeight + (bodyLineHeight * 2) + (hoursLinesCount * bodyLineHeight) +
                       (2 + hoursLinesCount) * lunarLineGap;

  const bool hasHoliday = (holiday != nullptr);
  const int holidayH = hasHoliday ? kHolidayBandHeight : 0;

  constexpr int quoteLineGap = 5;
  constexpr int authorGap = 8;
  const int quoteLinesCount = static_cast<int>(quoteLines.size());
  const int quoteH = (quoteLinesCount * quoteLineHeight) +
                     ((quoteLinesCount > 1 ? quoteLinesCount - 1 : 0) * quoteLineGap) + authorGap + authorHeight;

  constexpr int singleRuleH = 1;

  const int totalContentH = bannerH + weekdayH + (doubleRuleH * 2) + heroH + almanacH + holidayH + singleRuleH + quoteH;
  const int totalSlack = cardHeight - totalContentH;

  int weights[10];
  int numGaps = 0;
  if (hasHoliday) {
    numGaps = 10;
    weights[0] = 24;  // gapTop
    weights[1] = 20;  // gapBannerToWeekday
    weights[2] = 16;  // gapWeekdayToRule1
    weights[3] = 34;  // gapRule1ToHero
    weights[4] = 34;  // gapHeroToRule2
    weights[5] = 28;  // gapRule2ToAlmanac
    weights[6] = 26;  // gapAlmanacToHoliday
    weights[7] = 24;  // gapHolidayToRule3
    weights[8] = 26;  // gapRule3ToQuote
    weights[9] = 28;  // gapBottom
  } else {
    numGaps = 9;
    weights[0] = 24;  // gapTop
    weights[1] = 20;  // gapBannerToWeekday
    weights[2] = 16;  // gapWeekdayToRule1
    weights[3] = 36;  // gapRule1ToHero
    weights[4] = 36;  // gapHeroToRule2
    weights[5] = 30;  // gapRule2ToAlmanac
    weights[6] = 34;  // gapAlmanacToRule3
    weights[7] = 28;  // gapRule3ToQuote
    weights[8] = 30;  // gapBottom
  }

  int weightSum = 0;
  for (int i = 0; i < numGaps; ++i) weightSum += weights[i];

  int gaps[10];
  int allocatedSlack = 0;
  for (int i = 0; i < numGaps; ++i) {
    gaps[i] = (totalSlack * weights[i]) / weightSum;
    allocatedSlack += gaps[i];
  }
  int remainder = totalSlack - allocatedSlack;
  for (int i = 3; remainder > 0 && i < numGaps - 1; ++i) {
    gaps[i]++;
    remainder--;
  }

  // --- Rendering sequence -------------------------------------------------
  int y = cardY + gaps[0];

  // 1. Banner
  renderer.fillRect(contentX, y, contentWidth, bannerH, true);
  drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, y + (bannerH - titleLineHeight) / 2, contentWidth, banner, false,
                     EpdFontFamily::BOLD);
  y += bannerH + gaps[1];

  // 2. Weekday
  const int weekday = (gregorianWeekday(day, month, year) + 6) % 7;
  drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, y, contentWidth, weekdayName(weekday), true,
                     EpdFontFamily::BOLD);
  y += weekdayH + gaps[2];

  // 3. Double rule 1
  drawDoubleRule(renderer, contentX, y, contentWidth);
  y += doubleRuleH + gaps[3];

  // 4. Hero Day digits (160px)
  drawHeroDay(renderer, centerX, y, day);
  y += heroH + gaps[4];

  // 5. Double rule 2
  drawDoubleRule(renderer, contentX, y, contentWidth);
  y += doubleRuleH + gaps[5];

  // 6. Almanac block
  drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, y, contentWidth, lunarTitle, true, EpdFontFamily::BOLD);
  y += titleLineHeight + lunarLineGap;
  drawCenteredFitted(renderer, UI_10_FONT_ID, centerX, y, contentWidth, pillarYear);
  y += bodyLineHeight + lunarLineGap;
  drawCenteredFitted(renderer, UI_10_FONT_ID, centerX, y, contentWidth, pillarMonthDay);
  y += bodyLineHeight + lunarLineGap;
  for (size_t i = 0; i < hoursLines.size(); ++i) {
    drawCentered(renderer, UI_10_FONT_ID, centerX, y, hoursLines[i].c_str(), true);
    if (i + 1 < hoursLines.size()) {
      y += bodyLineHeight + lunarLineGap;
    } else {
      y += bodyLineHeight;
    }
  }

  // 7. Holiday (if present)
  if (hasHoliday) {
    y += gaps[6];
    renderer.drawRect(contentX, y, contentWidth, holidayH, 1, true);
    drawCenteredFitted(renderer, UI_12_FONT_ID, centerX, y + (holidayH - titleLineHeight) / 2, contentWidth - 16,
                       holiday, true, EpdFontFamily::BOLD);
    y += holidayH + gaps[7];
  } else {
    y += gaps[6];
  }

  // 8. Single Rule
  drawRule(renderer, contentX, y, contentWidth);
  y += singleRuleH + (hasHoliday ? gaps[8] : gaps[7]);

  // 9. Quote lines
  for (size_t i = 0; i < quoteLines.size(); ++i) {
    drawCentered(renderer, UI_12_FONT_ID, centerX, y, quoteLines[i].c_str(), true);
    if (i + 1 < quoteLines.size()) {
      y += quoteLineHeight + quoteLineGap;
    } else {
      y += quoteLineHeight;
    }
  }
  y += authorGap;

  // 10. Author
  char author[64];
  std::snprintf(author, sizeof(author), "— %s", quote.author);
  const std::string fittedAuthor = renderer.truncatedText(UI_10_FONT_ID, author, contentWidth, EpdFontFamily::ITALIC);
  const int authorWidth = renderer.getTextWidth(UI_10_FONT_ID, fittedAuthor.c_str(), EpdFontFamily::ITALIC);
  renderer.drawText(UI_10_FONT_ID, cardRight - kCardPadding - authorWidth, y, fittedAuthor.c_str(), true,
                    EpdFontFamily::ITALIC);

  return true;
}

}  // namespace CalendarBlockScreen
