// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/bit_pack.h"

#include <gmock/gmock.h>
#include "base/bits.h"
#include "base/gtest.h"
#include "base/random.h"
#include "util/coding/fastpfor/util.h"

using testing::ElementsAreArray;

namespace util {
namespace coding {

class BitPackTest : public testing::Test {
protected:
  template <typename T> void TestEncoding(uint8 width, const std::vector<T>& vals) {
    uint8* next = BitPack(vals.data(), vals.size(), width, buf_);
    uint32 expected_size = PackedByteCount(vals.size(), width);
    ASSERT_EQ(expected_size, next - buf_);

    std::vector<T> decoded_vals(vals.size(), 0);
    const uint8* src_next = BitUnpack(buf_, vals.size(), width, &decoded_vals.front());
    EXPECT_EQ(src_next, next);
    EXPECT_THAT(decoded_vals, ElementsAreArray(vals)) << "Width " << uint32(width);
  }

  uint8 buf_[1000];
};

TEST_F(BitPackTest, Bits) {
  EXPECT_EQ(3,  Bits::Log2FloorNonZero(15));
  EXPECT_EQ(4,  Bits::Log2FloorNonZero(16));
}

TEST_F(BitPackTest, BitPack) {
  std::vector<uint32> vals({13, 12, 5, 1, 2, 9});
  {
    TestEncoding(4, vals);
    EXPECT_EQ(13 | (12 << 4), buf_[0]);
    EXPECT_EQ(5 | (1 << 4), buf_[1]);
    EXPECT_EQ(2 | (9 << 4), buf_[2]);
  }

  {
    TestEncoding(5, vals);
    EXPECT_EQ(uint8(13 | (12 << 5)), buf_[0]);
    EXPECT_EQ(uint8((12 >> 3) | (5 << 2) | (1 << 7)), buf_[1]);
  }
  for (uint8 w = 6; w <= 31; ++w) {
    TestEncoding(w, vals);
  }
  vals = {917, 4096, 17387, 300, 23101, 27486, 519, 3067};
  for (uint8 w = 15; w <= 31; ++w) {
    TestEncoding(w, vals);
  }
}

TEST_F(BitPackTest, BitPack64) {
  std::vector<uint64> vals{13, 12, 5, 1, 2, 9};
  TestEncoding(4, vals);
  TestEncoding(5, vals);
  for (uint8 w = 6; w <= 64; ++w) {
    TestEncoding(w, vals);
  }
  for (auto& v : vals) {
    v += (kuint32max*2);
  }
  TestEncoding(39, vals);
  vals = { 1ULL << 35, 1ULL << 36, 1ULL << 37, 1ULL << 45, 1ULL << 46, 1ULL << 47};
  TestEncoding(48, vals);
  TestEncoding(63, vals);
}


DECLARE_BENCHMARK_FUNC(BM_BitPack, iters) {
  StopBenchmarkTiming();
  MTRandom rand(10);
  std::vector<uint32> vals(iters, 0);
  for (uint32_t i = 0; i < iters; ++i) {
    vals[i] = rand.Rand32() % 29947;
  }
  constexpr uint8 kWidth = 15;
  std::vector<uint8> buf(PackedByteCount(iters, kWidth));
  StartBenchmarkTiming();
  BitPack(vals.data(), vals.size(), kWidth, &buf.front());
}

DECLARE_BENCHMARK_FUNC(BM_BitUnpack, iters) {
  StopBenchmarkTiming();
  MTRandom rand(10);
  std::vector<uint32> vals(iters, 0);
  for (uint32_t i = 0; i < iters; ++i) {
    vals[i] = rand.Rand32() % 29947;
  }
  constexpr uint8 kWidth = 15;
  std::vector<uint8> buf(PackedByteCount(iters, kWidth));
  BitPack(vals.data(), vals.size(), kWidth, &buf.front());
  StartBenchmarkTiming();
  BitUnpack(buf.data(), vals.size(), kWidth, &vals.front());
}

DECLARE_BENCHMARK_FUNC(BM_BitBsr, iters) {
  StopBenchmarkTiming();
  std::vector<uint32> vals(1000, 0);
  for (size_t i = 0; i < vals.size(); ++i)
    vals[i] = i * 173 + 1024*1023;
  StartBenchmarkTiming();
  for (size_t i = 0; i < iters; ++i) {
    for (uint32 v : vals)
      base::sink_result(asmbits(v));
  }
}

DECLARE_BENCHMARK_FUNC(BM_BitClz, iters) {
  StopBenchmarkTiming();
  std::vector<uint32> vals(1000, 0);
  for (size_t i = 0; i < vals.size(); ++i)
    vals[i] = i * 173 + 1024*1023;
  StartBenchmarkTiming();
  for (size_t i = 0; i < iters; ++i) {
    for (uint32 v : vals) {
      if (v) base::sink_result(Bits::Log2FloorNonZero(v) + 1);
    }
  }
}
}  // namespace coding
}  // namespace util