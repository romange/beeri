// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef GZIPSOURCE_H
#define GZIPSOURCE_H

#include <zlib.h>

#include "base/macros.h"
#include "util/sinksource.h"

namespace util {

class ZlibSource : public BufferredSource {
 public:
  // Format key for constructor
  enum Format {
    // zlib will autodetect gzip header or deflate stream
    AUTO = 0,

    // GZIP streams have some extra header data for file attributes.
    GZIP = 1,

    // Simpler zlib stream format.
    ZLIB = 2,
  };


  // buffer_size and format may be -1 for default of 64kB and GZIP format.
  explicit ZlibSource(Source* sub_source, Ownership ownership,
                      Format format = AUTO,
                      uint32 buffer_size = BufferredSource::kDefaultBufferSize);
  ~ZlibSource();

  // Return last error message or NULL if no error.
  const char* ErrorMessage() const {
    return zcontext_.msg;
  }

  int ZlibErrorCode() const {
    return zerror_;
  }

  static bool IsZlibSource(Source* source);

 private:
  Source* sub_stream_;
  Ownership ownership_;
  Format format_;
  z_stream zcontext_;
  int zerror_ = Z_OK;

  strings::Slice input_buf_;

  int Inflate();

  bool RefillInternal();

  DISALLOW_EVIL_CONSTRUCTORS(ZlibSource);
};

}  // namespace util

#endif  // GZIPSOURCE_H
