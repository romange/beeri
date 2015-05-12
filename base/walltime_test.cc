// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@romange.com)
//
#include "base/walltime.h"

#include <mutex>
#include <time.h>
#include <functional>
#include <benchmark/benchmark.h>

#include "base/gtest.h"
#include "base/logging.h"
// #include "base/mutex.h"


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

TEST_F(WalltimeTest, Tmzone) {
  int est_diff = base::TimezoneDiff("EST");
  EXPECT_EQ(-5, est_diff);
  int ny_diff = base::TimezoneDiff("America/New_York");
  EXPECT_LT(ny_diff, 0);
  EXPECT_LE(est_diff, ny_diff);
}

TEST_F(WalltimeTest, ClockRes) {
  timespec ts;
  ASSERT_EQ(0, clock_getres(CLOCK_REALTIME_COARSE, &ts));
  ASSERT_EQ(0, ts.tv_sec);
  EXPECT_LE(ts.tv_nsec, 4000000);

  ASSERT_EQ(0, clock_getres(CLOCK_MONOTONIC_COARSE, &ts));
  ASSERT_EQ(0, ts.tv_sec);
  EXPECT_LE(ts.tv_nsec, 4000000);

  ASSERT_EQ(0, clock_getres(CLOCK_PROCESS_CPUTIME_ID, &ts));
  EXPECT_LE(ts.tv_nsec, 4000000);

  ASSERT_EQ(0, clock_getres(CLOCK_MONOTONIC, &ts));
  EXPECT_LE(ts.tv_nsec, 1000000);
}

static void BM_GetCurrentTimeMicros(benchmark::State& state) {
  LOG(INFO) << "Cycle frequency: " << CycleClock::CycleFreq();
  while (state.KeepRunning()) {
    sink_result(GetCurrentTimeMicros());
  }
}
BENCHMARK(BM_GetCurrentTimeMicros);

DECLARE_BENCHMARK_FUNC(BM_Time, iters) {
  while (state.KeepRunning()) {
    sink_result(time(NULL));
    sink_result(time(NULL));
  }
}

DECLARE_BENCHMARK_FUNC(BM_GetTimeOfDay, iters) {
  struct timeval tv;
  while (state.KeepRunning()) {
    sink_result(gettimeofday(&tv, NULL));
  }
}

DECLARE_BENCHMARK_FUNC(BM_ClockRealtimeCoarse, iters) {
  timespec ts;
  while (state.KeepRunning()) {
    sink_result(clock_gettime(CLOCK_REALTIME_COARSE, &ts));
  }
}

DECLARE_BENCHMARK_FUNC(BM_ClockMonotonic, iters) {
  timespec ts;
  while (state.KeepRunning()) {
    sink_result(clock_gettime(CLOCK_MONOTONIC, &ts));
  }
}

DECLARE_BENCHMARK_FUNC(BM_ClockMonotonicCoarse, iters) {
  timespec ts;
  while (state.KeepRunning()) {
    sink_result(clock_gettime(CLOCK_MONOTONIC_COARSE, &ts));
  }
}

DECLARE_BENCHMARK_FUNC(BM_ClockRealtime, iters) {
  timespec ts;
  while (state.KeepRunning()) {
    sink_result(clock_gettime(CLOCK_REALTIME, &ts));
  }
}

DECLARE_BENCHMARK_FUNC(BM_ClockProcessCPUID, iters) {
  timespec ts;
  while (state.KeepRunning()) {
    sink_result(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts));
  }
}

DECLARE_BENCHMARK_FUNC(BM_CycleClock, iters) {
  while (state.KeepRunning()) {
    sink_result(CycleClock::Now());
  }
}

DECLARE_BENCHMARK_FUNC(BM_ReadLock, iters) {
  pthread_rwlock_t lock;
  CHECK_EQ(0, pthread_rwlock_init(&lock, nullptr));
  CHECK_EQ(0, pthread_rwlock_rdlock(&lock));
  while (state.KeepRunning()) {
    CHECK_EQ(0, pthread_rwlock_rdlock(&lock));
    CHECK_EQ(0, pthread_rwlock_unlock(&lock));
  }
  CHECK_EQ(0, pthread_rwlock_unlock(&lock));
  pthread_rwlock_destroy(&lock);
}

DECLARE_BENCHMARK_FUNC(BM_WriteLock, iters) {
  pthread_rwlock_t lock;
  CHECK_EQ(0, pthread_rwlock_init(&lock, nullptr));
  while (state.KeepRunning()) {
    CHECK_EQ(0, pthread_rwlock_wrlock(&lock));
    CHECK_EQ(0, pthread_rwlock_unlock(&lock));
  }
  pthread_rwlock_destroy(&lock);
}

DECLARE_BENCHMARK_FUNC(BM_StlMutexLock, iters) {
  std::mutex m;
  while (state.KeepRunning()) {
    m.lock();
    m.unlock();
  }
}

}  // namespace base
