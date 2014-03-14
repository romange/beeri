// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Changed by Roman Gershman (romange@gmail.com)
#ifndef _FILE_SSTABLE_TABLE_FORMAT_H_
#define _FILE_SSTABLE_TABLE_FORMAT_H_

#include <string>
#include <stdint.h>
#include "strings/slice.h"
#include "base/status.h"
#include "file/sstable/options.h"

namespace file {

class ReadonlyFile;

namespace sstable {

// All internal key names start with "!".
extern const char kFilterNamePrefix[];
extern const char kMetaBlockKey[];

// BlockHandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class BlockHandle {
 public:
  BlockHandle() : offset_(~static_cast<uint64>(0)), size_(~static_cast<uint64>(0)) {}

  // The offset of the block in the file.
  uint64 offset() const { return offset_; }
  void set_offset(uint64 offset) { offset_ = offset; }

  // The size of the stored block
  uint64 size() const { return size_; }
  void set_size(uint64 size) { size_ = size; }

  void EncodeTo(std::string* dst) const;
  base::Status DecodeFrom(strings::Slice* input);

  // Maximum encoding length of a BlockHandle, Varint::kMax64 * 2.
  enum { kMaxEncodedLength = 10 + 10 };

 private:
  uint64 offset_;
  uint64 size_;
};

// Footer encapsulates the fixed information stored at the tail
// end of every table file.
class Footer {
 public:
  Footer() { }

  // The block handle for the metaindex block of the table
  const BlockHandle& metaindex_handle() const { return metaindex_handle_; }
  void set_metaindex_handle(const BlockHandle& h) { metaindex_handle_ = h; }

  // The block handle for the index block of the table
  const BlockHandle& index_handle() const {
    return index_handle_;
  }
  void set_index_handle(const BlockHandle& h) {
    index_handle_ = h;
  }

  void EncodeTo(std::string* dst) const;
  base::Status DecodeFrom(strings::Slice input);

  // Encoded length of a Footer.  Note that the serialization of a
  // Footer will always occupy exactly this many bytes.  It consists
  // of two block handles and a magic number.
  enum { kEncodedLength = 2*BlockHandle::kMaxEncodedLength + sizeof(uint64) };

 private:
  BlockHandle metaindex_handle_;
  BlockHandle index_handle_;
};

struct BlockContents {
  strings::Slice data;           // Actual contents of data
  bool cachable;        // True iff data can be cached
  bool heap_allocated;  // True iff caller should delete[] data.data()
};

// Read the block identified by "handle" from "file".  On failure
// return non-OK.  On success fill *result and return OK.
base::Status ReadBlock(ReadonlyFile* file,
                       const ReadOptions& options,
                       const BlockHandle& handle,
                       BlockContents* result);

}  // namespace sstable

}  // namespace file

#endif  // _FILE_SSTABLE_TABLE_FORMAT_H_
