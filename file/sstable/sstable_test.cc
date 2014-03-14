// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/sstable/sstable.h"

#include <map>
#include <string>
#include <snappy-c.h>
#include <gmock/gmock.h>

#include "base/gtest.h"
#include "base/random.h"
#include "file/sstable/iterator.h"
#include "file/sstable/sstable_builder.h"
#include "file/sstable/block.h"
#include "file/sstable/block_builder.h"
#include "file/sstable/format.h"
#include "util/sinksource.h"
#include "file/test_util.h"
#include "strings/stringpiece.h"
#include "strings/escaping.h"
#include "strings/stringprintf.h"

namespace file {
namespace sstable {

using strings::Slice;
using base::Status;
using ::testing::UnorderedElementsAre;
namespace {

string RandomString(RandomBase* rnd, unsigned len) {
  std::string dst(len, '\0');
  for (int i = 0; i < len; i++) {
    dst[i] = static_cast<char>(' ' + rnd->Rand32() % 95);   // ' ' .. '~'
  }
  return dst;
}

std::string RandomKey(RandomBase* rnd, int len) {
  // Make sure to generate a wide variety of characters so we
  // test the boundary conditions for short-key optimizations.
  static const char kTestChars[] = {
    '\0', '\1', 'a', 'b', 'c', 'd', 'e', '\xfd', '\xfe', '\xff'
  };
  std::string result;
  for (int i = 0; i < len; i++) {
    result += kTestChars[rnd->Rand8() % sizeof(kTestChars)];
  }
  return result;
}

static void Increment(std::string* key) {
  key->push_back('\0');
}

string CompressibleString(RandomBase* rnd, double compressed_fraction,
                          unsigned len) {
  unsigned raw = len * compressed_fraction;
  if (raw < 1) raw = 1;
  std::string raw_data = RandomString(rnd, raw);

  // Duplicate the random data until we have filled "len" bytes
  std::string dst;
  while (dst.size() < len) {
    dst.append(raw_data);
  }
  dst.resize(len);
  return dst;
}

typedef std::map<std::string, std::string> KVMap;

// Helper class for tests to unify the interface between
// BlockBuilder/TableBuilder and Block/Table.
class Constructor {
 public:
  explicit Constructor() { }
  virtual ~Constructor() { }

  void Add(const std::string& key, const string& value) {
    data_[key] = value;
  }

  // Finish constructing the data structure with all the keys that have
  // been added so far.  Returns the keys in sorted order in "*keys"
  // and stores the key/value pairs in "*kvmap"
  void Finish(const Options& options,
              std::vector<std::string>* keys,
              KVMap* kvmap) {
    *kvmap = data_;
    keys->clear();
    for (KVMap::const_iterator it = data_.begin();
         it != data_.end();
         ++it) {
      keys->push_back(it->first);
    }
    data_.clear();
    FinishImpl(options, *kvmap);
  }

  // Construct the data structure from the data in "data"
  virtual void FinishImpl(const Options& options, const KVMap& data) = 0;

  virtual Iterator* NewIterator() const = 0;

  virtual const KVMap& data() { return data_; }

 private:
  KVMap data_;
};

class BlockConstructor: public Constructor {
 public:
  BlockConstructor(): block_(NULL) { }
  ~BlockConstructor() {
    delete block_;
  }
  virtual void FinishImpl(const Options& options, const KVMap& data) override {
    delete block_;
    block_ = NULL;
    BlockBuilder builder(&options);

    for (KVMap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.Add(it->first, it->second);
    }
    // Open the block
    data_ = builder.Finish().as_string();
    BlockContents contents;
    contents.data = data_;
    contents.cachable = false;
    contents.heap_allocated = false;
    block_ = new Block(contents);
  }

  virtual Iterator* NewIterator() const {
    return block_->NewIterator();
  }

 private:
  std::string data_;
  Block* block_;
};

class TableConstructor: public Constructor {
 public:
  TableConstructor() : source_(NULL), table_(NULL) {
  }

  ~TableConstructor() {
    Reset();
  }

  virtual void FinishImpl(const Options& options, const KVMap& data) override {
    Reset();
    util::StringSink sink;
    TableBuilder builder(options, &sink);

    for (KVMap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.Add(it->first, it->second);
      ASSERT_TRUE(builder.status().ok());
    }
    Status s = builder.Finish();
    ASSERT_TRUE(s.ok()) << s.ToString();

    ASSERT_EQ(sink.contents().size(), builder.FileSize());

    // Open the table
    source_ = new ReadonlyStringFile(sink.contents());
    auto res = Table::Open(ReadOptions(), source_);
    ASSERT_TRUE(res.status.ok()) << res.status;
    table_ = res.obj;
  }

  virtual Iterator* NewIterator() const {
    return table_->NewIterator();
  }

