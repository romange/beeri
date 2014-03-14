// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/sync_queue.h"
#include <future>
#include <memory>
#include <gtest/gtest.h>

#include "base/logging.h"

namespace base {

class SyncQueueTest : public testing::Test {
protected:
  void SetUp() {
    q_.reset(new sync_queue<int>());
  }

  void Pop(int times) {
    for (int i = 0; i < times; ++i) {
      q_->pop();
      std::lock_guard<std::mutex> lock(mu_);
      op_log_.push_back('r');
    }
  }

  void PushN(int times) {
    for (int i = 0; i < times; ++i) {
      // there is a correctness bug here, since those 2 lines are not
      // atomic. I need to think how to change the test.
      q_->push(i);
      std::lock_guard<std::mutex> lock(mu_);
      op_log_.push_back('w');
    }
  }

  std::unique_ptr<sync_queue<int>> q_;
  std::string op_log_;
  std::mutex mu_;
};

TEST_F(SyncQueueTest, Basic) {
  base::ProgramAbsoluteFileName();
  for (int i = 0; i < 10; ++i)
    q_->push(i*5);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(5*i, q_->pop());
  }
}

TEST_F(SyncQueueTest, Threads) {
  auto f = std::async(std::launch::async, [this]() { Pop(2); });
  PushN(2);
  f.wait();
  ASSERT_EQ(4, op_log_.size());
  int level = 0;
  for (char c : op_log_) {
    if (c == 'r') {
      ASSERT_GT(level, 0);
      --level;
    } else
      ++level;
  }
}

TEST_F(SyncQueueTest, Blocked) {
  q_.reset(new sync_queue<int>(1));
  auto f = std::async(std::launch::async, [this]() { Pop(3); });
  PushN(3);
  f.wait();
  EXPECT_EQ("wrwrwr", op_log_);
}

TEST_F(SyncQueueTest, TimedWait) {
  q_.reset(new sync_queue<int>());
  int res = 0;
  EXPECT_FALSE(q_->pop(20, &res));

  auto f = std::async(std::launch::async, [this, &res]() { q_->pop(100, &res); });
  q_->push(5);
  f.wait();
  EXPECT_EQ(5, res);
}

}  // namespace base
