// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/int_coder.h"

#include <gmock/gmock.h>

#include "base/gtest.h"
#include "base/logging.h"
#include "base/random.h"
#include "file/filesource.h"
#include "strings/numbers.h"
#include "util/coding/varint.h"

using testing::ElementsAreArray;
using namespace std;

namespace util {
namespace coding {

static vector<uint32> LoadUInt32(const string& name, size_t limit = kuint32max) {
  string file = base::ProgramRunfile(name);
  file::LineReader reader(file);
  string line;
  std::vector<uint32> vals;
  uint32_t index = 0;
  while (reader.Next(&line)) {
    uint32 num = 0;
    CHECK(safe_strtou32_base(line, &num, 16));
    vals.push_back(num);
    if (++index == limit)
      break;
  }
  return vals;
}

class CodingTest : public testing::Test {
protected:
  UInt32Decoder get_decoder() {
    return UInt32Decoder(buf_.data(), buf_.size());
  }

  void PushBit(bool b, uint32 count) {
    for (uint32 i = 0; i < count; ++i) bit_array_.Push(b);
  }

  void Push32(uint32 v) { values_.push_back(v); }

  uint32 Finalize() {
    UInt32Encoder encoder;
    encoder.Encode(values_, true);
    encoder.Swap(&buf_);
    repeated_overhead_ = encoder.repeated_overhead();
    delta_overhead_ = encoder.delta_overhead();
    direct_overhead_ = encoder.direct_overhead();
    return encoder.header_overhead();
  }

