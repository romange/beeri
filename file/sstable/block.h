// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_H_

#include <cstddef>
#include "base/integral_types.h"

namespace file {
namespace sstable {
struct BlockContents;
class Iterator;

class Block {
 public:
  // Initialize the block with the specified contents.
  explicit Block(const BlockContents& contents);

  ~Block();

  size_t size() const { return size_; }
  Iterator* NewIterator();

 private:
  uint32 NumRestarts() const;

  const uint8* data_;
  size_t size_;
  uint32 restart_offset_;     // Offset in data_ of restart array
  bool owned_;                  // Block owns data_[]

  // No copying allowed
  Block(const Block&) = delete;
  void operator=(const Block&) = delete;

  class Iter;
};
}  // namespace sstable
}  // namespace file

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_H_
