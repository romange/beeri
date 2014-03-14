// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _UTIL_HOUR_COUNTER_H
#define _UTIL_HOUR_COUNTER_H

#include <time.h>
#include "base/integral_types.h"

namespace util {

class HourCounter {
  uint32 is_set_ : 1;
  uint32 hour_   : 7;
  uint32 count_  : 24;

public:
  HourCounter() : is_set_(0), hour_(0), count_(0) {}
  void Inc() {
    IncAtTime(time(NULL));
  }

  uint32 value() const {
    uint8 cur_hour = (time(NULL)/3600) & 127;
    return is_set_ && (cur_hour == hour_) ? count_ : 0;
  }

  void IncAtTime(long secs_epoch) {
    uint8 cur_hour = (secs_epoch/3600) & 127;
    if (!is_set_ || hour_ != cur_hour) {
      is_set_ = 1;
      hour_ = cur_hour;
      count_ = 1;
    } else {
      ++count_;
    }
  }
};

}  // namespace util


#endif  // _UTIL_HOUR_COUNTER_H