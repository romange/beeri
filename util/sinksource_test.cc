#include <gtest/gtest.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <algorithm>
#include <string>
#include <vector>

#include "util/coding/fixed.h"
#include "base/logging.h"
#include "base/random.h"

#include "util/zlib_source.h"

using std::string;
using std::vector;
using strings::Slice;

namespace util {

using google::protobuf::io::StringOutputStream;
using google::protobuf::io::GzipOutputStream;

class SourceTest : public testing::Test {
  void SetUp() {
    for (int i = 0; i < 100000; ++i) {
      coding::AppendFixed32(i, &original_);
    }
    EXPECT_EQ(100000 * coding::kFixed32Bytes, original_.size());
    const uint8* read = reinterpret_cast<const uint8*>(original_.data());
    for (int i = 0; i < 100000; ++i) {
      uint32 val = coding::DecodeFixed32(read);
      read += coding::kFixed32Bytes;
      EXPECT_EQ(i, val);
    }
    StringOutputStream compressed_stream(&compressed_);
    GzipOutputStream gstream(&compressed_stream);
    size_t copied = 0;
    read = reinterpret_cast<const uint8*>(original_.data());
    while (copied < original_.size()) {
      void* data = nullptr;
      int size = 0;
      CHECK(gstream.Next(&data, &size));
      int copy = std::min(size, int(original_.size() - copied));
      memcpy(data, read, copy);
      copied += copy;
      read += copy;
      if (copy < size) {
         gstream.BackUp(size - copy);
      }
    }
    EXPECT_EQ(original_.size(), copied);
    gstream.Flush();
  }

protected:
  string original_, compressed_;
};

TEST_F(SourceTest, Basic) {
  StringSource ssource(compressed_);
  EXPECT_EQ(compressed_.size(), ssource.Peek().size());
  EXPECT_TRUE(ZlibSource::IsZlibSource(&ssource));
  ZlibSource gsource(&ssource, DO_NOT_TAKE_OWNERSHIP);
  EXPECT_EQ(Z_OK, gsource.ZlibErrorCode());

  const uint8* read = reinterpret_cast<const uint8*>(original_.data());
  size_t compared = 0;
  while (compared < original_.size()) {
    Slice result = gsource.Peek(1215);
    EXPECT_GT(result.size(), 0) << "compared: " << compared;
    int cmp = memcmp(read, result.data(), result.size());
    EXPECT_EQ(0, cmp);
    gsource.Skip(result.size());
    compared += result.size();
    read += result.size();
  }
  EXPECT_EQ(original_.size(), compared);
}

TEST_F(SourceTest, MinSize) {
  StringSource ssource(compressed_);
  ZlibSource gsource(&ssource, DO_NOT_TAKE_OWNERSHIP,
    ZlibSource::AUTO, 3073);
  size_t compared = 0;
  const uint8* read = reinterpret_cast<const uint8*>(original_.data());
  MTRandom random(10);
  while (compared < original_.size()) {
    Slice result = gsource.Peek(1215);
    EXPECT_GT(result.size(), 0) << "compared: " << compared;
    size_t new_sz = (random.Rand16() % result.size()) + 1;
    result.truncate(new_sz);
    int cmp = memcmp(read, result.data(), result.size());
    EXPECT_EQ(0, cmp);
    gsource.Skip(result.size());
    compared += result.size();
    read += result.size();
  }
  EXPECT_EQ(original_.size(), compared);
}

}  // namespace util
