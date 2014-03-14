// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
// Copied from cpp-btree project.
#ifndef _BASE_GTEST_H
#define _BASE_GTEST_H

#include <gtest/gtest.h>

namespace base {

class BenchmarkRun {
public:
  BenchmarkRun(const char *name, void (*func)(uint32_t));
  void Run(unsigned padding_name);
  void Stop();
  void Start();
  void Reset();

  static void RunAllBenchmarks();
private:
  BenchmarkRun *next_benchmark_;
  const char *benchmark_name_;
  void (*benchmark_func)(uint32_t);
  int64_t accum_micros_;
  int64_t last_started_;
};

// Used to avoid compiler optimizations for these benchmarks.
// Just call it with the return value of the function.
template <typename T> void sink_result(const T& t0) {
  volatile T t = t0;
  (void)t;
}

}  // namespace base

void StopBenchmarkTiming();

void StartBenchmarkTiming();

#define BENCHMARK(func) \
  static base::BenchmarkRun _internal_bench_ ## func (# func, func)

#define DECLARE_BENCHMARK_FUNC(name, iters) \
  static void name(uint32_t); \
  BENCHMARK(name); \
  void name(uint32_t iters)

#endif  // _BASE_GTEST_H