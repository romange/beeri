// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/lmdb/disk_table.h"

#include <gtest/gtest.h>
#include "file/file_util.h"
#include "strings/numbers.h"
#include "strings/serialize.h"
#include "strings/strcat.h"

namespace util {
using strings::Slice;

static const char kDataDir[] = "./dbdir";
class DiskTableTest : public testing::Test {
  void SetUp() {
    file_util::DeleteRecursively(kDataDir);
  }
protected:
  void Put(uint64 i, string val) {
    table_->Put(Uint64ToKey(i), val);
  }

  string Get(uint64 i) {
    Slice val;
    if (table_->Get(Uint64ToKey(i), &val)) {

    } {
      return string();
    }
  }
  std::unique_ptr<DiskTable> table_;
};

TEST_F(DiskTableTest, Basic) {
  table_.reset(new DiskTable(60));
  auto status = table_->Open(kDataDir);
  ASSERT_TRUE(status.ok()) << status;
  for (int64 i = 0; i < 1000000; ++i) {
    status = table_->BeginTransaction();
    ASSERT_TRUE(status.ok()) << status;
    status = table_->Put(StrCat("key", i), StrCat("val", i));
    ASSERT_TRUE(status.ok()) << status;
    status = table_->CommitTransaction();
    ASSERT_TRUE(status.ok()) << status << " " << i;
  }
  ASSERT_TRUE(table_->BeginTransaction().ok());
  Slice val;
  ASSERT_TRUE(table_->Get(Slice("key12"), &val));
  EXPECT_EQ("val12", val.as_string());
  ASSERT_FALSE(table_->Get(Slice("key-12"), &val));

  ASSERT_TRUE(table_->Get(Slice("key1222"), &val));
  EXPECT_EQ("val1222", val.as_string());
  table_->AbortTransaction();
}

TEST_F(DiskTableTest, Batch) {
  table_.reset(new DiskTable(150));
  auto status = table_->Open(kDataDir);
  ASSERT_TRUE(status.ok()) << status;
  for (int64 i = 0; i < 100; ++i) {
    ASSERT_TRUE(table_->BeginTransaction().ok());
    for (int64 j = 0; j < 10000; ++j) {
      status = table_->Put(StrCat("key", i*10000+j), StrCat("val", i*10000+j));
      ASSERT_TRUE(status.ok()) << status << " " << i;
    }
    status = table_->CommitTransaction();
      ASSERT_TRUE(status.ok()) << status;
    }
}

TEST_F(DiskTableTest, Iterator) {
  table_.reset(new DiskTable(150, true));
  auto status = table_->Open(kDataDir);
  ASSERT_TRUE(status.ok()) << status;
  ASSERT_TRUE(table_->BeginTransaction().ok());
  table_->Put(Slice("key3"), Slice("val3"));
  table_->Put(Slice("key4"), Slice("val4"));
  table_->Put(Slice("key2"), Slice("val2"));
  table_->Put(Slice("key1"), Slice("val1"));
  status = table_->CommitTransaction();
  ASSERT_TRUE(status.ok()) << status;

  CHECK(table_->BeginTransaction().ok());
  DiskTable::Iterator it1, it2;
  it1 = table_->GetIterator();
  Slice key, val;
  for (int i = 1; i < 5; ++i) {
    ASSERT_TRUE(it1.Next(&key, &val));
    EXPECT_EQ(StrCat("key", i), key.as_string());
    EXPECT_EQ(StrCat("val", i), val.as_string());
  }
  ASSERT_TRUE(it1.First(&key, &val));
  EXPECT_EQ("key1", key.as_string());
  ASSERT_TRUE(it1.Next(&key, &val));
  EXPECT_EQ("key2", key.as_string());
  table_->AbortTransaction();
}


}  // namespace util