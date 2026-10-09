#include "components/themes/minuta/SolumTheme.h"

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
#include "fontIds.h"

namespace {
// Homage to the reference layout: the title baseline sits a fixed distance below
// the cover, and the author hangs off the title's line box.
constexpr int kTitleBaselineGap = 46;
constexpr int kAuthorGap = -4;
constexpr int kMinSidePadding = 40;
constexpr int kCoverIconSize = 32;

// CrossInk's Home thumbnails are generated at this ratio by
// UITheme::getCoverThumbPath(path, height), and LyraTheme sizes its cover the same
// way, so the painted cover matches the cached bitmap with no scaling and no
// letterboxing. Solum keeps that ratio rather than the reference's 3:5 because the
// reference forces its box with drawBitmapStretched, which has no equivalent here.
constexpr int kCoverAspectWidth = 2;
constexpr int kCoverAspectHeight = 3;

const int kTitleFontId = LEXENDDECA_16_FONT_ID;
const int kAuthorFontId = UI_10_FONT_ID;

// Text may run slightly wider than the cover, as in the reference, but never into
// the bezel.
int textAreaWidthFor(const GfxRenderer& renderer) {
  return std::max(1, renderer.getScreenWidth() - 2 * kMinSidePadding);
}
}  // namespace

Rect SolumTheme::coverRectFor(const GfxRenderer& renderer, const Rect& rect) {
  const int sidePadding = std::max(renderer.getScreenWidth() / 8, kMinSidePadding);
  const int maxWidth = std::max(0, rect.width - 2 * sidePadding);
  const int maxHeight = std::max(0, rect.height);
  int height = SolumMetrics::values.homeCoverHeight;
  int width = height * kCoverAspectWidth / kCoverAspectHeight;
  if (width > maxWidth && maxWidth > 0) {
    width = maxWidth;
    height = width * kCoverAspectHeight / kCoverAspectWidth;
  }
  if (height > maxHeight && maxHeight > 0) {
    height = maxHeight;
    width = height * kCoverAspectWidth / kCoverAspectHeight;
  }
  return Rect{rect.x + (rect.width - width) / 2, rect.y, width, height};
}

void SolumTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                     int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                     bool& bufferRestored, const std::function<bool()>& storeCoverBuffer,
                                     const BookReadingStats* /*stats*/, float /*progressPercent*/,
                                     const GlobalReadingStats* /*globalStats*/,
                                     const char* /*currentChapterTitle*/) const {
  (void)bufferRestored;

  const Rect cover = coverRectFor(renderer, rect);
  const bool hasBook = !recentBooks.empty();

  if (!hasBook) {
    // A bare outline where the cover would be: no fill, no icon, no text.
    renderer.drawRect(cover.x, cover.y, cover.width, cover.height, true);
    coverRendered = false;
    coverBufferStored = false;
    return;
  }

  const int index = std::clamp(selectorIndex, 0, static_cast<int>(recentBooks.size()) - 1);
  const RecentBook& book = recentBooks[index];

  if (!coverRendered) {
    bool hasCover = !book.coverBmpPath.empty();
    if (hasCover) {
      const std::string coverBmpPath = UITheme::getCoverThumbPath(book.coverBmpPath, cover.height);
      HalFile file;
      if (!coverBmpPath.empty() && Storage.openFileForRead("HOME", coverBmpPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderer.drawBitmap(bitmap, cover.x, cover.y, cover.width, cover.height);
        } else {
          hasCover = false;
        }
        file.close();
      } else {
        hasCover = false;
      }
    }

    if (!hasCover) {
      renderer.fillRect(cover.x, cover.y + cover.height / 3, cover.width, 2 * cover.height / 3, true);
      renderer.drawIcon(CoverIcon, cover.x + cover.width / 2 - kCoverIconSize / 2,
                        cover.y + cover.height / 2 - kCoverIconSize / 2, kCoverIconSize);
    }

    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  // Touch readers open the cover directly, so the target must match the box the
  // bitmap was painted into rather than the whole tile.
  TouchRegistry::getInstance().add(cover, 0, TouchRegistry::Cover);

  const int textAreaWidth = textAreaWidthFor(renderer);
  const int textAreaX = rect.x + (rect.width - textAreaWidth) / 2;

  // Title: one word-boundary-fitted line under the cover.
  const int titleBaselineY = cover.y + cover.height + kTitleBaselineGap;
  const int titleY = titleBaselineY - renderer.getFontAscenderSize(kTitleFontId);
  const std::string title = MinutaTextLayout::truncateAtWord(renderer, kTitleFontId, book.title, textAreaWidth);
  if (!title.empty()) {
    const int titleWidth = renderer.getTextWidth(kTitleFontId, title.c_str(), EpdFontFamily::REGULAR);
    renderer.drawText(kTitleFontId, textAreaX + (textAreaWidth - titleWidth) / 2, titleY, title.c_str(), true,
                      EpdFontFamily::REGULAR);
  }

  if (book.author.empty()) {
    return;
  }

  const std::string author = MinutaTextLayout::truncateAtWord(renderer, kAuthorFontId, book.author, textAreaWidth);
  if (author.empty()) {
    return;
  }
  const int authorBaselineY = titleBaselineY + renderer.getLineHeight(kTitleFontId) + kAuthorGap;
  const int authorY = authorBaselineY - renderer.getFontAscenderSize(kAuthorFontId);
  const int authorWidth = renderer.getTextWidth(kAuthorFontId, author.c_str(), EpdFontFamily::REGULAR);
  renderer.drawText(kAuthorFontId, textAreaX + (textAreaWidth - authorWidth) / 2, authorY, author.c_str(), true,
                    EpdFontFamily::REGULAR);
}
