// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/pb_writer.h"
#include "util/coding/pb_reader.h"
#include <gmock/gmock.h>
#include "base/gtest.h"
#include "file/file.h"
#include "strings/numbers.h"
#include "util/plang/addressbook.pb.h"
#include "util/sinksource.h"

using namespace tutorial;

MATCHER_P(EqualProto, p, "") {
  *result_listener << " where the expected pb is: " << p.ShortDebugString()
                   << "but actual is " << arg.ShortDebugString();
  return arg.SerializeAsString() == p.SerializeAsString();
}

namespace util {
namespace coding {

class PbSerializerTest : public testing::Test {
public:

};

TEST_F(PbSerializerTest, BankAccount) {
  BankAccount account;
  for (unsigned j = 0; j < 1000; ++j) {
    account.add_activity_id(46789 + j * 173);
  }
  PbBlockSerializer writer(BankAccount::descriptor());
  util::StringSink ssink;
  writer.SerializeTo(&ssink);
  EXPECT_LT(ssink.contents().size(), 20);
}

TEST_F(PbSerializerTest, Basic) {
  AddressBook book;
  Person* p = book.add_person();
  for (uint64 i = 0; i < 20; ++i) {
    uint64 v = (uint64(kuint32max) << 16) + i * 1024*1024;
    book.add_tmp(v);
  }
  for (int64 i = 0; i < 20; ++i) {
    int64 v = (int64(kint32max) << 24) + i * 8192;
    book.add_ts(v);
  }
  p->set_id(1234567891234ULL);
  p->set_name("Jessika Kapara");
  p->set_email("jessika@alba.com");
  for (int j = 0; j < 20; ++j) {
    auto* phone = p->add_phone();
    phone->set_number(IntToString(j));
  }
  PbBlockSerializer writer(AddressBook::descriptor());
  const uint64 kBase = 1234567891234ULL;
  for (int j = 0; j < 500; ++j) {
    book.mutable_person(0)->set_id(kBase + j);
    writer.Add(book);
  }
  util::StringSink ssink;
  writer.SerializeTo(&ssink);
  strings::Slice contents(ssink.contents());
  file::FileCloser fc(file::Open("sink.bin", "w"));
  uint64 w = 0;
  CHECK(fc->Write(contents.data(), contents.size(), &w).ok());
  EXPECT_EQ(604, book.ByteSize());
  // EXPECT_EQ(0, contents.size());

  PbBlockDeserializer reader(AddressBook::descriptor());
  uint32 num_msgs = 0;
  auto st = reader.Init(contents, &num_msgs);
  ASSERT_TRUE(st.ok()) << st;
  ASSERT_EQ(500, num_msgs);

  for (int j = 0; j < 500; ++j) {
    AddressBook actual;
    st = reader.Read(&actual);
    ASSERT_TRUE(st.ok()) << st << " " << j;
    book.mutable_person(0)->set_id(kBase + j);
    ASSERT_THAT(actual, EqualProto(book));
  }
}

}  // namespace coding
}  // namespace util
