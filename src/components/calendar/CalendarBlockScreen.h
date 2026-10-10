#pragma once

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
// the caller owns the display refresh. Returns false when there is no usable
// date to print, in which case nothing was drawn.
bool draw(const GfxRenderer& renderer);

}  // namespace CalendarBlockScreen
