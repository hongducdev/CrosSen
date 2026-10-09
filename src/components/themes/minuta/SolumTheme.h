#pragma once

#include "components/themes/lyra/LyraTheme.h"

class GfxRenderer;

// Solum: a one-cover Home screen.
//
// The most recently opened book's cover is centred at the theme's home cover
// height with its title and author underneath, and the Home screen has no menu
// rows — the actions live in the on-demand button menu instead. Every other
// screen keeps Lyra's look, because SolumTheme derives from LyraTheme.
//
// Layout and interaction are ported from the minuta reference fork
// (MIT, Copyright (c) 2025 Dave Allie) — https://github.com/hiitsalice/minuta,
// src/components/themes/minuta/solum.{h,cpp}.
namespace SolumMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics v = LyraMetrics::values;
  v.homeTopPadding = 60;
  v.homeCoverHeight = 600;
  v.homeCoverTileHeight = 680;
  v.homeRecentBooksCount = 1;
  v.homeContinueReadingInMenu = false;
  v.homeMenuTopOffset = 0;
  return v;
}

constexpr ThemeMetrics values = makeValues();
}  // namespace SolumMetrics

class SolumTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats = nullptr,
                           float progressPercent = -1.0f, const GlobalReadingStats* globalStats = nullptr,
                           const char* currentChapterTitle = nullptr) const override;

  // Cover box for the given content rect: centred, as tall as the metrics allow,
  // and at the 2:3 aspect the default Home thumbnail is generated with. Shared
  // with the Home activity so touch targets match the painted cover exactly.
  static Rect coverRectFor(const GfxRenderer& renderer, const Rect& rect);
};
