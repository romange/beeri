// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _EXECUTOR_H
#define _EXECUTOR_H

#include <memory>

struct event_base;

namespace util {

class Executor {
  class Rep;
  std::unique_ptr<Rep> rep_;

public:
  // if num_threads is 0, then Executor will choose number of threads automatically
  // based on the number of cpus in the system.
  explicit Executor(unsigned int num_threads = 0);
  ~Executor();

  event_base* ebase();

  void Add(std::function<void()> f);

  // Async function that tells Executor to shut down all its worker threads and its event loop.
  void Shutdown();

  // Blocks the calling thread until the event loop exits.
  // Some other thread should call ShutdownEventLoop for that to happen.
  void WaitForLoopToExit();

  void StopOnTermSignal();

  static Executor& Default();
};

}  // namespace util

#endif  // _EXECUTOR_H