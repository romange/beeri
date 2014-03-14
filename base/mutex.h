// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _BASE_MUTEX_H
#define _BASE_MUTEX_H

#include <pthread.h>
#include "base/logging.h"
#include "base/macros.h"

namespace base {

class Mutex {
 public:
  Mutex() {}
  ~Mutex() { pthread_mutex_destroy(&mu_); }
  inline void Lock() { CHECK_EQ(0, pthread_mutex_lock(&mu_)); }
  inline void Unlock() { CHECK_EQ(0, pthread_mutex_unlock(&mu_)); }

 private:
  pthread_mutex_t mu_ = PTHREAD_MUTEX_INITIALIZER;
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

// Scoped locking helpers.
class MutexLock {
 public:
  explicit MutexLock(Mutex *lock) : lock_(lock) { lock_->Lock(); }
  ~MutexLock() { lock_->Unlock(); }

 private:
  Mutex *lock_;

  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

}  // namespace base

#endif  // _BASE_MUTEX_H