  std::vector<uint8> buf_;
  BitArray bit_array_;
  vector<uint32> values_;
  uint32 repeated_overhead_ = 0, delta_overhead_ = 0, direct_overhead_ = 0;
};

TEST_F(CodingTest, Basic) {
  Push32(5);
  Push32(5);
  Push32(5);
  Push32(7);
  Push32(1 << 15);
  Finalize();

  /*const auto& buffer = encoder_.buffer();
  ASSERT_EQ(5, buffer.size());
  EXPECT_EQ(0B00000001, buffer[0]);  // Binary notation that suprisingly works.
  EXPECT_EQ(5, buffer[1]);
  EXPECT_EQ(0B00001100, buffer[2]);
  EXPECT_EQ(0, buffer[3]);
  EXPECT_EQ(6, buffer[4]);
  */
  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    EXPECT_EQ(5, val);
  }
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(7, val);
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(1 << 15, val);
  LOG(INFO) << "END";
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Basic64) {
  UInt64Encoder encoder;
  std::vector<uint64> values;
  const uint64 kBase = uint64(kuint32max) << 24;
  values.push_back(kBase + 5);
  values.push_back(kBase + 5);
  values.push_back(kBase + 5);
  values.push_back(kBase + 6);
  ASSERT_EQ(values.size(), encoder.Encode(values.data(), values.size(), true));

  // const auto& buffer = encoder.buffer();
  // EXPECT_EQ(1 + Varint::Length64(kBase) + 2 + 7, buffer.size());
  /* EXPECT_EQ(0B00000001, buffer[0]);  // Binary notation that suprisingly works.
  uint64 tmp;
  const uint8* next = Varint::Parse(buffer.data() + 1, &tmp);
  ASSERT_TRUE(next != nullptr);
  EXPECT_EQ(kBase + 5, tmp);
  EXPECT_EQ(56 << 2, *next++);*/

  util::StringSink ssink;
  ASSERT_TRUE(encoder.SerializeTo(&ssink).ok());
  strings::Slice slice(ssink.contents());
  UInt64Decoder decoder(slice.data(), slice.size());
  uint64 val;
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    EXPECT_EQ(kBase + 5, val);
  }
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(kBase + 6, val);
  LOG(INFO) << "END";
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Sequential64) {
  UInt64Encoder encoder;
  std::vector<uint64> values;
  const uint64 kBase = uint64(kuint32max) << 24;
  values.push_back(kBase + 1000);
  values.push_back(kBase + 800);
  for (int i = 0; i < 100; ++i)
    values.push_back(kBase + 270 + i*5);
  ASSERT_EQ(values.size(), encoder.Encode(values.data(), values.size(), true));
  util::StringSink ssink;
  ASSERT_TRUE(encoder.SerializeTo(&ssink).ok());
  strings::Slice slice(ssink.contents());

  // 5 bytes for direct chunk and 6 for delta,2 base,repeated, cnt, val.
  EXPECT_EQ(28, slice.size());

  UInt64Decoder decoder(slice.data(), slice.size());
  uint64 val;
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(kBase + 1000, val);
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(kBase + 800, val);
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(kBase + 270 + i*5, val) << i;
  }
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Direct) {
  uint32 vals[] = {5, 3, 7, 4};
  for (int i = 0; i < arraysize(vals); ++i) {
    Push32(vals[i]);
  }
  Finalize();

  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (int i = 0; i < arraysize(vals); ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    EXPECT_EQ(vals[i], val);
  }
  EXPECT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Sequential) {
  Push32(1000);
  Push32(800);
  for (int i = 0; i < 100; ++i)
    Push32(270 + i*5);
  Finalize();

  const auto& buffer = buf_;
  size_t expected = 11;  // 5 bytes for direct chunk and 6 for delta,2 base,repeated, cnt, val.
  EXPECT_EQ(expected, buffer.size());

  UInt32Decoder decoder = get_decoder();
  uint32 val;
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(1000, val);
  ASSERT_TRUE(decoder.Next(&val));
  EXPECT_EQ(800, val);
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(270 + i*5, val);
  }
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Repeated) {
  for (uint32 k = 0; k < 1000; ++k) {
     Push32(16543);
  }
  for (uint32 k = 0; k < 5; ++k) {
     Push32(18);
  }
  Finalize();

  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (uint32 k = 0; k < 1000; ++k) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(16543, val);
  }
  for (uint32 k = 0; k < 5; ++k) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(18, val);
  }
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, DirectLong) {
  for (uint32 i = 0; i < 200000; ++i) {
    Push32(1024 + 10 * (i % 2));
  }
  Finalize();

  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (uint32 k = 0; k < 200000; ++k) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(1024 + 10 * (k % 2), val) << k;
  }
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, DeltaLong) {
  for (uint32 i = 0; i < 100000; ++i) {
    Push32(i*i);
  }
  Finalize();

  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (uint32 k = 0; k < 100000; ++k) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(k*k, val);
  }
  ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Long) {
  for (uint32 i = 0; i < 100; ++i) {
    for (uint32 k = 0; k < 100; ++k) {
      Push32(i*100 + k);
    }
    for (uint32 k = 0; k < 1000; ++k) {
      Push32(16543);
    }
    for (uint32 k = 0; k < 20; ++k) {
      Push32(18);
    }
    for (uint32 k = 0; k < 10; ++k) {
      Push32(i*100 + k);
    }
  }
  uint32 ho = Finalize();
  const auto& buffer = buf_;
  LOG(INFO) << "Long buf takes " << buffer.size() << " bytes for " << 100*(100 + 1000 + 20 + 10)
            << " items";
  EXPECT_LT(ho, buf_.size() / 10);
  UInt32Decoder decoder(buffer.data(), buffer.size());
  uint32 val = 0;
  for (uint32 i = 0; i < 100; ++i) {
    for (uint32 k = 0; k < 100; ++k) {
      ASSERT_TRUE(decoder.Next(&val));
      ASSERT_EQ(i*100 + k, val) << "k " << k << " i " << i;
    }
    for (uint32 k = 0; k < 1000; ++k) {
      ASSERT_TRUE(decoder.Next(&val));
      ASSERT_EQ(16543, val);
    }
    for (uint32 k = 0; k < 20; ++k) {
      ASSERT_TRUE(decoder.Next(&val));
      ASSERT_EQ(18, val) << "k " << k << " i " << i;
    }
    for (uint32 k = 0; k < 10; ++k) {
      ASSERT_TRUE(decoder.Next(&val));
      ASSERT_EQ(i*100 + k, val) << "k " << k << " i " << i;
    }
  }
   ASSERT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Delta32) {
  uint32 arr[] = {3,2,2,3,3,3,3,2,3,3,3,3,3,3,2,2,3,3,3,2,2,3,2};
  for (uint32 k = 0; k < arraysize(arr); ++k) {
    Push32(arr[k]);
  }
  uint32 ho = Finalize();
  EXPECT_LT(ho, 6);

  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (int i = 0; i < arraysize(arr); ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    EXPECT_EQ(arr[i], val);
  }
  EXPECT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, SmallNumbers) {
  values_ = LoadUInt32("testdata/small_numbers.txt");
  uint32 ho = Finalize();
  EXPECT_LT(ho, 700);
  EXPECT_LT(buf_.size(), 21100);
  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (int i = 0; i < values_.size(); ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(values_[i], val) << i;
  }
  EXPECT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, Medium2) {
  values_ = LoadUInt32("testdata/medium2.txt");
  uint32 ho = Finalize();
  EXPECT_LT(ho, 1400);
  EXPECT_LT(direct_overhead_, 1000);
  EXPECT_LT(buf_.size(), 125000);
  UInt32Decoder decoder = get_decoder();
  uint32 val;
  for (int i = 0; i < values_.size(); ++i) {
    ASSERT_TRUE(decoder.Next(&val));
    ASSERT_EQ(values_[i], val) << i;
  }
  EXPECT_FALSE(decoder.Next(&val));
}

