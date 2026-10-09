#pragma once

// Cursor movement for the Quartum 2x2 Home grid.
//
// Quartum is a fixed quad over the four most recent books, so the cursor is an
// index into that list rather than a scroll position: Left/Right step one book and
// Up/Down step a whole row, both wrapping. Up/Down are naturally a no-op with two
// books because stepping two slots in a two-slot ring returns to the start.
//
// This is deliberately free of renderer and SDK types so the native test suite can
// cover the wrap-around behaviour without stubbing the device layer.
namespace QuartumGridNav {

constexpr int kColumns = 2;
constexpr int kSlots = 4;

// delta is the number of slots to move: -1/+1 for Left/Right, -2/+2 for Up/Down.
// An out-of-range cursor is pulled back to the first slot so a shrunken recents
// list can never index past the end.
constexpr int nextIndex(const int current, const int bookCount, const int delta) {
  if (bookCount <= 0) {
    return 0;
  }
  if (bookCount == 1) {
    return 0;
  }
  const int base = (current < 0 || current >= bookCount) ? 0 : current;
  const int shifted = (base + delta) % bookCount;
  return shifted < 0 ? shifted + bookCount : shifted;
}

constexpr int left(const int current, const int bookCount) { return nextIndex(current, bookCount, -1); }
constexpr int right(const int current, const int bookCount) { return nextIndex(current, bookCount, 1); }
constexpr int up(const int current, const int bookCount) { return nextIndex(current, bookCount, -kColumns); }
constexpr int down(const int current, const int bookCount) { return nextIndex(current, bookCount, kColumns); }

}  // namespace QuartumGridNav
