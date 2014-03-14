// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "util/sinksource.h"
#include "base/logging.h"

using strings::Slice;

namespace util {

Sink::WritableBuffer Sink::GetAppendBuffer(
    size_t min_capacity,
    WritableBuffer scratch,
    size_t /*desired_capacity_hint*/) {
  CHECK_GE(scratch.capacity, min_capacity);
  return scratch;
}

Status Sink::Flush() { return Status::OK; }

BufferredSource::BufferredSource(uint32 bufsize) : buffer_(new uint8[bufsize]),
  buf_size_(bufsize) {
  peek_pos_ = buffer_.get();
}

void BufferredSource::Skip(size_t count) {
  CHECK_LE(count, avail_peek_);
  avail_peek_ -= count;
  peek_pos_ += count;
  if (avail_peek_ == 0) {
    // What was written to the buffer, was consumed. Lets use this opportunity and
    // turn around to the start.
    peek_pos_ = buffer_.get();
  }
}

Slice BufferredSource::Peek(uint32 minimal_size) {
  DCHECK_LT(minimal_size, buf_size_);
  if (IsPeekable(minimal_size)) {
    return Slice(peek_pos_, avail_peek_);
  }
  Refill(minimal_size);
  if (!eof_) {
    CHECK_GE(avail_peek_, minimal_size);
  }
  return Slice(peek_pos_, avail_peek_);
}

void BufferredSource::Refill(uint32 minimal_size) {
  uint8* start = buffer_.get();
  uint32 offset = peek_pos_ - start;

  // We do not have enough space to fill the buffer in order to accomodate flat block of
  // minimal_size bytes. We must reset peek_pos_ to the start.
  if (buf_size_ < offset + avail_peek_ + minimal_size) {
    VLOG(1) << "Moving block of " << avail_peek_ << " bytes because of minimal_size "
            << minimal_size << " with buf_size " << buf_size_ << " and offset " << offset;
    // Move [peek_pos_, peek_pos_ + avail_peek_] to
    // [start, start + avail_peek_]
    // if they overlap, then minimal_size is probably too large.
    if (GOOGLE_PREDICT_FALSE(avail_peek_ > offset)) {
      LOG(DFATAL) << "minimal_size is too large: " << minimal_size;
      memmove(start, peek_pos_, avail_peek_);
    } else {
      memcpy(start, peek_pos_, avail_peek_);
    }
    peek_pos_ = start;
  }
  eof_ = RefillInternal();
}

}  // namespace util

