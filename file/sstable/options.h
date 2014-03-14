// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef _FILE_SSTABLE_OPTIONS_H_
#define _FILE_SSTABLE_OPTIONS_H_

#include <stddef.h>

namespace file {
namespace sstable {

class FilterPolicy;

// DB contents are stored in a set of blocks, each of which holds a
// sequence of key,value pairs.  Each block may be compressed before
// being stored in a file.  The following enum describes which
// compression method (if any) is used to compress a block.
enum CompressionType {
  // NOTE: do not change the values of existing entries, as these are
  // part of the persistent format on disk.
  kNoCompression     = 0x0,
  kSnappyCompression = 0x1
};

// Options that control read operations
struct ReadOptions {
  const FilterPolicy* filter_policy = nullptr;
  // If true, all data read from underlying storage will be
  // verified against corresponding checksums.
  bool verify_checksums = false;
};

// Options to control the behavior of a database (passed to DB::Open)
struct Options {
  // Number of keys between restart points for delta encoding of keys.
  // This parameter can be changed dynamically.  Most clients should
  // leave this parameter alone.
  //
  // Default: 16
  unsigned block_restart_interval = 16;

  // Compress blocks using the specified compression algorithm.  This
  // parameter can be changed dynamically.
  //
  // Default: kSnappyCompression, which gives lightweight but fast
  // compression.
  //
  // Typical speeds of kSnappyCompression on an Intel(R) Core(TM)2 2.4GHz:
  //    ~200-500MB/s compression
  //    ~400-800MB/s decompression
  // Note that these speeds are significantly faster than most
  // persistent storage speeds, and therefore it is typically never
  // worth switching to kNoCompression.  Even if the input data is
  // incompressible, the kSnappyCompression implementation will
  // efficiently detect that and will switch to uncompressed mode.
  CompressionType compression = kSnappyCompression;

  // If non-NULL, use the specified filter policy to reduce disk reads.
  // Many applications will benefit from passing the result of
  // NewBloomFilterPolicy() here.
  //
  // Default: NULL
  const FilterPolicy* filter_policy = nullptr;

  // Approximate size of user data packed per block.  Note that the
  // block size specified here corresponds to uncompressed data.  The
  // actual size of the unit read from disk may be smaller if
  // compression is enabled.
  unsigned block_size = 16384;

  // Create an Options object with default values for all fields.
  Options() {}
};

}  // namespace sstable
}  // namespace file

#endif  // _FILE_SSTABLE_OPTIONS_H_
