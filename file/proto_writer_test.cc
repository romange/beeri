// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "file/proto_writer.h"
#include "file/proto_writer_test.pb.h"
#include "base/gtest.h"

namespace file {

class ProtoWriterTest : public ::testing::Test {
protected:
};

TEST_F(ProtoWriterTest, Basic) {
  ProtoWriter writer("foo.lst", test::Container::descriptor());
  test::Container boo;
  boo.mutable_person()->set_name("Roman");
  boo.mutable_person()->set_id(5);
  ASSERT_TRUE(writer.Add(boo).ok());
}

TEST_F(ProtoWriterTest, KeyTable) {
  ProtoWriter::Options options;
  options.format = ProtoWriter::KEY_TABLE;

  ProtoWriter writer("foo", test::Container::descriptor(), options);
  test::Container boo;
  boo.mutable_person()->set_name("Roman");
  boo.mutable_person()->set_id(5);
  ASSERT_TRUE(writer.Add(strings::Slice("bar"), boo).ok());
  ASSERT_TRUE(writer.Flush().ok());
}


}  // namespace file