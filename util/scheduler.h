// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <chrono>
#include <pthread.h>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "base/integral_types.h"

namespace util {

// This class is threadsafe.
class Scheduler {
  struct Params {
    std::function<void()> callback;
    uint64 duration_ms;
  };

public:
  typedef uint32 handler_t;
  static constexpr uint32 INVALID_HANDLE = 0;

  Scheduler();

  // Shuts down the scheduling thread.
  ~Scheduler();

  // period can not be 0.
  handler_t Schedule(std::function<void()> f, std::chrono::milliseconds period,
                     bool is_periodic = true);

  // Returns true if removal succeeded or false if the handler was not found
  // or non-periodic function has already been run and its handler
  // has been removed already.
  bool Remove(handler_t h);

  static Scheduler& Default();
private:
  // typedef std::chrono::time_point<std::chrono::system_clock> time_point;
  // milliseconds
  typedef std::pair<uint64, handler_t> scheduled_pair;

  void ThreadMain();  // exits when next_id_ is 0 (assuming that we never reach maxuint32)

  void ScheduleHandlerLocked(uint64 milliseconds, handler_t h);

  std::unordered_map<handler_t, Params> scheduler_map_;

  std::vector<scheduled_pair> queue_;

  handler_t next_id_;
  pthread_mutex_t mutex_;
  std::thread scheduler_thread_;

  pthread_cond_t cond_var_;
  // std::condition_variable ;
};

}  // namespace util

#endif  // SCHEDULER_H