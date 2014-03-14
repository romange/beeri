// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef BZIP_SOURCE_H
#define BZIP_SOURCE_H

#include <memory>
#include "util/sinksource.h"

namespace util {

class BzipSource : public BufferredSource {
public:
  BzipSource(Source* sub_source, Ownership ownership);
  ~BzipSource();

  static bool IsBzipSource(Source* source);
private:
  Source* sub_stream_;
  Ownership ownership_;

  bool RefillInternal() override;

  struct Rep;

  std::unique_ptr<Rep> rep_;
};

}  // namespace util

#endif  // BZIP_SOURCE_H