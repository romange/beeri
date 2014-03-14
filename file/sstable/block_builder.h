// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef _FILE_SSTABLE_BLOCK_BUILDER_H_
#define _FILE_SSTABLE_BLOCK_BUILDER_H_

#include <vector>

#include <cstdint>
#include "strings/slice.h"

namespace file {
namespace sstable {

struct Options;

class BlockBuilder {
 public:
  explicit BlockBuilder(const Options* options);

  // Reset the contents as if the BlockBuilder was just constructed.
  void Reset();

  // REQUIRES: Finish() has not been callled since the last call to Reset().
  // REQUIRES: key is larger than any previously added key
  void Add(const strings::Slice& key, const strings::Slice& value);

  // Finish building the block and return a slice that refers to the
  // block contents.  The returned slice will remain valid for the
  // lifetime of this builder or until Reset() is called.
  strings::Slice Finish();

  // Returns an estimate of the current (uncompressed) size of the block
  // we are building.
  size_t CurrentSizeEstimate() const;

  // Return true iff no entries have been added since the last Reset()
  bool empty() const {
    return buffer_.empty();
  }

 private:
  const Options*        options_;
  std::string           buffer_;      // Destination buffer
  std::vector<uint32_t> restarts_;    // Restart points
  unsigned              counter_;     // Number of entries emitted since restart
  bool                  finished_;    // Has Finish() been called?
  std::string           last_key_;

  // No copying allowed
  BlockBuilder(const BlockBuilder&) = delete;
  void operator=(const BlockBuilder&) = delete;
};

}  // namespace sstable
}  // namespace file

#endif  // _FILE_SSTABLE_BLOCK_BUILDER_H_
