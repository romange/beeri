// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/stats/sliding_counter.h"

#include "base/gtest.h"
#include "base/logging.h"
#include "util/stats/hour_counter.h"

namespace util {

class SlidingCounterTest : public testing::Test {
};

TEST_F(SlidingCounterTest, HourCounter) {
  static_assert(sizeof(HourCounter) == 4, "Wrong counter size");
  uint32 base = time(NULL);
  HourCounter c;
  c.IncAtTime(base);
  c.IncAtTime(base);
  EXPECT_EQ(2, c.value());
  c.IncAtTime(base + 3600);
  EXPECT_EQ(0, c.value());
}

class SlidingSecondCounterTest : public testing::Test {
};

TEST_F(SlidingSecondCounterTest, Basic) {
  SlidingSecondBase::SetCurrentTime_Test(1);
  SlidingSecondCounter<10,1> second_counter;
  second_counter.Inc();
  second_counter.Inc();
  EXPECT_EQ(2, second_counter.Sum());
  SlidingSecondBase::SetCurrentTime_Test(2);
  EXPECT_EQ(2, second_counter.Sum());
  EXPECT_EQ(0, second_counter.SumLast(0, 1));
  EXPECT_EQ(2, second_counter.SumLast(1, 1));
  EXPECT_EQ(0, second_counter.SumLast(2, 1));
  second_counter.Inc();
  EXPECT_EQ(1, second_counter.SumLast(0, 1));
  EXPECT_EQ(3, second_counter.Sum());

  SlidingSecondBase::SetCurrentTime_Test(11);
  EXPECT_EQ(1, second_counter.Sum());
  SlidingSecondBase::SetCurrentTime_Test(12);
  EXPECT_EQ(0, second_counter.Sum());
  EXPECT_EQ(0, second_counter.DecIfNotLess(1));

  EXPECT_LE(sizeof(SlidingSecondCounter<10,1>), 44);
}

#if 0
DECLARE_BENCHMARK_FUNC(BM_IncFresh, iters) {
  SlidingCounterBase<uint32, 17> window;
  window.Inc(9);
  for (int i = 0; i < iters; ++i) {
    window.Inc(i << 1);
  }
}
#endif

DECLARE_BENCHMARK_FUNC(BM_IncSecondCounter, iters) {
  SlidingSecondCounter<10,60> cnt;
  for (int i = 0; i < iters; ++i) {
    cnt.Inc();
  }
}

#if 0
DECLARE_BENCHMARK_FUNC(BM_Sum, iters) {
  SlidingCounterBase<uint32, 17> window;
  window.Inc(9);
  for (int i = 0; i < iters; ++i) {
    base::sink_result(window.SumLast(8, 6));
  }
}
#endif

}  // namespace util