#pragma once

#include "components/themes/lyra/LyraTheme.h"

class GfxRenderer;

// Quartum: a fixed 2x2 Home screen over the four most recent books.
//
// All four slots are always laid out, so a slot with no book is simply drawn as an
// outline. A cursor moves between the slots with the front buttons (Left/Right step
// one book, Up/Down step a row) and only the highlighted book shows its metadata:
// a title wrapped to two lines above the top row or below the bottom row, plus one
// italic author line rotated alongside the cover. Every other screen keeps Lyra's
// look, because QuartumTheme derives from LyraTheme.
//
// Layout and interaction are ported from the minuta reference fork
// (MIT, Copyright (c) 2025 Dave Allie) — https://github.com/hiitsalice/minuta,
// src/components/themes/minuta/quartum.{h,cpp}.
namespace QuartumMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics v = LyraMetrics::values;
  v.homeTopPadding = 60;
  v.homeCoverHeight = 285;
  v.homeCoverTileHeight = 680;
  v.homeRecentBooksCount = 4;
  v.homeContinueReadingInMenu = false;
  v.homeMenuTopOffset = 0;
  return v;
}

constexpr ThemeMetrics values = makeValues();
}  // namespace QuartumMetrics

class QuartumTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats = nullptr,
                           float progressPercent = -1.0f, const GlobalReadingStats* globalStats = nullptr,
                           const char* currentChapterTitle = nullptr) const override;
};
