// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/list_file.h"

#include <cstdio>
#include <snappy-c.h>
#include "util/coding/fixed.h"
#include "util/coding/varint.h"
#include "util/crc32c.h"

namespace file {

using util::Status;
using base::StatusCode;
using strings::Slice;
using namespace ::util;
using namespace list_file;

ListReader::ListReader(file::ReadonlyFile* file, Ownership ownership, bool checksum,
                       CorruptionReporter reporter)
  : file_(file), ownership_(ownership), reporter_(reporter),
    checksum_(checksum) {
}

ListReader::ListReader(StringPiece filename, bool checksum, CorruptionReporter reporter)
    : ownership_(TAKE_OWNERSHIP), reporter_(reporter), checksum_(checksum) {
  auto res = file::ReadonlyFile::Open(filename);
  CHECK(res.ok()) << res.status << ", file name: " << filename;
  file_ = res.obj;
  CHECK(file_) << filename;
}

ListReader::~ListReader() {
  if (ownership_ == TAKE_OWNERSHIP) {
    auto st = file_->Close();
    if (!st.ok()) {
      LOG(WARNING) << "Error closing file, status " << st;
    }
    delete file_;
  }
}

bool ListReader::GetMetaData(std::map<std::string, std::string>* meta) {
  if (!ReadHeader()) return false;
  *meta = meta_;
  return true;
}

bool ListReader::ReadRecord(Slice* record, std::string* scratch) {
  if (!ReadHeader()) return false;

  scratch->clear();
  record->clear();
  bool in_fragmented_record = false;
  // Record offset of the logical record that we're reading
  // 0 is a dummy value to make compilers happy
  // uint64_t prospective_record_offset = 0;

  Slice fragment;
  while (true) {
    if (array_records_ > 0) {
      uint32 item_size = 0;
      const uint8* item_ptr = Varint::Parse32WithLimit(array_store_.begin(), array_store_.end(),
                                                       &item_size);
      if (item_ptr == nullptr || item_ptr + item_size > array_store_.end()) {
        ReportCorruption(array_store_.size(), "invalid array record");
        array_records_ = 0;
      } else {
        const uint8* next = item_ptr + item_size;
        array_store_.set(next, array_store_.end() - next);
        record->set(item_ptr, item_size);
        --array_records_;
        return true;
      }
    }
    // uint64_t physical_record_offset = end_of_buffer_offset_ - buffer_.size();
    const unsigned int record_type = ReadPhysicalRecord(&fragment);
    switch (record_type) {
      case kFullType:
        if (in_fragmented_record) {
          ReportCorruption(scratch->size(), "partial record without end(1)");
        }
        scratch->clear();
        *record = fragment;
        // last_record_offset_ = physical_record_offset;
        return true;

      case kFirstType:
        if (in_fragmented_record) {
          // Handle bug in earlier versions of log::Writer where
          // it could emit an empty kFirstType record at the tail end
          // of a block followed by a kFullType or kFirstType record
          // at the beginning of the next block.
          if (scratch->empty()) {
            in_fragmented_record = false;
          } else {
            ReportCorruption(scratch->size(), "partial record without end(2)");
          }
        }
        // prospective_record_offset = physical_record_offset;
        scratch->assign(fragment.as_string());
        in_fragmented_record = true;
        break;

      case kMiddleType:
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(1)");
        } else {
          scratch->append(reinterpret_cast<const char*>(fragment.data()), fragment.size());
        }
        break;

      case kLastType:
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(2)");
        } else {
          scratch->append(reinterpret_cast<const char*>(fragment.data()), fragment.size());
          *record = Slice(*scratch);
          // last_record_offset_ = prospective_record_offset;
          return true;
        }
        break;
      case kArrayType: {
        if (in_fragmented_record) {
            ReportCorruption(scratch->size(), "partial record without end(1)");
        }
        uint32 array_records = 0;
        const uint8* array_ptr = Varint::Parse32WithLimit(fragment.begin(), fragment.end(),
                                                          &array_records);
        if (array_ptr == nullptr || array_records == 0) {
          ReportCorruption(fragment.size(), "invalid array record");
        } else {
          array_records_ = array_records;
          array_store_.set(array_ptr, fragment.end() - array_ptr);
        }
      }
      break;
      case kEof:
        if (in_fragmented_record) {
          ReportCorruption(scratch->size(), "partial record without end(3)");
          scratch->clear();
        }
        return false;

      case kBadRecord:
        if (in_fragmented_record) {
          ReportCorruption(scratch->size(), "error in middle of record");
          in_fragmented_record = false;
          scratch->clear();
        }
        break;

      default: {
        char buf[40];
        snprintf(buf, sizeof(buf), "unknown record type %u", record_type);
        ReportCorruption(
            (fragment.size() + (in_fragmented_record ? scratch->size() : 0)),
            buf);
        in_fragmented_record = false;
        scratch->clear();
      }
    }
  }
  return true;
}

