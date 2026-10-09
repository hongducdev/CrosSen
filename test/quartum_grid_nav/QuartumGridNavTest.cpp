// Quartum's Home cursor is a ring over the most recent books, driven by front
// buttons: Left/Right step one book and Up/Down step a whole row of the fixed 2x2
// grid. The wrap-around behaviour is easy to get subtly wrong (especially with two
// or three books, where a two-slot step can be an identity move), so it lives in a
// pure header and is covered here without any renderer or SDK dependency.

#include <gtest/gtest.h>

#include "components/themes/minuta/QuartumGridNav.h"

namespace {

using QuartumGridNav::down;
using QuartumGridNav::left;
using QuartumGridNav::nextIndex;
using QuartumGridNav::right;
using QuartumGridNav::up;

TEST(QuartumGridNav, NoBooksStayAtZero) {
  EXPECT_EQ(nextIndex(0, 0, 1), 0);
  EXPECT_EQ(nextIndex(0, 0, -1), 0);
  EXPECT_EQ(nextIndex(0, -4, 2), 0);
  EXPECT_EQ(nextIndex(3, 0, -2), 0);
}

TEST(QuartumGridNav, SingleBookNeverMoves) {
  EXPECT_EQ(left(0, 1), 0);
  EXPECT_EQ(right(0, 1), 0);
  EXPECT_EQ(up(0, 1), 0);
  EXPECT_EQ(down(0, 1), 0);
}

TEST(QuartumGridNav, OutOfRangeCursorIsPulledBackToFirstSlot) {
  EXPECT_EQ(nextIndex(7, 4, 0), 0);
  EXPECT_EQ(nextIndex(-1, 4, 0), 0);
  EXPECT_EQ(nextIndex(4, 4, 0), 0);
  // A clamped cursor still moves relative to slot 0.
  EXPECT_EQ(nextIndex(9, 4, 1), 1);
}

TEST(QuartumGridNav, TwoBooksOnlyStepSideways) {
  EXPECT_EQ(right(0, 2), 1);
  EXPECT_EQ(right(1, 2), 0);
  EXPECT_EQ(left(0, 2), 1);
  EXPECT_EQ(left(1, 2), 0);

  // Stepping a whole row in a two-slot ring returns to the same book, so Up/Down
  // are a deliberate no-op rather than an arbitrary jump.
  EXPECT_EQ(down(0, 2), 0);
  EXPECT_EQ(down(1, 2), 1);
  EXPECT_EQ(up(0, 2), 0);
  EXPECT_EQ(up(1, 2), 1);
}

TEST(QuartumGridNav, ThreeBooksWrapInEveryDirection) {
  EXPECT_EQ(left(0, 3), 2);
  EXPECT_EQ(right(2, 3), 0);

  // Up/Down move by two slots, which in a three-book ring is the same as stepping
  // one slot the other way.
  EXPECT_EQ(down(0, 3), 2);
  EXPECT_EQ(down(2, 3), 1);
  EXPECT_EQ(up(1, 3), 2);
  EXPECT_EQ(up(0, 3), 1);
}

TEST(QuartumGridNav, FourBooksFormAQuad) {
  // Horizontally the cursor alternates within a row.
  EXPECT_EQ(right(0, 4), 1);
  EXPECT_EQ(right(1, 4), 2);
  EXPECT_EQ(right(3, 4), 0);
  EXPECT_EQ(left(2, 4), 1);
  EXPECT_EQ(left(0, 4), 3);

  // Vertically the cursor swaps rows, keeping the column.
  EXPECT_EQ(down(0, 4), 2);
  EXPECT_EQ(down(1, 4), 3);
  EXPECT_EQ(down(2, 4), 0);
  EXPECT_EQ(down(3, 4), 1);
  EXPECT_EQ(up(2, 4), 0);
  EXPECT_EQ(up(3, 4), 1);
  EXPECT_EQ(up(0, 4), 2);
  EXPECT_EQ(up(1, 4), 3);
}

TEST(QuartumGridNav, RoundTripsReturnToTheStart) {
  for (int bookCount = 2; bookCount <= 6; ++bookCount) {
    for (int start = 0; start < bookCount; ++start) {
      EXPECT_EQ(right(left(start, bookCount), bookCount), start) << "n=" << bookCount << " start=" << start;
      EXPECT_EQ(left(right(start, bookCount), bookCount), start) << "n=" << bookCount << " start=" << start;
      EXPECT_EQ(down(up(start, bookCount), bookCount), start) << "n=" << bookCount << " start=" << start;
      EXPECT_EQ(up(down(start, bookCount), bookCount), start) << "n=" << bookCount << " start=" << start;
    }
  }
}

TEST(QuartumGridNav, EveryMoveStaysWithinRange) {
  for (int bookCount = 0; bookCount <= 8; ++bookCount) {
    for (int start = 0; start < 8; ++start) {
      for (int delta = -4; delta <= 4; ++delta) {
        const int moved = nextIndex(start, bookCount, delta);
        EXPECT_GE(moved, 0) << "n=" << bookCount << " start=" << start << " delta=" << delta;
        EXPECT_LT(moved, std::max(1, bookCount)) << "n=" << bookCount << " start=" << start << " delta=" << delta;
      }
    }
  }
}

}  // namespace
