// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/sstable/format.h"

#include <memory>
#include <snappy-c.h>
#include "base/logging.h"
#include "file/file.h"
#include "util/coding/fixed.h"
#include "util/coding/varint.h"
#include "util/crc32c.h"

using base::Status;
using strings::Slice;

namespace file {
namespace {

// kTableMagicNumber was picked by running
//    echo 'Roman&Jessie' | sha1sum
// and taking the leading 64 bits.
const uint64 kTableMagicNumber = 0xf968d1dde8e3d8d6ull;

// 1-byte type + 32-bit crc
const size_t kBlockTrailerSize = 5;

bool GetVarint64(Slice* s, uint64* value) {
  const uint8* next = Varint::Parse64WithLimit(s->begin(), s->end(), value);
  if (next == nullptr)
    return false;
  s->set(next, s->end() - next);
  return true;
}

inline Status Corruption(const char* str) {
  return Status(base::StatusCode::IO_ERROR, str);
}

}  // namespace

namespace sstable {

const char kFilterNamePrefix[] = "!filter.";
const char kMetaBlockKey[] = "!meta_block";

void BlockHandle::EncodeTo(std::string* dst) const {
  // Sanity check that all fields have been set
  DCHECK_NE(offset_ , ~static_cast<uint64_t>(0));
  DCHECK_NE(size_ , ~static_cast<uint64_t>(0));
  Varint::Append64(dst, offset_);
  Varint::Append64(dst, size_);
}

Status BlockHandle::DecodeFrom(Slice* input) {
  if (GetVarint64(input, &offset_) &&
      GetVarint64(input, &size_)) {
    return Status::OK;
  }
  return Corruption("bad block handle");
}

void Footer::EncodeTo(std::string* dst) const {
  const size_t original_size = dst->size();
  metaindex_handle_.EncodeTo(dst);
  index_handle_.EncodeTo(dst);
  dst->resize(2 * BlockHandle::kMaxEncodedLength);  // Padding
  coding::AppendFixed64(kTableMagicNumber, dst);
  DCHECK_EQ(dst->size(),  original_size + kEncodedLength);
}

Status Footer::DecodeFrom(Slice input) {
  DCHECK_EQ(kEncodedLength, input.size());
  const uint8* magic_ptr = input.end() - coding::kFixed64Bytes;
  uint64 magic = 0;
  coding::DecodeFixed64(magic_ptr, &magic);
  if (magic != kTableMagicNumber) {
    return Corruption("not an sstable (bad magic number)");
  }

  Status result = metaindex_handle_.DecodeFrom(&input);
  if (result.ok()) {
    result = index_handle_.DecodeFrom(&input);
  }
  return result;
}

Status ReadBlock(ReadonlyFile* file,
                 const ReadOptions& options,
                 const BlockHandle& handle,
                 BlockContents* result) {
  result->data = Slice();
  result->cachable = false;
  result->heap_allocated = false;

  // Read the block contents as well as the type/crc footer.
  // See table_builder.cc for the code that built this structure.
  size_t n = static_cast<size_t>(handle.size());
  std::unique_ptr<uint8[]> buf(new uint8[n + kBlockTrailerSize]);
  Slice contents;
  Status s = file->Read(handle.offset(), n + kBlockTrailerSize, &contents, buf.get());
  if (!s.ok()) {
    return s;
  }
  if (contents.size() != n + kBlockTrailerSize) {
    return Corruption("truncated block read");
  }

  // Check the crc of the type and the block contents
  const uint8* data = contents.data();    // Pointer to where Read put the data
  if (options.verify_checksums) {
    const uint32_t crc = util::crc32c::Unmask(coding::DecodeFixed32(data + n + 1));
    const uint32_t actual = util::crc32c::Value(data, n + 1);
    if (actual != crc) {
      return Corruption("block checksum mismatch");
    }
  }

  switch (data[n]) {
    case kNoCompression:
      if (data != buf.get()) {
        // File implementation gave us pointer to some other data.
        // Use it directly under the assumption that it will be live
        // while the file is open.
        result->data = Slice(data, n);
        result->heap_allocated = false;
        result->cachable = false;  // Do not double-cache
      } else {
        result->data.set(buf.release(), n);
        result->heap_allocated = true;
        result->cachable = true;
      }

      // Ok
      break;
    case kSnappyCompression: {
      size_t ulength = 0;
      if (snappy_uncompressed_length(strings::charptr(data), n, &ulength) != SNAPPY_OK) {
        LOG(ERROR) << "snappy_uncompressed_length error with n " << n;
        return Corruption("corrupted compressed block contents");
      }
      std::unique_ptr<char[]> ubuf(new char[ulength]);
      if (snappy_uncompress(strings::charptr(data), n, ubuf.get(), &ulength) != SNAPPY_OK) {
        return Corruption("corrupted compressed block contents");
      }
      result->data = Slice(ubuf.release(), ulength);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    }
    default:
      return Corruption("bad block type");
  }
  return Status::OK;
}

}  // namespace sstable
}  // namespace file
