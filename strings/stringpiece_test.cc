#include <array>
#include <gtest/gtest.h>
#include "base/logging.h"
#include "strings/stringpiece.h"

class StringPieceTest : public testing::Test {
};

TEST_F(StringPieceTest, Length) {
  StringPiece pc("Fooo");
  EXPECT_EQ(4, pc.length());
  std::array<uint8, 4> array{{1, 3, 4, 8}};
  strings::Slice slice(array.data(), array.size());
  EXPECT_EQ(4, slice.length());
  EXPECT_EQ(3, slice[1]);
  pc.rfind('o', 0);
}