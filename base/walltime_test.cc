// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/gtest.h"
#include "base/logging.h"
#include "base/walltime.h"
#include "base/mutex.h"
#include <mutex>
#include <time.h>

namespace base {

class WalltimeTest : public testing::Test {
};

TEST_F(WalltimeTest, BasicTimer) {
  LOG(INFO) << "Resolution in ms: " << Timer::ResolutionUsec() / 1000;

  Timer timer;
  EXPECT_EQ(0, timer.EvalUsec());
  SleepForMilliseconds(4);
  EXPECT_LE(timer.EvalUsec(), 8000);
}

class Dummy {
  char buf[1000];
public:
  void f() noexcept {};
};

constexpr uint32 kNoCopyLimit = sizeof(std::_Nocopy_types);
constexpr uint32 kAlignLimit = __alignof__(std::_Nocopy_types);

template<typename _Functor> class GccCopyPolicy {
public:
 static const bool value =
 (std::__is_location_invariant<_Functor>::value && sizeof(_Functor) <= kNoCopyLimit
       && __alignof__(_Functor) <= kAlignLimit
       && (kAlignLimit % __alignof__(_Functor) == 0));
};


TEST_F(WalltimeTest, Lambda) {
  auto lambda = [](){};

  typedef decltype(lambda) lambda_t;

  // That means that lambdas are always allocated on heap.
  EXPECT_FALSE(GccCopyPolicy<lambda_t>::value);
  Dummy d;
  auto f = std::bind(&Dummy::f, &d);
  typedef decltype(f) dummy_f_t;

  // Also std::bind
  EXPECT_FALSE(GccCopyPolicy<dummy_f_t>::value);

  typedef void (*FuncCb)(Dummy);
  EXPECT_TRUE(GccCopyPolicy<FuncCb>::value);
}

DECLARE_BENCHMARK_FUNC(BM_GetCurrentTimeMicros, iters) {
  LOG(INFO) << "Cycle frequency: " << CycleClock::CycleFreq();
  for (int i = 0; i < iters; ++i) {
    sink_result(GetCurrentTimeMicros());
  }
}

DECLARE_BENCHMARK_FUNC(BM_Time, iters) {
  for (int i = 0; i < iters; ++i) {
    sink_result(time(NULL));
  }
}

DECLARE_BENCHMARK_FUNC(BM_GetTimeOfDay, iters) {
  struct timeval tv;
  for (int i = 0; i < iters; ++i) {
    sink_result(gettimeofday(&tv, NULL));
  }
}

DECLARE_BENCHMARK_FUNC(BM_ClockCoarse, iters) {
  timespec ts;
  for (size_t i = 0; i < iters; ++i) {
    sink_result(clock_gettime(CLOCK_REALTIME_COARSE, &ts));
  }
}

DECLARE_BENCHMARK_FUNC(BM_CycleClock, iters) {
  for (size_t i = 0; i < iters; ++i) {
    sink_result(CycleClock::Now());
  }
}

DECLARE_BENCHMARK_FUNC(BM_ReadLock, iters) {
  pthread_rwlock_t lock;
  CHECK_EQ(0, pthread_rwlock_init(&lock, nullptr));
  CHECK_EQ(0, pthread_rwlock_rdlock(&lock));
  for (size_t i = 0; i < iters; ++i) {
    CHECK_EQ(0, pthread_rwlock_rdlock(&lock));
    CHECK_EQ(0, pthread_rwlock_unlock(&lock));
  }
  CHECK_EQ(0, pthread_rwlock_unlock(&lock));
  pthread_rwlock_destroy(&lock);
}

DECLARE_BENCHMARK_FUNC(BM_WriteLock, iters) {
  pthread_rwlock_t lock;
  CHECK_EQ(0, pthread_rwlock_init(&lock, nullptr));
  for (size_t i = 0; i < iters; ++i) {
    CHECK_EQ(0, pthread_rwlock_wrlock(&lock));
    CHECK_EQ(0, pthread_rwlock_unlock(&lock));
  }
  pthread_rwlock_destroy(&lock);
}

DECLARE_BENCHMARK_FUNC(BM_MutexLock, iters) {
  Mutex m;
  for (size_t i = 0; i < iters; ++i) {
    m.Lock();
    m.Unlock();
  }
}

DECLARE_BENCHMARK_FUNC(BM_StlMutexLock, iters) {
  std::mutex m;
  for (size_t i = 0; i < iters; ++i) {
    m.lock();
    m.unlock();
  }
}

}  // namespace base
