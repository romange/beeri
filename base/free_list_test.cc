// Copyright 2015, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "base/free_list.h"

#include <chrono>
#include <random>
#include <thread>
#include "base/gtest.h"

namespace base {

class FreelistTest : public testing::Test {
public:
  FreelistTest() : list_(100) {}

  void DeallocSlow(FreeList::T* t) {
    std::uniform_int_distribution<int> uniform_dist(1, 50);
    std::chrono::milliseconds dura(uniform_dist(re_));
    *t = dura.count();

    std::this_thread::sleep_for(dura);
    list_.Release(t);
  }

protected:
  FreeList list_;
  std::default_random_engine re_;
};

TEST_F(FreelistTest, Basic) {
  std::vector<std::thread> workers;
  for (unsigned i = 0; i < 1000; ++i) {
    workers.emplace_back(&FreelistTest::DeallocSlow, this, list_.New());
    std::this_thread::yield();
  }
  for (unsigned i = 0; i < 1000; ++i) {
    workers[i].join();
  }
  EXPECT_GE(list_.list_allocated, 100);
}

}  // namespace base
