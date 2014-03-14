// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "util/zlib_source.h"

#include <memory>
#include "base/logging.h"

namespace util {

bool ZlibSource::IsZlibSource(Source* source) {
  strings::Slice header(source->Peek(2));
  return header.size() >= 2 && (header[0] == 0x1f) && (header[1] == 0x8b);
}

ZlibSource::ZlibSource(
    Source* sub_stream, Ownership ownership, Format format, uint32 buffer_size)
    : BufferredSource(buffer_size), sub_stream_(sub_stream), ownership_(ownership),
      format_(format) {
  zcontext_.zalloc = Z_NULL;
  zcontext_.zfree = Z_NULL;
  zcontext_.opaque = Z_NULL;
  zcontext_.total_out = 0;
  zcontext_.next_in = NULL;
  zcontext_.avail_in = 0;
  zcontext_.total_in = 0;
  zcontext_.msg = NULL;

  CHECK_GT(buffer_size, 1024) << "Buffer size is not large enough.";
}

ZlibSource::~ZlibSource() {
  inflateEnd(&zcontext_);
  if (ownership_ == TAKE_OWNERSHIP) delete sub_stream_;
}

static inline int internalInflateInit2(ZlibSource::Format format,
    z_stream* zcontext) {
  int windowBitsFormat = 0;
  switch (format) {
    case ZlibSource::GZIP: windowBitsFormat = 16; break;
    case ZlibSource::AUTO: windowBitsFormat = 32; break;
    case ZlibSource::ZLIB: windowBitsFormat = 0; break;
  }
  return inflateInit2(zcontext, /* windowBits */15 | windowBitsFormat);
}

bool ZlibSource::RefillInternal() {
  zcontext_.next_out = peek_pos_ + avail_peek_;
  zcontext_.avail_out = available_to_refill();
  while (zcontext_.avail_out > 0) {
    if (zcontext_.avail_in == 0) {
      bool first = zcontext_.next_in == nullptr;
      sub_stream_->Skip(input_buf_.size());
      input_buf_ = sub_stream_->Peek(buf_size_ / 16);
      if (input_buf_.empty()) {
        return true;
      }
      zcontext_.next_in = const_cast<uint8*>(input_buf_.data());
      zcontext_.avail_in = input_buf_.size();
      if (first) {
        zerror_ = internalInflateInit2(format_, &zcontext_);
        if (zerror_ != Z_OK) {
          // Consider removing error output here and leave it to the caller.
          LOG(WARNING) << "inflateinit error: " << zerror_ << " message: "
                       << zcontext_.msg;
          return true;
        }
      }
    }
    zerror_ = inflate(&zcontext_, Z_NO_FLUSH);
    avail_peek_ = zcontext_.next_out - peek_pos_;
    if (zerror_ != Z_OK) {
      if (zerror_ == Z_STREAM_END) {
        zerror_ = Z_OK;
      } else {
        LOG(WARNING) << "inflate error: " << zerror_ << " message: "
                   << zcontext_.msg;
      }
      return true;
    }
  }
  return false;
}

}  // namespace util