  uint64_t ApproximateOffsetOf(const string& key) const {
    return table_->ApproximateOffsetOf(Slice(key));
  }

 private:
  void Reset() {
    delete table_;
    delete source_;
    table_ = NULL;
    source_ = NULL;
  }

  file::ReadonlyStringFile* source_;
  Table* table_;
};


enum TestType {
  TABLE_TEST,
  BLOCK_TEST,
};

struct TestArgs {
  TestType type;
  int restart_interval;
};

static const TestArgs kTestArgList[] = {
  { TABLE_TEST, 16 },
  { TABLE_TEST, 1 },
  { TABLE_TEST, 1024 },
  { TABLE_TEST, 16 },
  { TABLE_TEST, 1 },
  { TABLE_TEST, 1024 },

  { BLOCK_TEST, 16 },
  { BLOCK_TEST, 1 },
  { BLOCK_TEST, 1024 },
  { BLOCK_TEST, 16 },
  { BLOCK_TEST, 1 },
  { BLOCK_TEST, 1024 },
};

static const int kNumTestArgs = sizeof(kTestArgList) / sizeof(kTestArgList[0]);

}  // namespace

class Harness : public ::testing::Test {
 public:
  Harness() : constructor_(NULL) { }

  void Init(const TestArgs& args) {
    delete constructor_;
    constructor_ = NULL;
    options_ = Options();

    options_.block_restart_interval = args.restart_interval;
    // Use shorter block size for tests to exercise block boundary
    // conditions more.
    options_.block_size = 256;

    switch (args.type) {
      case TABLE_TEST:
        constructor_ = new TableConstructor();
        break;
      case BLOCK_TEST:
        constructor_ = new BlockConstructor();
        break;
      default:
        LOG(FATAL) << "Invalid type";
    }
  }

  ~Harness() {
    delete constructor_;
  }

  void Add(const std::string& key, const std::string& value) {
    constructor_->Add(key, value);
  }

  void Test(RandomBase* rnd) {
    std::vector<std::string> keys;
    KVMap data;
    constructor_->Finish(options_, &keys, &data);

    TestForwardScan(keys, data);
    TestBackwardScan(keys, data);
    TestRandomAccess(rnd, keys, data);
  }

  void TestForwardScan(const std::vector<std::string>& keys,
                       const KVMap& data) {
    Iterator* iter = constructor_->NewIterator();
    ASSERT_TRUE(!iter->Valid());
    iter->SeekToFirst();
    for (KVMap::const_iterator model_iter = data.begin();
         model_iter != data.end();
         ++model_iter) {
      string expected = ToString(data, model_iter);
      ASSERT_EQ(expected, ToString(iter));
      //CHECK_EQ(expected, ToString(iter));
      iter->Next();
    }
    ASSERT_TRUE(!iter->Valid());
    delete iter;
  }

  void TestBackwardScan(const std::vector<std::string>& keys,
                        const KVMap& data) {
    Iterator* iter = constructor_->NewIterator();
    ASSERT_TRUE(!iter->Valid());
    iter->SeekToLast();
    for (KVMap::const_reverse_iterator model_iter = data.rbegin();
         model_iter != data.rend();
         ++model_iter) {
      ASSERT_EQ(ToString(data, model_iter), ToString(iter));
      iter->Prev();
    }
    ASSERT_TRUE(!iter->Valid());
    delete iter;
  }

  void TestRandomAccess(RandomBase* rnd,
                        const std::vector<std::string>& keys,
                        const KVMap& data) {
    static const bool kVerbose = false;
    Iterator* iter = constructor_->NewIterator();
    ASSERT_TRUE(!iter->Valid());
    KVMap::const_iterator model_iter = data.begin();
    LOG(INFO) << "---\n";
    for (int i = 0; i < 200; i++) {
      const int toss = rnd->Rand8() % 5;
      switch (toss) {
        case 0: {
          if (iter->Valid()) {
            if (kVerbose) fprintf(stderr, "Next\n");
            iter->Next();
            ++model_iter;
            ASSERT_EQ(ToString(data, model_iter), ToString(iter));
          }
          break;
        }

        case 1: {
          if (kVerbose) fprintf(stderr, "SeekToFirst\n");
          iter->SeekToFirst();
          model_iter = data.begin();
          ASSERT_EQ(ToString(data, model_iter), ToString(iter));
          break;
        }

        case 2: {
          std::string key = PickRandomKey(rnd, keys);
          model_iter = data.lower_bound(key);
          string hex_str = strings::CHexEscape(key);
          LOG(INFO) << i << " Seek '" << hex_str << "'";
          iter->Seek(Slice(key));
          string expected = ToString(data, model_iter);
          // CHECK_EQ(expected, ToString(iter));
          ASSERT_EQ(expected, ToString(iter));
          break;
        }

        case 3: {
          if (iter->Valid()) {
            if (kVerbose) fprintf(stderr, "Prev\n");
            iter->Prev();
            if (model_iter == data.begin()) {
              model_iter = data.end();   // Wrap around to invalid value
            } else {
              --model_iter;
            }
            ASSERT_EQ(ToString(data, model_iter), ToString(iter));
          }
          break;
        }

        case 4: {
          if (kVerbose) fprintf(stderr, "SeekToLast\n");
          iter->SeekToLast();
          if (keys.empty()) {
            model_iter = data.end();
          } else {
            std::string last = data.rbegin()->first;
            model_iter = data.lower_bound(last);
          }
          ASSERT_EQ(ToString(data, model_iter), ToString(iter));
          break;
        }
      }
    }
    delete iter;
  }

