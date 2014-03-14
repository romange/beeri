// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/scheduler.h"

#include <sys/time.h>
#include <algorithm>
#include "base/logging.h"

using std::chrono::milliseconds;
namespace util {

Scheduler::Scheduler() : next_id_(10), scheduler_thread_(&Scheduler::ThreadMain, this) {
  cond_var_ = PTHREAD_COND_INITIALIZER;
  mutex_ = PTHREAD_MUTEX_INITIALIZER;
}

Scheduler::~Scheduler() {
  pthread_mutex_lock(&mutex_);
  next_id_ = 0;
  pthread_cond_signal(&cond_var_);
  pthread_mutex_unlock(&mutex_);
  scheduler_thread_.join();
}

Scheduler::handler_t Scheduler::Schedule(
    std::function<void()> f, milliseconds period, bool is_periodic) {
  DCHECK(f) << "f is not valid!";
  DCHECK(period != milliseconds::zero()) << "period should be non-zero.";
  pthread_mutex_lock(&mutex_);

  handler_t cur_id = next_id_++;

  uint64 special_duration = is_periodic ? period.count() : 0;
  scheduler_map_.emplace(cur_id, Params{f, special_duration});
  ScheduleHandlerLocked(period.count(), cur_id);

  pthread_cond_signal(&cond_var_);
  pthread_mutex_unlock(&mutex_);

  return cur_id;
}

bool Scheduler::Remove(handler_t h) {
  pthread_mutex_lock(&mutex_);
  bool res = scheduler_map_.erase(h) != 0;
  pthread_mutex_unlock(&mutex_);
  return res;
}

inline void Scheduler::ScheduleHandlerLocked(uint64 period, handler_t h) {
  struct timeval now;
  gettimeofday(&now, NULL);
  period += now.tv_sec * 1000;
  period += now.tv_usec / 1000;
  queue_.emplace_back(period, h);
  std::push_heap(queue_.begin(), queue_.end(), std::greater<scheduled_pair>());
}

Scheduler& Scheduler::Default() {
  static Scheduler scheduler;
  return scheduler;
}

void Scheduler::ThreadMain() {
  pthread_mutex_lock(&mutex_);
  struct timespec time_spec;
  while (next_id_ != 0) {
    if (queue_.empty()) {
      VLOG(2) << "before pthread_cond_wait";
      pthread_cond_wait(&cond_var_, &mutex_);
      VLOG(2) << "after pthread_cond_wait";
      continue;
    }
    uint64 next = queue_.front().first;
    VLOG(1) << "Next timepoint in millis " << next;
    time_spec.tv_sec = next/1000;
    time_spec.tv_nsec = (next % 1000) * 1000000UL;
    int status = pthread_cond_timedwait(&cond_var_, &mutex_, &time_spec);
    if (status == 0) {
      continue;
    }
    if (status != ETIMEDOUT) {
      LOG(ERROR) << "Error in pthread_cond_timedwait "  << status;
    }
    // we reached the next time point. Lets pull it out of the queue,
    // and try to run the callback.
    std::pop_heap(queue_.begin(), queue_.end(), std::greater<scheduled_pair>());
    handler_t h = queue_.back().second;
    queue_.pop_back();
    auto it = scheduler_map_.find(h);
    if (it == scheduler_map_.end())
      continue;
    const Params& params = it->second;
    auto f = params.callback;
    DCHECK(f);
    if (params.duration_ms != 0) {
      // It's a periodic function. Schedule it again.
      ScheduleHandlerLocked(params.duration_ms,  h);
    } else {
      scheduler_map_.erase(it);
    }
    pthread_mutex_unlock(&mutex_);
    f();
    pthread_mutex_lock(&mutex_);
  }
  pthread_mutex_unlock(&mutex_);
}

}  // namespace util