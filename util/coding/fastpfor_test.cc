// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/fastpfor/fastpfor.h"

#include <gmock/gmock.h>

#include "base/gtest.h"
#include "base/bits.h"
#include "base/logging.h"
#include "base/macros.h"
#include "file/file_util.h"
#include "file/filesource.h"
#include "strings/numbers.h"
#include "strings/stringprintf.h"

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

class FastPforTest : public testing::Test {
protected:
  vector<uint32> Decode(size_t original_size, const vector<uint32>& buf) {
    std::vector<uint32> decoded;
    decoded.resize(FastPFor::uncompressedLength(buf.data(), buf.size()));
    size_t out_length = decoded.size();
    EXPECT_EQ(out_length, original_size);
    coder_.decodeArray(buf.data(), buf.size(), &decoded.front(), out_length);
    EXPECT_EQ(original_size, out_length);
    return decoded;
  }

  FastPFor coder_;
  std::vector<uint32> buf_;
};

TEST_F(FastPforTest, Basic) {
  std::vector<uint32> vals({ 5, 5, 5, 7, 1, 1 << 15});
  buf_.resize(100);
  size_t cnt = buf_.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf_[0], cnt);
  EXPECT_EQ(3, cnt);

  buf_.resize(cnt);
  std::vector<uint32> decoded = Decode(vals.size(), buf_);
  EXPECT_EQ(vals, decoded);
}

TEST_F(FastPforTest, PFor) {
  vector<uint32> vals;
  for (int i = 0; i < 1024; ++i) {
    vals.push_back(417 + i % 16);
  }
  buf_.resize(2048);
  size_t cnt = buf_.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf_[0], cnt);
  // EXPECT_EQ(108, cnt);

  buf_.resize(cnt);
  std::vector<uint32> decoded = Decode(vals.size(), buf_);
  EXPECT_EQ(vals, decoded);
}

TEST_F(FastPforTest, SmallNumbers) {
  vector<uint32> vals = LoadUInt32("testdata/small_numbers.txt", 100000);
  buf_.resize(coder_.maxCompressedLength(vals.size()));
  size_t cnt = buf_.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf_.front(), cnt);
  EXPECT_LT(cnt * sizeof(uint32), 19000);

  buf_.resize(cnt);
  std::vector<uint32> decoded = Decode(vals.size(), buf_);
  ASSERT_EQ(vals, decoded);
}

TEST_F(FastPforTest, Medium1) {
  vector<uint32> vals = LoadUInt32("testdata/medium1.txt");
  buf_.resize(coder_.maxCompressedLength(vals.size()));
  size_t cnt = buf_.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf_.front(), cnt);
  EXPECT_LT(cnt, buf_.size());
  buf_.resize(cnt);
  std::vector<uint32> decoded = Decode(vals.size(), buf_);
  ASSERT_EQ(vals, decoded);
}

TEST_F(FastPforTest, BigNums) {
  std::vector<uint32> vals(4096, kuint32max);

  EXPECT_LT(coder_.maxCompressedLength(vals.size()), 4700);
  std::vector<uint32> buf(coder_.maxCompressedLength(vals.size()));
  size_t cnt = buf.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf.front(), cnt);

  buf.resize(cnt);
  vector<uint32> decoded = Decode(vals.size(), buf);
  EXPECT_EQ(vals, decoded);
  EXPECT_EQ(240, cnt * sizeof(uint32));
}

TEST_F(FastPforTest, Zeroes) {
  std::vector<uint32> vals(128, 0);
  std::vector<uint32> buf(coder_.maxCompressedLength(vals.size()));
  size_t cnt = buf.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf.front(), cnt);
  EXPECT_EQ(20, cnt * sizeof(uint32));
  buf.resize(cnt);
  vector<uint32> decoded = Decode(vals.size(), buf);
  EXPECT_EQ(vals, decoded);
}

TEST_F(FastPforTest, LsbShifted) {
  std::vector<uint32> vals(128, 0);
  for (size_t i = 0; i < 100; ++i) {
    vals[i] = (i + 10)* 8;
  }
  for (size_t i = 101; i < vals.size(); ++i) {
    vals[i] = i * 16;
  }

  std::vector<uint32> buf(coder_.maxCompressedLength(vals.size()));
  size_t cnt = buf.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf.front(), cnt);
  buf.resize(cnt);
  vector<uint32> decoded = Decode(vals.size(), buf);
  EXPECT_EQ(vals, decoded);
}

TEST_F(FastPforTest, Num64) {
  string file = base::ProgramRunfile("testdata/numbers64.txt.gz");
  file::File* fl = file_util::OpenOrDie(file, "r");
  util::Source* source = file::Source::Uncompressed(fl);
  file::LineReader reader(source, TAKE_OWNERSHIP);
  string line;
  std::vector<uint32> vals;
  while (reader.Next(&line)) {
    uint64 num = 0;
    CHECK(safe_strtou64_base(line, &num, 16));
    uint32 n32 = num >> 32;
    vals.push_back(n32);
  }
  std::vector<uint32> buf(coder_.maxCompressedLength(vals.size()));
  size_t cnt = buf.size();
  coder_.encodeArray(vals.data(), vals.size(), &buf.front(), cnt);
  LOG(INFO) << "cnt " << cnt << " input size: " << vals.size();
}

DECLARE_BENCHMARK_FUNC(BM_EncodeSmall, iters) {
  StopBenchmarkTiming();
  vector<uint32> vals = LoadUInt32("testdata/small_numbers.txt", 65536);
  FastPFor coder;
  CHECK_GT(vals.size(), 1000);
  std::vector<uint32> buf(coder.maxCompressedLength(vals.size()));
  size_t cnt = buf.size();
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    coder.encodeArray(vals.data(), vals.size(), &buf.front(), cnt);
  }
}

}  // namespace coding
}  // namespace util