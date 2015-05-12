// Copyright 2015, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "base/free_list.h"
#include "base/logging.h"

namespace base {

FreeListBase::FreeListBase(unsigned size) {
  CHECK_NE(0, size);
  slow_allocated = 0;
  list_allocated = 0;
}

}  // namespace base