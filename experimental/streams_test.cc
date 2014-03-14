#include <gtest/gtest.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "base/integral_types.h"
#include "base/logging.h"
#include "file/test_util.h"

using google::protobuf::io::FileOutputStream;
using std::string;

class StreamTest : public testing::Test {
};

TEST_F(StreamTest, Basic) {
  string fname = TestTempDir() + "/outfile";
  int outfd = open(fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
  ASSERT_GT(outfd, 0) << fname;
  FileOutputStream fs(outfd, 16);
  uint8* buffer = NULL;
  int size;
  EXPECT_TRUE(fs.Next(reinterpret_cast<void**>(&buffer), &size));
  EXPECT_EQ(16, size);
  fs.BackUp(6);
  uint8* next_marker = buffer + 10;
  EXPECT_TRUE(fs.Next(reinterpret_cast<void**>(&buffer), &size));
  EXPECT_EQ(16, size);
  EXPECT_EQ(next_marker, buffer);
  CHECK_EQ(0, close(outfd));
}