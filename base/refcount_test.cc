// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/refcount.h"

#include <gtest/gtest.h>

namespace base {

static int constructed = 0;
static int destructed = 0;

class Example : public RefCount<Example> {
public:
  Example() { ++constructed; }
  ~Example() { ++destructed; }
};

class RefCountTest : public testing::Test {
protected:
};

TEST_F(RefCountTest, Basic) {
  Example* e = new Example();
  e->AddRef();
  EXPECT_FALSE(e->DecRef());
  EXPECT_TRUE(e->DecRef());
  EXPECT_EQ(1, constructed);
  EXPECT_EQ(1, destructed);
}

struct Foo {
  int i = 0;

  Foo() {}
  Foo(const Foo& f) : i(f.i + 1) {}
  Foo(Foo&& f) = default;
};

__attribute__ ((noinline)) int Bar2(Foo f) {
static std::vector<Foo> v;
  v.push_back(std::move(f));
  return v.back().i;
}

__attribute__ ((noinline)) int Bar1(Foo f) {
  return Bar2(std::move(f));
}

class MoveTest : public testing::Test {
protected:
};

TEST_F(MoveTest, Basic) {
  EXPECT_EQ(0, Bar1(Foo()));
}

}  // namespace base