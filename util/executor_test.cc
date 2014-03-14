// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/executor.h"
#include <atomic>
#include <thread>
#include <gtest/gtest.h>

namespace util {

class ExecutorTest : public testing::Test {
protected:
};

TEST_F(ExecutorTest, Basic) {
  Executor executor(4);
  std::atomic_long val(0);
  for (int i = 0; i < 10; ++i) {
    executor.Add([&val]() { val.fetch_add(1); });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  executor.Shutdown();
  executor.WaitForLoopToExit();
  EXPECT_EQ(10, val);

  executor.Add([&val]() { val.fetch_add(20); });  // no op.
  executor.WaitForLoopToExit(); // should be no op.
   std::this_thread::sleep_for(std::chrono::milliseconds(30));
  EXPECT_EQ(10, val);
}

TEST_F(ExecutorTest, ManyExecutors) {
  for (int i = 0; i < 1300; ++i) {
    Executor* executor = new Executor;
    executor->Shutdown();
    delete executor;
  }
}

}  // namespace util