static const uint8* DecodeString(const uint8* ptr, const uint8* end, string* dest) {
  if (ptr == nullptr) return nullptr;
  uint32 string_sz = 0;
  ptr = Varint::Parse32WithLimit(ptr, end, &string_sz);
  if (ptr == nullptr || ptr + string_sz > end)
    return nullptr;
  const char* str = reinterpret_cast<const char*>(ptr);
  dest->assign(str, str + string_sz);
  return ptr + string_sz;
}

#define EXIT_ON_ERROR do { if (!st.ok()) { \
    ReportDrop(file_size_, st); \
    block_size_ = 0; eof_ = true; \
    return false; }} while(false)

bool ListReader::ReadHeader() {
  if (block_size_ != 0) return true;
  if (eof_) return false;
  uint8 buf[kListFileHeaderSize];
  file_size_ = file_->Size();

  strings::Slice result;
  Status st = file_->Read(0, kListFileHeaderSize, &result, buf);
  EXIT_ON_ERROR;
  if (result.size() != kListFileHeaderSize ||
      !result.starts_with(Slice(kMagicString, kMagicStringSize)) ||
      result[kMagicStringSize] == 0 || result[kMagicStringSize] > 100 ) {
    ReportCorruption(kListFileHeaderSize, "Invalid header");
    return false;
  }
  block_size_ = result[kMagicStringSize] * kBlockSizeFactor;
  backing_store_.reset(new uint8[block_size_]);
  uncompress_buf_.reset(new uint8[block_size_]);
  file_offset_ = kListFileHeaderSize;
  if (result[kMagicStringSize + 1] == kMetaExtension) {
    uint8 meta_header[8];
    st = file_->Read(file_offset_, sizeof meta_header, &result, meta_header);
    EXIT_ON_ERROR;
    file_offset_ += result.size();
    uint32 length = coding::DecodeFixed32(result.data() + 4);
    uint32 crc = crc32c::Unmask(coding::DecodeFixed32(result.data()));
    std::unique_ptr<uint8[]> meta_buf(new uint8[length]);
    st = file_->Read(file_offset_, length, &result, meta_buf.get());
    EXIT_ON_ERROR;
    CHECK_EQ(result.size(), length);
    file_offset_ += result.size();
    uint32 actual_crc = crc32c::Value(result.data(), result.size());
    if (crc != actual_crc) {
      block_size_ = 0; eof_ = true;
      LOG(ERROR) << "Corrupted meta data";
      return false;
    }
    const uint8* end = result.end();
    const uint8* ptr = Varint::Parse32WithLimit(result.begin(), end, &length);
    for (uint32 i = 0; i < length; ++i) {
      string key, val;
      ptr = DecodeString(ptr, end, &key);
      ptr = DecodeString(ptr, end, &val);
      if (ptr == nullptr) {
        LOG(ERROR) << "Corrupted meta data";
        block_size_ = 0; eof_ = true;
        return false;
      }
      meta_[key] = val;
    }
  }
  return true;
}

#undef EXIT_ON_ERROR

void ListReader::ReportCorruption(size_t bytes, const string& reason) {
  ReportDrop(bytes, Status(base::StatusCode::IO_ERROR, reason));
}