  std::string ToString(const KVMap& data, const KVMap::const_iterator& it) {
    if (it == data.end()) {
      return "END";
    } else {
      return "'" + it->first + "->" + it->second + "'";
    }
  }

  std::string ToString(const KVMap& data,
                       const KVMap::const_reverse_iterator& it) {
    if (it == data.rend()) {
      return "END";
    } else {
      return "'" + it->first + "->" + it->second + "'";
    }
  }

  std::string ToString(const Iterator* it) {
    if (!it->Valid()) {
      return "END";
    } else {
      return "'" + it->key().as_string() + "->" + it->value().as_string() + "'";
    }
  }

  std::string PickRandomKey(RandomBase* rnd, const std::vector<std::string>& keys) {
    if (keys.empty()) {
      return "foo";
    } else {
      const int index = rnd->Rand32() % keys.size();
      std::string result = keys[index];
      switch (rnd->Rand8() % 3) {
        case 0:
          // Return an existing key
          break;
        case 1: {
          // Attempt to return something smaller than an existing key
          if (result.size() > 0 && result[result.size()-1] > '\0') {
            result[result.size()-1]--;
          }
          break;
        }
        case 2: {
          // Return something larger than an existing key
          Increment(&result);
          break;
        }
      }
      return result;
    }
  }

