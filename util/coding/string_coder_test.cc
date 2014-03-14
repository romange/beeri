// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/string_coder.h"

#include "base/gtest.h"
#include "util/sinksource.h"

namespace util {
namespace coding {

class StringCoderTest : public testing::Test {
protected:
  void Add(StringPiece str) { encoder_.AddStringPiece(str); }

  void Finalize() {
    encoder_.Finalize();
    serialized_size_ = encoder_.ByteSize();
    encoder_.SerializeTo(&ssink_);
    ASSERT_EQ(serialized_size_, ssink_.contents().size());
  }

  strings::Slice contents() const {
    return strings::Slice(ssink_.contents());
  }

  StringEncoder encoder_;
  uint32 serialized_size_;
  util::StringSink ssink_;
  StringDecoder decoder_;
};

TEST_F(StringCoderTest, Basic) {
  const char* vals[] = {"Roman", "And", "Jessica", "Alba" };
  for (int i = 0; i < arraysize(vals); ++i) {
    Add(vals[i]);
  }
  Finalize();

  ASSERT_TRUE(decoder_.Init(contents()).ok());
  StringPiece str;
  for (int i = 0; i < arraysize(vals); ++i) {
    ASSERT_TRUE(decoder_.Next(&str)) << i;
    EXPECT_EQ(vals[i], str);
  }
  ASSERT_FALSE(decoder_.Next(&str));
}

TEST_F(StringCoderTest, Empty) {
  Finalize();
  ASSERT_TRUE(decoder_.Init(contents()).ok());
  StringPiece str;
  ASSERT_FALSE(decoder_.Next(&str));
}

TEST_F(StringCoderTest, EmptyStr) {
  Add("");
  Finalize();
  ASSERT_TRUE(decoder_.Init(contents()).ok());
  StringPiece str;
  ASSERT_TRUE(decoder_.Next(&str));
  ASSERT_EQ("", str);
}

TEST_F(StringCoderTest, Compression) {
  for (int i = 0; i < 200; ++i) {
    Add("aa");
  }
  Finalize();
  auto st = decoder_.Init(contents());
  ASSERT_TRUE(st.ok()) << st;
  StringPiece str;
  for (int i = 0; i < 200; ++i) {
    ASSERT_TRUE(decoder_.Next(&str)) << i;
    EXPECT_EQ("aa", str);
  }
  ASSERT_FALSE(decoder_.Next(&str));
}


}  // namespace coding
}  // namespace util
