// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/hash.h"

#include <city.h>

#include "base/gtest.h"
#include "file/filesource.h"
#include "strings/strip.h"

using namespace std;

namespace base {

static std::vector<std::string> ReadIds() {
  file::LineReader line_reader(base::ProgramRunfile("testdata/ids.txt.gz"));
  decltype(ReadIds()) res;

  string line;
  while (line_reader.Next(&line)) {
    StripWhiteSpace(&line);
    res.push_back(line);
    CHECK(!line.empty());
  }
  return res;
}

class HashTest : public testing::Test {
protected:
  uint32 C32(uint32 v) {
    return CityHash32(reinterpret_cast<const char*>(&v), sizeof(v));
  }

  uint64 C64(uint64 v) {
    return CityHash64(reinterpret_cast<const char*>(&v), sizeof(v));
  }
};

TEST_F(HashTest, Basic) {
  auto ids = ReadIds();
  ASSERT_GT(ids.size(), 10);
}

#if 0
TEST_F(HashTest, City32OutValues) {
  ASSERT_NE(0, C32(0));
  ASSERT_NE(1, C32(0));
  // 1005356300 maps to 0.
  for (uint32 i = 1; i != 0; ++i) {
    uint32 v = C32(i);

    ASSERT_NE(1, v) << i;
  }
}

#endif

DECLARE_BENCHMARK_FUNC(BM_MurMur, iters) {
  StopBenchmarkTiming();
  auto ids = ReadIds();
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    int j = i % ids.size();
    const auto* val = reinterpret_cast<const uint8*>(ids[j].data());
    sink_result(base::MurmurHash3_x86_32(val, ids[j].size(), i));
  }
}

DECLARE_BENCHMARK_FUNC(BM_City, iters) {
  StopBenchmarkTiming();
  auto ids = ReadIds();
  StartBenchmarkTiming();

  for (int i = 0; i < iters; ++i) {
    int j = i % ids.size();
    sink_result(CityHash64(ids[j].data(), ids[j].size()));
  }
}


}  // namespace base