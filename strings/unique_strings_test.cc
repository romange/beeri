// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "strings/unique_strings.h"
#include <gtest/gtest.h>

class UniqueStringsTest : public testing::Test {
};

TEST_F(UniqueStringsTest, Base) {
  UniqueStrings unique;
  StringPiece foo1 = unique.Get("foo");
  string str("foo");
  StringPiece foo2 = unique.Get(str);

  EXPECT_EQ(foo1, foo2);
  EXPECT_EQ(foo1.data(), foo2.data());
  StringPiece bar = unique.Get("bar");
  EXPECT_NE(bar, foo2);
}