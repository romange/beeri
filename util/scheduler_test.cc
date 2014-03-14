// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/scheduler.h"
#include "util/proc_stats.h"

#include <mutex>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Matches;

namespace util {
using std::chrono::milliseconds;
using namespace std;
using chrono::system_clock;
using chrono::duration_cast;

class SchedulerTest : public testing::Test {
protected:
  Scheduler::handler_t Schedule(uint32 val, uint32 period_msec, bool is_periodic = true) {
    return scheduler_.Schedule([=]() {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_.push_back(val);
      }, milliseconds(period_msec), is_periodic);
  }

  mutex mutex_;
  vector<uint32> vec_;
  Scheduler scheduler_;
};

TEST_F(SchedulerTest, ProcStats) {
  ProcessStats stats = ProcessStats::Read();
  EXPECT_GT(stats.vm_size, 0);
  EXPECT_GE(stats.vm_peak, stats.vm_size);
  EXPECT_GE(stats.vm_rss, 0);
  EXPECT_GE(stats.start_time_seconds, 1341904000);
  cout << "Stats: " << stats << endl;
}

TEST_F(SchedulerTest, Basic) {
  const int k20ms = 20;
  const int k100ms = 100;

  Schedule(k100ms, k100ms);
  Schedule(k20ms, k20ms);

  this_thread::sleep_for(milliseconds(500));

  std::lock_guard<std::mutex> lock(mutex_);

  ASSERT_GT(vec_.size(), 26);
  EXPECT_EQ(k20ms, vec_.front());

  EXPECT_THAT(count_if(vec_.begin(), vec_.end(), bind1st(equal_to<uint32>(), k20ms)),
              AllOf(Ge(22), Le(27)));
  EXPECT_THAT(count_if(vec_.begin(), vec_.end(), bind1st(equal_to<uint32>(), k100ms)),
              AllOf(Ge(4), Le(7)));
}

TEST_F(SchedulerTest, Remove) {
  auto h1 = Schedule(100, 20);
  ASSERT_TRUE(scheduler_.Remove(h1));
  ASSERT_FALSE(scheduler_.Remove(h1));
  size_t sz = vec_.size();
  this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(sz, vec_.size());
  vec_.clear();
  const int kId = 1;
  auto h2 = Schedule(kId, 10, false); // one time only
  this_thread::sleep_for(milliseconds(50));
  ASSERT_FALSE(scheduler_.Remove(h2));
  ASSERT_EQ(1, vec_.size());
  EXPECT_EQ(kId, vec_.front());
}

TEST_F(SchedulerTest, Precision) {
  const int kPeriodms = 20;
  system_clock::time_point prev = system_clock::now();

  scheduler_.Schedule([=, &prev]() {
        std::lock_guard<std::mutex> lock(mutex_);
        system_clock::time_point now = system_clock::now();
        milliseconds duration = duration_cast<milliseconds>(now - prev);
        prev = now;
        vec_.push_back(duration.count());
      }, milliseconds(kPeriodms), true);
  this_thread::sleep_for(milliseconds(500));
  for (uint32 v : vec_) {
    EXPECT_THAT(v, AllOf(Ge(kPeriodms - 2), Le(kPeriodms + 2)));
  }
}

}  // namespace util