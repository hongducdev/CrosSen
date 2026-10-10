#pragma once

#include <cstdint>

class GfxRenderer;

// Tear-off calendar ("lịch bloc") sleep screen.
//
// Draws the day the device went to sleep as a printed Vietnamese block calendar
// page: the solar day in oversized digits, the month banner, the weekday, the
// lunar date with its can-chi, the day's auspicious hours, a holiday line when
// the day has one, and a book quote filling the masthead of the page.
//
// The clock is deliberately absent: a sleep screen is painted once and then
// left on the panel for hours, so a time readout would be stale the moment the
// device sleeps.
namespace CalendarBlockScreen {

// Paints the page into the current framebuffer but does not refresh the panel;
// the caller owns the display refresh. The caller also resolves the local date,
// so this component never touches the RTC or the UTC offset.
// Returns false when the date cannot be printed, in which case nothing was drawn.
bool draw(const GfxRenderer& renderer, uint16_t year, uint8_t month, uint8_t day);

}  // namespace CalendarBlockScreen
