#include "components/themes/minuta/QuartumTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <algorithm>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"
#include "components/themes/minuta/MinutaTextLayout.h"
#include "components/themes/minuta/QuartumGridNav.h"
#include "fontIds.h"

namespace {
constexpr int kColumnGap = 18;
constexpr int kRowGap = 18;
constexpr int kTextTopGap = 4;     // content rect top -> reserved title band
constexpr int kTitleCoverGap = 9;  // title block <-> cover edge
constexpr int kTitleLineGap = 4;   // gap between the two title lines
constexpr int kMinSidePadding = 40;
constexpr int kCoverIconSize = 32;

// CrossInk's Home thumbnails are generated at this ratio by
// UITheme::getCoverThumbPath(path, height) and LyraTheme sizes its cover the same
// way, so painted covers match the cached bitmaps with no scaling or letterboxing.
constexpr int kCoverAspectWidth = 2;
constexpr int kCoverAspectHeight = 3;

const int kTitleFontId = UI_12_FONT_ID;
const int kAuthorFontId = LEXENDDECA_10_FONT_ID;

struct QuartumLayout {
  int slotWidth = 0;
  int slotHeight = 0;
  int gridX = 0;
  int topCoverY = 0;
  int bottomCoverY = 0;
};

// Two-line title blocks are reserved above the top row and below the bottom row, so
// a book whose title wraps never collides with the header or the button hints. The
// slot height then shrinks to whatever vertical room is left, capped by the metric
// that also drives thumbnail generation.
QuartumLayout computeLayout(const GfxRenderer& renderer, const Rect& rect) {
  QuartumLayout layout;

  const int titleLineHeight = renderer.getLineHeight(kTitleFontId);
  const int maxTitleBlock = 2 * titleLineHeight + kTitleLineGap;
  const int topBandHeight = kTextTopGap + maxTitleBlock + kTitleCoverGap;

  const int safeBottom = renderer.getScreenHeight() - UITheme::getInstance().getMetrics().buttonHintsHeight;
  const int availableForCovers = safeBottom - rect.y - topBandHeight - kRowGap - kTitleCoverGap - maxTitleBlock;
  layout.slotHeight = std::clamp(availableForCovers / 2, 0, QuartumMetrics::values.homeCoverHeight);
  layout.slotWidth = layout.slotHeight * kCoverAspectWidth / kCoverAspectHeight;

  const int gridWidth = 2 * layout.slotWidth + kColumnGap;
  const int maxGridWidth = std::max(0, rect.width - 2 * kMinSidePadding);
  if (gridWidth > maxGridWidth && maxGridWidth > 0) {
    // A narrower panel: shrink to fit rather than run into the bezel.
    layout.slotWidth = std::max(1, (maxGridWidth - kColumnGap) / 2);
    layout.slotHeight = std::min(layout.slotHeight, layout.slotWidth * kCoverAspectHeight / kCoverAspectWidth);
  }

  const int finalGridWidth = 2 * layout.slotWidth + kColumnGap;
  layout.gridX = rect.x + (rect.width - finalGridWidth) / 2;
  layout.topCoverY = rect.y + kTextTopGap + maxTitleBlock + kTitleCoverGap;
  layout.bottomCoverY = layout.topCoverY + layout.slotHeight + kRowGap;
  return layout;
}

int slotXFor(const QuartumLayout& layout, const int index) {
  return layout.gridX + (index % QuartumGridNav::kColumns) * (layout.slotWidth + kColumnGap);
}

int slotYFor(const QuartumLayout& layout, const int index) {
  return index < QuartumGridNav::kColumns ? layout.topCoverY : layout.bottomCoverY;
}

bool paintCover(GfxRenderer& renderer, const RecentBook& book, const Rect& slot, const int thumbHeight) {
  if (book.coverBmpPath.empty()) {
    return false;
  }
  const std::string coverBmpPath = UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeight);
  if (coverBmpPath.empty()) {
    return false;
  }

  HalFile file;
  if (!Storage.openFileForRead("HOME", coverBmpPath, file)) {
    return false;
  }

  bool painted = false;
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() == BmpReaderError::Ok) {
    painted = renderer.drawBitmap(bitmap, slot.x, slot.y, slot.width, slot.height);
  }
  file.close();
  return painted;
}

void drawMissingCover(GfxRenderer& renderer, const Rect& slot) {
  renderer.fillRect(slot.x, slot.y + slot.height / 3, slot.width, 2 * slot.height / 3, true);
  renderer.drawIcon(CoverIcon, slot.x + slot.width / 2 - kCoverIconSize / 2,
                    slot.y + slot.height / 2 - kCoverIconSize / 2, kCoverIconSize);
}
}  // namespace

void QuartumTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                       int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                       bool& bufferRestored, const std::function<bool()>& storeCoverBuffer,
                                       const BookReadingStats* /*stats*/, float /*progressPercent*/,
                                       const GlobalReadingStats* /*globalStats*/,
                                       const char* /*currentChapterTitle*/) const {
  (void)bufferRestored;

  const int bookCount = std::min(static_cast<int>(recentBooks.size()), QuartumMetrics::values.homeRecentBooksCount);
  const QuartumLayout layout = computeLayout(renderer, rect);

  if (!coverRendered) {
    for (int i = 0; i < QuartumGridNav::kSlots; ++i) {
      const Rect slot{slotXFor(layout, i), slotYFor(layout, i), layout.slotWidth, layout.slotHeight};
      if (i >= bookCount) {
        // No book in this slot: an empty outline keeps the grid readable.
        renderer.drawRect(slot.x, slot.y, slot.width, slot.height, true);
        continue;
      }
      const bool painted = paintCover(renderer, recentBooks[i], slot, QuartumMetrics::values.homeCoverHeight);
      if (!painted) {
        drawMissingCover(renderer, slot);
      }
    }

    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  // Touch readers open a slot directly, so every occupied slot is a target.
  for (int i = 0; i < bookCount; ++i) {
    TouchRegistry::getInstance().add(
        Rect{slotXFor(layout, i), slotYFor(layout, i), layout.slotWidth, layout.slotHeight}, i, TouchRegistry::Cover);
  }

  if (bookCount <= 0 || selectorIndex < 0 || selectorIndex >= bookCount) {
    return;
  }

  const bool isTopRow = selectorIndex < QuartumGridNav::kColumns;
  const int slotX = slotXFor(layout, selectorIndex);
  const int slotY = slotYFor(layout, selectorIndex);
  const Rect slot{slotX, slotY, layout.slotWidth, layout.slotHeight};

  // A double inset outline stays visible over the cover artwork itself.
  renderer.drawRect(slot.x + 1, slot.y + 1, std::max(0, slot.width - 2), std::max(0, slot.height - 2), true);
  renderer.drawRect(slot.x + 2, slot.y + 2, std::max(0, slot.width - 4), std::max(0, slot.height - 4), true);

  const RecentBook& book = recentBooks[selectorIndex];

  // Title: at most two centred lines, placed above the top-row cover or below the
  // bottom-row cover so a one-line title hugs the artwork and a two-line title
  // expands away from it.
  const std::vector<std::string> titleLines =
      MinutaTextLayout::wrapTwoLines(renderer, kTitleFontId, book.title, slot.width);
  if (!titleLines.empty()) {
    const int lineHeight = renderer.getLineHeight(kTitleFontId);
    const int blockHeight =
        static_cast<int>(titleLines.size()) * lineHeight + (static_cast<int>(titleLines.size()) - 1) * kTitleLineGap;
    int lineY = isTopRow ? slot.y - kTitleCoverGap - blockHeight : slot.y + slot.height + kTitleCoverGap;
    for (const auto& line : titleLines) {
      const int lineWidth = renderer.getTextWidth(kTitleFontId, line.c_str(), EpdFontFamily::REGULAR);
      renderer.drawText(kTitleFontId, slot.x + (slot.width - lineWidth) / 2, lineY, line.c_str(), true,
                        EpdFontFamily::REGULAR);
      lineY += lineHeight + kTitleLineGap;
    }
  }

  if (book.author.empty()) {
    return;
  }

  const std::string author =
      renderer.truncatedText(kAuthorFontId, book.author.c_str(), slot.height, EpdFontFamily::ITALIC);
  if (author.empty()) {
    return;
  }

  // Author: one italic line rotated alongside the cover, truncated to the slot
  // height first so it can never run past the slot. The left column reads upward
  // (270 degrees) and the right column downward (90 degrees), so both read away
  // from the cover. Orientation must be restored before any further drawing.
  const int authorTextWidth = renderer.getTextWidth(kAuthorFontId, author.c_str(), EpdFontFamily::ITALIC);
  const int authorBandWidth = renderer.getTextHeight(kAuthorFontId);
  const bool isRightColumn = (selectorIndex % QuartumGridNav::kColumns) != 0;
  const auto originalOrientation = renderer.getOrientation();

  if (isRightColumn) {
    const int portraitWidth = renderer.getScreenWidth();
    const int authorLeftX = slot.x + slot.width + kTitleCoverGap;
    const int authorTopY = isTopRow ? slot.y : slot.y + slot.height - authorTextWidth;
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
    renderer.drawText(kAuthorFontId, authorTopY, portraitWidth - authorLeftX - authorBandWidth, author.c_str(), true,
                      EpdFontFamily::ITALIC);
  } else {
    const int portraitHeight = renderer.getScreenHeight();
    const int authorLeftX = slot.x - kTitleCoverGap - authorBandWidth;
    const int authorAnchorY = isTopRow ? slot.y + authorTextWidth : slot.y + slot.height - 1;
    renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
    renderer.drawText(kAuthorFontId, portraitHeight - 1 - authorAnchorY, authorLeftX, author.c_str(), true,
                      EpdFontFamily::ITALIC);
  }

  renderer.setOrientation(originalOrientation);
}
