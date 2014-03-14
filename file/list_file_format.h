// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Log format information shared by reader and writer.
// See ../doc/log_format.txt for more detail.

#ifndef _LIST_FILE_FORMAT_H_
#define _LIST_FILE_FORMAT_H_

#include "base/integral_types.h"
#include "base/port.h"

namespace file {
namespace list_file {

enum RecordType {
  // Zero is reserved for preallocated files
  kZeroType = 0,

  kFullType = 1,

  // For fragments
  kFirstType = 2,
  kMiddleType = 3,
  kArrayType = 4,
  kLastType = 5
};
const uint8 kMaxRecordType = kLastType;

const uint8 kCompressedMask = 0x10;

// Please note that in case of compression, the record header is followed by a byte describing
// the compression method. Right now just SNAPPY is supported.
const uint8 kCompressionSnappy = 1;

// The file header is:
//    magic string "LST1\0",
//    uint8 block_size_multiplier;
//    uint8 extentsion_type;
const uint8 kMagicStringSize = 5;
const uint8 kListFileHeaderSize = kMagicStringSize + 2;
const uint32 kBlockSizeFactor = 65536;
const uint8 kNoExtension = 0;
const uint8 kMetaExtension = 1;

// Header is checksum (4 bytes), record length (Fixed32 - 4bytes), type (1 byte) and
// optional "record specific header".
// kBlockHeaderSize summarizes lengths of checksum, the length and the type
// The type is an enum RecordType masked with kXXMask values (currently just kCompressedMask).
const uint32 kBlockHeaderSize = 4 + 4 + 1;

// Record of arraytype header is just varint32 that contains number of array records.
const uint32 kArrayRecordMaxHeaderSize = 5 /*Varint::kMax32*/ + kBlockHeaderSize;

extern const char kMagicString[];

}  // namespace list_file
}  // namespace file

#endif  // _LIST_FILE_FORMAT_H_