 private:
  Options options_;
  Constructor* constructor_;
};

// Test empty table/block.
TEST_F(Harness, Empty) {
  for (int i = 0; i < kNumTestArgs; i++) {
    Init(kTestArgList[i]);
    MTRandom rnd(10 + 1);
    Test(&rnd);
  }
}

// Special test for a block with no restart entries.  The C++ leveldb
// code never generates such blocks, but the Java version of leveldb
// seems to.
TEST_F(Harness, ZeroRestartPointsInBlock) {
  char data[sizeof(uint32_t)];
  memset(data, 0, sizeof(data));
  BlockContents contents;
  contents.data = Slice(data, sizeof(data));
  contents.cachable = false;
  contents.heap_allocated = false;
  Block block(contents);
  Iterator* iter = block.NewIterator();
  iter->SeekToFirst();
  ASSERT_TRUE(!iter->Valid());
  iter->SeekToLast();
  ASSERT_TRUE(!iter->Valid());
  iter->Seek(Slice("foo"));
  ASSERT_TRUE(!iter->Valid());
  delete iter;
}

// Test the empty key
TEST_F(Harness, SimpleEmptyKey) {
  for (int i = 0; i < kNumTestArgs; i++) {
    Init(kTestArgList[i]);
    MTRandom rnd(10 + 1);
    Add("", "v");
    Test(&rnd);
  }
}

TEST_F(Harness, SimpleSingle) {
  for (int i = 0; i < kNumTestArgs; i++) {
    Init(kTestArgList[i]);
    MTRandom rnd(10 + 2);
    Add("abc", "v");
    Test(&rnd);
  }
}

TEST_F(Harness, SimpleMulti) {
  for (int i = 0; i < kNumTestArgs; i++) {
    Init(kTestArgList[i]);
    MTRandom rnd(10 + 3);
    Add("abc", "v");
    Add("abcd", "v");
    Add("ac", "v2");
    Test(&rnd);
  }
}

TEST_F(Harness, SimpleSpecialKey) {
  for (int i = 0; i < kNumTestArgs; i++) {
    Init(kTestArgList[i]);
    MTRandom rnd(10 + 4);
    Add("\xff\xff", "v3");
    Test(&rnd);
  }
}

TEST_F(Harness, Randomized) {
  for (int i = 0; i < kNumTestArgs; i++) {
    Init(kTestArgList[i]);
    MTRandom rnd(10 + 5);
    for (int num_entries = 0; num_entries < 2000;
         num_entries += (num_entries < 50 ? 1 : 200)) {
      if ((num_entries % 10) == 0) {
        LOG(INFO) << StringPrintf("case %d of %d: num_entries = %d\n",
                (i + 1), int(kNumTestArgs), num_entries);
      }
      for (int e = 0; e < num_entries; e++) {
        Add(RandomKey(&rnd, rnd.Skewed(4)), RandomString(&rnd, rnd.Skewed(5)));
      }
      Test(&rnd);
    }
  }
}

static bool Between(uint64_t val, uint64_t low, uint64_t high) {
  bool result = (val >= low) && (val <= high);
  if (!result) {
    LOG(INFO) << StringPrintf("Value %llu is not in range [%llu, %llu]\n",
            (unsigned long long)(val),
            (unsigned long long)(low),
            (unsigned long long)(high));
  }
  return result;
}

class TableTest : public ::testing::Test  {
protected:
  util::StringSink sink_;
};

TEST_F(TableTest, ApproximateOffsetOfPlain) {
  TableConstructor c;
  c.Add("k01", "hello");
  c.Add("k02", "hello2");
  c.Add("k03", std::string(10000, 'x'));
  c.Add("k04", std::string(200000, 'x'));
  c.Add("k05", std::string(300000, 'x'));
  c.Add("k06", "hello3");
  c.Add("k07", std::string(100000, 'x'));
  std::vector<std::string> keys;
  KVMap kvmap;
  Options options;
  options.block_size = 1024;
  options.compression = kNoCompression;
  c.Finish(options, &keys, &kvmap);

  ASSERT_TRUE(Between(c.ApproximateOffsetOf("abc"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k01"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k01a"),      0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k02"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k03"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k04"),   10000,  11000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k04a"), 210000, 211000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k05"),  210000, 211000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k06"),  510000, 511000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k07"),  510000, 511000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("xyz"),  610000, 612000));

}

static bool SnappyCompressionSupported() {
  StringPiece in("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  size_t sz_out = snappy_max_compressed_length(in.size());
  std::string out(sz_out, '\0');
  return snappy_compress(in.data(), in.size(), &out.front(), &sz_out) == SNAPPY_OK;
}

TEST_F(TableTest, ApproximateOffsetOfCompressed) {
  if (!SnappyCompressionSupported()) {
    fprintf(stderr, "skipping compression tests\n");
    return;
  }

  MTRandom rnd(301);
  TableConstructor c;
  std::string tmp;
  c.Add("k01", "hello");
  c.Add("k02", CompressibleString(&rnd, 0.25, 10000));
  c.Add("k03", "hello3");
  c.Add("k04", CompressibleString(&rnd, 0.25, 10000));
  std::vector<std::string> keys;
  KVMap kvmap;
  Options options;
  options.block_size = 1024;
  options.compression = kSnappyCompression;
  c.Finish(options, &keys, &kvmap);

  ASSERT_TRUE(Between(c.ApproximateOffsetOf("abc"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k01"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k02"),       0,      0));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k03"),    2000,   3000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("k04"),    2000,   3000));
  ASSERT_TRUE(Between(c.ApproximateOffsetOf("xyz"),    4000,   6000));
}

TEST_F(TableTest, MetaBlockTest) {
  TableBuilder builder(Options(), &sink_);
  builder.AddMeta("foo", Slice::FromCstr("bar"));
  builder.AddMeta("1", Slice::FromCstr("2"));
  builder.AddMeta("8", Slice::FromCstr("3"));
  ASSERT_TRUE(builder.Finish().ok());

  ReadonlyStringFile fl(sink_.contents());
  auto res = Table::Open(ReadOptions(), &fl);
  ASSERT_TRUE(res.status.ok()) << res.status;
  std::unique_ptr<Table> t(res.obj);
  const auto& meta = t->GetMeta();
  std::map<string, string> expected({{"foo", "bar"}, {"1", "2" }, {"8", "3"}});
  EXPECT_EQ(expected, meta);
}

TEST_F(TableTest, SeekToFirst) {
  TableBuilder builder(Options(), &sink_);
  builder.Add(Slice::FromCstr(""), Slice::FromCstr("bar"));
  builder.Flush();
  builder.Add(Slice::FromCstr("foo"), Slice::FromCstr("bar"));
  builder.Flush();
  ASSERT_TRUE(builder.Finish().ok());
  ReadonlyStringFile fl(sink_.contents());
  auto res = Table::Open(ReadOptions(), &fl);
  ASSERT_TRUE(res.status.ok()) << res.status;
  std::unique_ptr<Table> t(res.obj);
  std::unique_ptr<Iterator> it(t->NewIterator());
  it->SeekToFirst();
  ASSERT_TRUE(it->Valid());
  it->SeekToLast();
  ASSERT_TRUE(it->Valid());
}

}  // namespace sstable
}  // namespace file