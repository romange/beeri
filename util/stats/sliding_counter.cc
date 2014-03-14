// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/stats/sliding_counter.h"

#include "base/walltime.h"

#include <time.h>
#include <pthread.h>

#define PTHREAD_CALL(x) \
  do { \
    int my_err = pthread_ ## x; \
    CHECK_EQ(0, my_err) << strerror(my_err); \
  } while(false)

namespace util {

static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static pthread_t g_time_update_thread = 0;
static bool g_test_used = false;

std::atomic<uint32> SlidingSecondBase::current_time_global_ = ATOMIC_VAR_INIT(0);

SlidingSecondBase::SlidingSecondBase() {
  pthread_once(&g_init_once, &SlidingSecondBase::InitTimeGlobal);
}

void SlidingSecondBase::InitTimeGlobal() {
  if (!g_test_used) current_time_global_ = time(NULL);
  pthread_attr_t attrs;
  PTHREAD_CALL(attr_init(&attrs));

  PTHREAD_CALL(attr_setstacksize(&attrs, PTHREAD_STACK_MIN));
  PTHREAD_CALL(create(&g_time_update_thread, &attrs, &SlidingSecondBase::UpdateTimeGlobal, nullptr));
  PTHREAD_CALL(setname_np(g_time_update_thread, "UpdateTimeTh"));
  PTHREAD_CALL(attr_destroy(&attrs));
}

void* SlidingSecondBase::UpdateTimeGlobal(void*) {
  uint32 t = current_time_global_;
  while(true) {
    // To support unit testing - if current_time_global_ was out of sync from what to expect
    uint32 new_val = time(NULL);
    if (g_test_used ||
        !current_time_global_.compare_exchange_strong(t, new_val, std::memory_order_acq_rel))
      break;
    t = new_val;
    SleepForMilliseconds(100);
  }
  LOG(INFO) << "UpdateTimeGlobal exited.";
  return nullptr;
}

void SlidingSecondBase::SetCurrentTime_Test(uint32 time_val) {
  g_test_used = true;
  current_time_global_.store(time_val, std::memory_order_release);
}

uint32 QPSCount::Get() const {
  constexpr unsigned kWinSize = decltype(window_)::SIZE - 1;

  return window_.SumLast(1, kWinSize) / kWinSize; // Average over kWinSize values.
}


}  // namespace util