TEST_F(CodingTest, BitArray) {
  // 1 literal word
  for (uint32 i = 0; i < 31; ++i) {
    bit_array_.Push((i & 1) == 1);
  }
  // 1 fill word
  PushBit(false, 310);
  // 1 fill word with diff position.
  PushBit(true, 333);
  PushBit(false, 1);
  // 1 fill word
  PushBit(true, 10000);
  // 1 literal + 1 fill + remainder literal.
  PushBit(false, 5000);
  bit_array_.Finalize();
  uint32 count = 0;
  for (uint32 i = 0; i < 31; ++i, ++count) {
    ASSERT_EQ((i & 1) == 1, bit_array_.Get(i)) << count;
  }
  for (uint32 i = 0; i < 310; ++i, ++count) {
    ASSERT_FALSE(bit_array_.Get(count)) << count;
  }
  for (uint32 i = 0; i < 333; ++i, ++count) {
    ASSERT_TRUE(bit_array_.Get(count)) << count;
  }
  ASSERT_FALSE(bit_array_.Get(count++)) << count;
  for (uint32 i = 0; i < 10000; ++i, ++count) {
    ASSERT_TRUE(bit_array_.Get(count)) << count;
  }
  for (uint32 i = 0; i < 5000; ++i, ++count) {
    ASSERT_FALSE(bit_array_.Get(count)) << count;
  }
  EXPECT_EQ(7*sizeof(uint32), bit_array_.ByteSize());
  const auto& data = bit_array_.data();
  uint32 val = 0;
  for (uint32 i = 0; i < 31; ++i)
    val |= ((i & 1) << i);
  EXPECT_EQ(val, data[0]);
  EXPECT_EQ(2U << 30 | 9, data[1]);

  val = (3U << 30) | 9 | (24 << 25);
  EXPECT_EQ(val, data[2]);
}

TEST_F(CodingTest, LargeCounts) {
  PushBit(false, 300);
  PushBit(true, (1 << 25) * 11 + 10);
  bit_array_.Finalize();
  auto iter = bit_array_.begin();
  for (uint32 i = 0; i < 300; ++i, ++iter) {
    ASSERT_FALSE(iter.Done());
    ASSERT_FALSE(*iter);
  }
  for (; !iter.Done(); ++iter) {
    ASSERT_TRUE(*iter);
  }
}

TEST_F(CodingTest, Push500) {
  PushBit(true, 500);
  bit_array_.Finalize();
  ASSERT_EQ(2*sizeof(uint32), bit_array_.ByteSize());
}

TEST_F(CodingTest, BitArrayIterator) {
  for (uint32 i = 0; i < 100000; ++i) {
    bit_array_.Push((i % 500) != 499);
  }
  uint32 count = 0;
  for (auto i = bit_array_.begin(); !i.Done(); ++i, ++count) {
    ASSERT_EQ((count % 500) != 499, *i) << count;
  }
  EXPECT_EQ(100000, count);
}

DECLARE_BENCHMARK_FUNC(BM_MemCopy, iters) {
  StopBenchmarkTiming();
  MTRandom rand(10);
  std::vector<uint32> vals(iters, 0);
  for (uint32_t i = 0; i < iters; ++i) {
    vals[i] = rand.Rand32() % 29947;
  }
  std::vector<uint32> copy(iters, 0);
  StartBenchmarkTiming();
  memcpy(&copy.front(), vals.data(), vals.size()*sizeof(uint32));
}

DECLARE_BENCHMARK_FUNC(BM_MemMove, iters) {
  StopBenchmarkTiming();
  MTRandom rand(10);
  std::vector<uint32> vals(iters, 0);
  for (uint32_t i = 0; i < iters; ++i) {
    vals[i] = rand.Rand32() % 29947;
  }
  std::vector<uint32> copy(iters, 0);
  StartBenchmarkTiming();
  memmove(&copy.front(), vals.data(), vals.size()*sizeof(uint32));
}

DECLARE_BENCHMARK_FUNC(BM_BitArrayPushDense, iters) {
  BitArray bit_array;
  for (uint32_t i = 0; i < iters; ++i) {
    bit_array.Push(i & 1);
  }
}

DECLARE_BENCHMARK_FUNC(BM_BitArrayPushSparse, iters) {
  BitArray bit_array;
  for (uint32_t i = 0; i < iters; ++i) {
    bit_array.Push((i % 256) != 0);
  }
}

DECLARE_BENCHMARK_FUNC(BM_BitArrayPushConst, iters) {
  BitArray bit_array;
  for (uint32_t i = 0; i < iters; ++i) {
    bit_array.Push(true);
  }
}

}  // namespace coding
}  // namespace util