void ListReader::ReportDrop(size_t bytes, const Status& reason) {
  LOG(ERROR) << "ReportDrop: " << bytes << " "
             << " block buffer_size " << block_buffer_.size() << ", reason: " << reason;
  if (reporter_ /*&& end_of_buffer_offset_ >= initial_offset_ + block_buffer_.size() + bytes*/) {
    reporter_(bytes, reason);
  }
}

using strings::charptr;

unsigned int ListReader::ReadPhysicalRecord(Slice* result) {
  size_t fsize = file_->Size();
  while (true) {
    if (block_buffer_.size() < kBlockHeaderSize) {
      if (!eof_) {
        size_t length = file_offset_ + block_size_ <= fsize ? block_size_ : fsize - file_offset_;
        Status status = file_->Read(file_offset_, length, &block_buffer_, backing_store_.get());
        // end_of_buffer_offset_ += read_size;
        VLOG(2) << "read_size: " << block_buffer_.size() << ", status: " << status;
        if (!status.ok()) {
          ReportDrop(length, status);
          eof_ = true;
          return kEof;
        }
        file_offset_ += block_buffer_.length();
        if (file_offset_ >= fsize) {
          eof_ = true;
        }
        continue;
      } else if (block_buffer_.empty()) {
        // End of file
        return kEof;
      } else {
        size_t drop_size = block_buffer_.size();
        block_buffer_.clear();
        ReportCorruption(drop_size, "truncated record at end of file");
        return kEof;
      }
    }

    // Parse the header
    const uint8* header = block_buffer_.data();
    const uint8 type = header[8];
    uint32 length = coding::DecodeFixed32(header + 4);
    if (length + kBlockHeaderSize > block_buffer_.size()) {
      VLOG(1) << "Invalid length " << length;
      size_t drop_size = block_buffer_.size();
      block_buffer_.clear();
      ReportCorruption(drop_size, "bad record length or truncated record at eof.");
      return kBadRecord;
    }

    if (type == kZeroType && length == 0) {
      // Skip zero length record without reporting any drops since
      // such records are produced by the mmap based writing code in
      // env_posix.cc that preallocates file regions.
      block_buffer_.clear();
      return kBadRecord;
    }
    const uint8* data_ptr = header + kBlockHeaderSize;
    // Check crc
    if (checksum_) {
      uint32_t expected_crc = crc32c::Unmask(coding::DecodeFixed32(header));
      // compute crc of the record and the type.
      uint32_t actual_crc = crc32c::Value(data_ptr - 1, 1 + length);
      if (actual_crc != expected_crc) {
        // Drop the rest of the buffer since "length" itself may have
        // been corrupted and if we trust it, we could find some
        // fragment of a real log record that just happens to look
        // like a valid log record.
        size_t drop_size = block_buffer_.size();
        block_buffer_.clear();
        ReportCorruption(drop_size, "checksum mismatch");
        return kBadRecord;
      }
    }
    uint32 record_size = length + kBlockHeaderSize;
    block_buffer_.remove_prefix(record_size);
    if (type & kCompressedMask) {
      if (*data_ptr != kCompressionSnappy) {
        ReportCorruption(record_size, "Unknown compression method.");
        return kBadRecord;
      }
      ++data_ptr;
      --length;
      size_t uncompress_size = block_size_;
      snappy_status st = snappy_uncompress(charptr(data_ptr),
                                           length, charptr(uncompress_buf_.get()),
                                           &uncompress_size);
      if (st != SNAPPY_OK) {
        ReportCorruption(record_size, "Uncompress failed.");
        return kBadRecord;
      }
      data_ptr = uncompress_buf_.get();
      length = uncompress_size;
    }
    // Skip physical record that started before initial_offset_
    /*if (end_of_buffer_offset_ < initial_offset_ + block_buffer_.size() + record_size) {
      result->clear();
      return kBadRecord;
    }*/

    *result = Slice(data_ptr, length);
    return type & 0xF;
  }
}

}  // namespace file
