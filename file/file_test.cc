// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "file/file.h"

#include <memory>
#include "file/file_util.h"
#include "base/gtest.h"

namespace file {

class FileTest : public ::testing::Test {
protected:
};

TEST_F(FileTest, S3Basic) {
  constexpr char kFileName[] = "s3://somefile";

  FileCloser fl(Open(kFileName, "r"));
  ASSERT_TRUE(fl.get() != nullptr);

  constexpr size_t kSize = 100000;
  std::unique_ptr<uint8[]> buf(new uint8[kSize]);
  size_t read_size = 0;
  auto status = fl->Read(kSize, buf.get(), &read_size);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(fl->eof());

  string str;
  ASSERT_TRUE(file_util::ReadFileToString(kFileName, &str));
  EXPECT_TRUE(str.find("package") != string::npos);

  EXPECT_TRUE(Exists(kFileName));
  EXPECT_FALSE(Exists("s3://simefile"));
}

}  // namespace file