// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/list_file.h"

#include <snappy-c.h>

#include "file/filesource.h"
#include "file/file_util.h"
#include "util/coding/fixed.h"
#include "util/coding/varint.h"
#include "util/crc32c.h"

namespace file {

namespace list_file {

const char kMagicString[] = "LST1";

}  // namespace list_file

using util::Status;
using strings::Slice;
using namespace ::util;
using base::StatusCode;

using namespace list_file;

namespace {

class Varint32Encoder {
  uint8 buf_[Varint::kMax32];
  uint8 sz_ = 0;
public:
  Varint32Encoder() : buf_{{0}} {}
  Varint32Encoder(uint32 val) {
    encode(val);
  }

  Slice slice() const { return Slice(buf_, sz_); }
  uint8 size() const { return sz_; }
  void encode(uint32 val) {
    sz_ = Varint::Encode32(buf_, val) - buf_;
  }
  const uint8* data() const { return buf_; }
};

}  // namespace

ListWriter::ListWriter(StringPiece filename, const Options& options)
    : options_(options) {
  File* file = file_util::OpenOrDie(filename, "w");
  dest_.reset(new Sink(file, TAKE_OWNERSHIP));
  Construct();
}

ListWriter::ListWriter(util::Sink* dest, const Options& options)
   : dest_(dest), options_(options) {
  Construct();
}

void ListWriter::Construct() {
  block_size_ = kBlockSizeFactor * options_.block_size_multiplier;
  array_store_.reset(new uint8[block_size_]);
  block_leftover_ = block_size_;
  if (options_.use_compression) {
    compress_buf_size_ = snappy_max_compressed_length(block_size_);
    compress_buf_.reset(new uint8[compress_buf_size_ + 1]); // +1 for compression method byte.
  }
}

ListWriter::~ListWriter() {
  DCHECK_EQ(array_records_, 0) << "ListWriter::Flush() was not called!";
  CHECK(Flush().ok());
}

// Adds user provided meta information about the file. Must be called before Init.
void ListWriter::AddMeta(StringPiece key, strings::Slice value) {
  CHECK(!init_called_);
  meta_[key.as_string()] = value.as_string();
}

util::Status ListWriter::Init() {
  CHECK_GT(options_.block_size_multiplier, 0);
  CHECK(!init_called_);
  RETURN_IF_ERROR(dest_->Append(StringPiece(kMagicString, kMagicStringSize).as_slice()));
  uint8 more_data[2] = {options_.block_size_multiplier,
    meta_.empty() ? kNoExtension : kMetaExtension};
  RETURN_IF_ERROR(dest_->Append(Slice(more_data, arraysize(more_data))));
  if (!meta_.empty()) {
    // Meta format: crc32, fixed32 - meta block size, varint32 map size,
    // (varint string size, string data)+
    // We do not bother with memory optimizations since the meta data should relatively small.
    std::vector<uint8> buf;
    Varint32Encoder encoder(meta_.size());

    // Not proud of this code. In general, I did not find how to make serialization code
    // more readable without giving up on performance.
    auto insert_size = [&buf, &encoder]() {
          buf.insert(buf.end(), encoder.data(), encoder.data() + encoder.size());
        };
    auto append = [&buf](const std::string& s) {
          buf.insert(buf.end(), s.begin(), s.end());
        };
    insert_size();

    for (const auto& k_v : meta_) {
      encoder.encode(k_v.first.size());
      insert_size();
      append(k_v.first);
      encoder.encode(k_v.second.size());
      insert_size();
      append(k_v.second);
    }
    uint8 meta_header[8];
    coding::EncodeFixed32(buf.size(), meta_header + 4);
    uint32 crc = crc32c::Mask(crc32c::Value(buf.data(), buf.size()));
    coding::EncodeFixed32(crc, meta_header);
    RETURN_IF_ERROR(dest_->Append(Slice(meta_header, sizeof meta_header)));
    RETURN_IF_ERROR(dest_->Append(Slice(buf.data(), buf.size())));
  }
  init_called_ = true;
  return Status::OK;
}

inline void ListWriter::AddRecordToArray(Slice size_enc, Slice record) {
  memcpy(array_next_, size_enc.data(), size_enc.size());
  memcpy(array_next_ + size_enc.size(), record.data(), record.size());
  array_next_ += size_enc.size() + record.size();
  ++array_records_;
}

inline Status ListWriter::FlushArray() {
  if (array_records_ == 0) return Status::OK;
  Varint32Encoder enc(array_records_);

  // We prepend array_records_ integer right before the data.
  uint8* start = array_store_.get() + kArrayRecordMaxHeaderSize - enc.size();
  memcpy(start, enc.data(), enc.size());
  // Flush the open array.
  Status st = EmitPhysicalRecord(kArrayType, start, array_next_ - start);
  array_records_ = 0;
  return st;
}

Status ListWriter::AddRecord(strings::Slice record) {
  CHECK_GT(block_size_, 0) << "ListWriter::Init was not called.";

  Varint32Encoder record_size_encoded(record.size());
  const uint32 record_size_total = record_size_encoded.size() + record.size();
  // Try to accomodate either in the array or a single block.  Multiple iterations might be
  // needed since we might fragment the record.
  bool fragmenting = false;
  ++records_added_;
  while (true) {
    if (array_records_ > 0) {
      if (array_next_ + record_size_total <= array_end_) {
        AddRecordToArray(record_size_encoded.slice(), record);
        return Status::OK;
      }
      RETURN_IF_ERROR(FlushArray());
      // Also we must either split the record or transfer to the next block.
    }
    if (block_leftover() < kBlockHeaderSize) {
      // Block trailing bytes. Just fill them with zeroes.
      uint8 kBlockFilling[kBlockHeaderSize] = {0};
      RETURN_IF_ERROR(dest_->Append(Slice(kBlockFilling, block_leftover())));
      block_offset_ = 0;
      block_leftover_ = block_size_;
    }

    if (fragmenting) {
      size_t fragment_length = record.size();
      RecordType type = kLastType;
      if (fragment_length > block_leftover() - kBlockHeaderSize) {
        fragment_length = block_leftover() - kBlockHeaderSize;
        type = kMiddleType;
      }
      RETURN_IF_ERROR(EmitPhysicalRecord(type, record.data(), fragment_length));
      if (type == kLastType)
        return Status::OK;
      record.remove_prefix(fragment_length);
      continue;
    }
    if (record_size_total + kArrayRecordMaxHeaderSize < block_leftover()) {
      // Lets start the array accumulation.
      // We leave space at the beginning to prepend the header at the end.
      array_next_ = array_store_.get() + kArrayRecordMaxHeaderSize;
      array_end_ = array_store_.get() + block_leftover();
      AddRecordToArray(record_size_encoded.slice(), record);
      return Status::OK;
    }
    if (kBlockHeaderSize + record.size() <= block_leftover()) {
      // We have space for exactly one record in this block.
      return EmitPhysicalRecord(kFullType, record.data(), record.size());
    }
    // We must fragment.
    fragmenting = true;
    const size_t fragment_length = block_leftover() - kBlockHeaderSize;
    RETURN_IF_ERROR(EmitPhysicalRecord(kFirstType, record.data(), fragment_length));
    record.remove_prefix(fragment_length);
  };
  return Status(StatusCode::INTERNAL_ERROR, "Should not reach here");
}

Status ListWriter::Flush() {
  return FlushArray();
}

using strings::charptr;

Status ListWriter::EmitPhysicalRecord(RecordType type, const uint8* ptr,
                                      size_t length) {
  // Varint32Encoder enc(length);
  DCHECK_LE(kBlockHeaderSize + length, block_leftover());

  // Format the header
  uint8 buf[kBlockHeaderSize];
  buf[8] = type;
  if (options_.use_compression && length > 128) {
    size_t compressed_length = compress_buf_size_;
    snappy_status st = snappy_compress(charptr(ptr), length, charptr(compress_buf_.get()) + 1,
                                       &compressed_length);
    VLOG(1) << "Compressed record with size " << length << " to ratio "
            << float(compressed_length) / length;
    if (st == SNAPPY_OK) {
      if (compressed_length < length - length / 8) {
        buf[8] |= kCompressedMask;
        compress_buf_[0] = kCompressionSnappy;
        ptr = compress_buf_.get();
        length = compressed_length + 1;
      }
    } else {
      LOG(WARNING) << "Snappy error " << st;
    }
  }

  coding::EncodeFixed32(length, buf + 4);

  // Compute the crc of the record type and the payload.
  uint32 crc = crc32c::Value(buf + 8, 1);
  crc = crc32c::Extend(crc, ptr, length);
  crc = crc32c::Mask(crc);                 // Adjust for storage
  VLOG(2) << "EmitPhysicalRecord: type " << (type & 0xF) <<  ", length: " << length
          << ", crc: " << crc << " compressed: " << (buf[8] & kCompressedMask);
  coding::EncodeFixed32(crc, buf);

  // Write the header and the payload
  RETURN_IF_ERROR(dest_->Append(Slice(buf, kBlockHeaderSize)));
  RETURN_IF_ERROR(dest_->Append(Slice(ptr, length)));
  bytes_added_ += (kBlockHeaderSize + length);
  block_offset_ += (kBlockHeaderSize + length);
  block_leftover_ = block_size_ - block_offset_;
  return Status::OK;
}

}  // namespace file
