// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <cstdio>
#include <iomanip>
#include <gtest/gtest.h>
#include "base/googleinit.h"
#include "base/gtest.h"
#include <sys/time.h>

DEFINE_int32(benchmark_max_iters, 10000000, "Maximum test iterations");
DEFINE_int32(benchmark_min_iters, 100, "Minimum test iterations");
DEFINE_int32(benchmark_target_seconds, 3,
             "Attempt to benchmark for this many seconds");
DEFINE_string(benchmark_filter, "", "substring filter for the benchmarks");
DEFINE_bool(bench, false, "Run benchmarks");

namespace base {
static BenchmarkRun *first_benchmark = nullptr;
static BenchmarkRun *current_benchmark_ = nullptr;

int64_t get_micros () {
  timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

BenchmarkRun::BenchmarkRun(const char *name, void (*func)(uint32_t))
  : next_benchmark_(first_benchmark),
    benchmark_name_(name),
    benchmark_func(func),
    accum_micros_(0),
    last_started_(0) {
  first_benchmark = this;
}

void BenchmarkRun::Start() {
  assert(!last_started_);
  last_started_ = get_micros();
}

void BenchmarkRun::Stop() {
  if (last_started_ == 0) {
    return;
  }
  accum_micros_ += get_micros() - last_started_;
  last_started_ = 0;
}

void BenchmarkRun::Reset() {
  last_started_ = 0;
  accum_micros_ = 0;
}

void BenchmarkRun::Run(unsigned padding_name) {
  assert(current_benchmark_ == NULL);
  current_benchmark_ = this;
  uint32_t iters = FLAGS_benchmark_min_iters;
  std::cout << std::setw(padding_name) << benchmark_name_
              << std::setw(5) << "\t" << std::flush;
  for (;;) {
    Reset();
    Start();
    benchmark_func(iters);
    Stop();
    if (accum_micros_ > FLAGS_benchmark_target_seconds * 1000000 ||
        iters >= uint32_t(FLAGS_benchmark_max_iters)) {
      break;
    } else if (accum_micros_ == 0) {
      iters *= 100;
    } else {
      int64_t target_micros = FLAGS_benchmark_target_seconds * 1000000;
      iters = target_micros * iters / accum_micros_;
    }
    iters = std::min(iters, uint32_t(FLAGS_benchmark_max_iters));
  }
  std::cout << accum_micros_ * 1000 / iters << "\t\t"
            << iters << "\t" << accum_micros_ << std::endl;
  current_benchmark_ = NULL;
}

void BenchmarkRun::RunAllBenchmarks() {
  if (!first_benchmark) return;
  size_t name_size = 0;
  for (BenchmarkRun* bench = first_benchmark; bench;
       bench = bench->next_benchmark_) {
    name_size = std::max(name_size, strlen(bench->benchmark_name_));
  }
  std::cout << std::setw(name_size) << "Name\t" << "Time(ns per iteration)\t #iterations\t"
            << "Time total(usec)\n";
  for (BenchmarkRun* bench = first_benchmark; bench;
       bench = bench->next_benchmark_) {
    if (strstr(bench->benchmark_name_, FLAGS_benchmark_filter.c_str()) != NULL)
      bench->Run(name_size);
  }
}

}  // namespace base

void StopBenchmarkTiming() {
  base::current_benchmark_->Stop();
}

void StartBenchmarkTiming() {
  base::current_benchmark_->Start();
}


int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  MainInitGuard guard(&argc, &argv);
  LOG(INFO) << "Starting tests in " << argv[0];
  int res = RUN_ALL_TESTS();
  if (FLAGS_bench) {
    base::BenchmarkRun::RunAllBenchmarks();
  }
  return res;
}
