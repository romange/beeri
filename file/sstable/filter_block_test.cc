// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/sstable/filter_block.h"

#include "file/sstable/filter_policy.h"
#include "base/gtest.h"
#include "base/hash.h"
#include "base/logging.h"
#include "strings/escaping.h"
#include "strings/stringpiece.h"
#include "util/coding/fixed.h"

namespace file {
namespace sstable {
using strings::Slice;

// For testing: emit an array with one hash value per key
class TestHashFilter : public FilterPolicy {
 public:
  virtual const char* Name() const override {
    return "TestHashFilter";
  }

  virtual void CreateFilter(const Slice* keys, uint32_t n, std::string* dst) const override {
    for (int i = 0; i < n; i++) {
      uint32_t h = base::MurmurHash3_x86_32(keys[i].data(), keys[i].size(), 1);
      coding::AppendFixed32(h, dst);
    }
  }

  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const override {
    uint32_t h = base::MurmurHash3_x86_32(key.data(), key.size(), 1);
    for (int i = 0; i + 4 <= filter.size(); i += 4) {
      if (h == coding::DecodeFixed32(filter.data() + i)) {
        return true;
      }
    }
    return false;
  }
};

inline bool KeyMayMatch(const FilterBlockReader& reader, uint32 offset, StringPiece str) {
  return reader.KeyMayMatch(offset, str.as_slice());
}

class FilterBlockTest : public testing::Test {
 public:
  FilterBlockTest() : builder_(&policy_) {}

  void AddKey(StringPiece key) {
    builder_.AddKey(key.as_slice());
  }

  TestHashFilter policy_;
  FilterBlockBuilder builder_;
};

TEST_F(FilterBlockTest, EmptyBuilder) {
  Slice block = builder_.Finish();
  ASSERT_EQ("\\x00\\x00\\x00\\x00\\x0b", strings::CHexEscape(block));
  FilterBlockReader reader(&policy_, block);
  ASSERT_TRUE(KeyMayMatch(reader, 0, Slice("foo")));
  ASSERT_TRUE(KeyMayMatch(reader, 100000, Slice("foo")));
}

TEST_F(FilterBlockTest, SingleChunk) {
  builder_.StartBlock(100);
  AddKey("foo");
  AddKey("bar");
  AddKey("box");
  builder_.StartBlock(200);
  AddKey("box");
  builder_.StartBlock(300);
  AddKey("hello");
  Slice block = builder_.Finish();
  FilterBlockReader reader(&policy_, block);
  ASSERT_TRUE(KeyMayMatch(reader, 100, "foo"));
  ASSERT_TRUE(KeyMayMatch(reader, 100, "bar"));
  ASSERT_TRUE(KeyMayMatch(reader, 100, "box"));
  ASSERT_TRUE(KeyMayMatch(reader, 100, "hello"));
  ASSERT_TRUE(KeyMayMatch(reader, 100, "foo"));
  ASSERT_TRUE(!KeyMayMatch(reader, 100, "missing"));
  ASSERT_TRUE(!KeyMayMatch(reader, 100, "other"));
}

TEST_F(FilterBlockTest, MultiChunk) {
  // First filter
  builder_.StartBlock(0);
  AddKey("foo");
  builder_.StartBlock(2000);
  AddKey("bar");

  // Second filter
  builder_.StartBlock(3100);
  AddKey("box");

  // Third filter is empty

  // Last filter
  builder_.StartBlock(9000);
  AddKey("box");
  AddKey("hello");

  Slice block = builder_.Finish();
  FilterBlockReader reader(&policy_, block);

  // Check first filter
  ASSERT_TRUE(KeyMayMatch(reader, 0, "foo"));
  ASSERT_TRUE(KeyMayMatch(reader, 2000, "bar"));
  ASSERT_TRUE(! KeyMayMatch(reader, 0, "box"));
  ASSERT_TRUE(! KeyMayMatch(reader, 0, "hello"));

  // Check second filter
  ASSERT_TRUE(KeyMayMatch(reader, 3100, "box"));
  ASSERT_TRUE(! KeyMayMatch(reader, 3100, "foo"));
  ASSERT_TRUE(! KeyMayMatch(reader, 3100, "bar"));
  ASSERT_TRUE(! KeyMayMatch(reader, 3100, "hello"));

  // Check third filter (empty)
  ASSERT_TRUE(! KeyMayMatch(reader, 4100, "foo"));
  ASSERT_TRUE(! KeyMayMatch(reader, 4100, "bar"));
  ASSERT_TRUE(! KeyMayMatch(reader, 4100, "box"));
  ASSERT_TRUE(! KeyMayMatch(reader, 4100, "hello"));

  // Check last filter
  ASSERT_TRUE(KeyMayMatch(reader, 9000, "box"));
  ASSERT_TRUE(KeyMayMatch(reader, 9000, "hello"));
  ASSERT_TRUE(! KeyMayMatch(reader, 9000, "foo"));
  ASSERT_TRUE(! KeyMayMatch(reader, 9000, "bar"));
}

}  // namespace sstable
}  // namespace file

