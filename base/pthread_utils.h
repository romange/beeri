// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _BASE_PTHREAD_UTILS_H
#define _BASE_PTHREAD_UTILS_H

#include <pthread.h>
#include <functional>

constexpr int kThreadStackSize = 65536;

#define PTHREAD_CHECK(x) \
  do { \
    int my_err = pthread_ ## x; \
    CHECK_EQ(0, my_err) << strerror(my_err); \
  } while(false)

namespace base {

void InitCondVarWithClock(clockid_t clock_id, pthread_cond_t* var);


pthread_t StartThread(const char* name, void *(*start_routine) (void *), void *arg);
pthread_t StartThread(const char* name, std::function<void()> f);

class PMutexGuard {
  PMutexGuard(const PMutexGuard&) = delete;
  void operator=(const PMutexGuard&) = delete;
  pthread_mutex_t* m_;
public:
  PMutexGuard(pthread_mutex_t& m) : m_(&m) {
    PTHREAD_CHECK(mutex_lock(m_));
  }

  ~PMutexGuard() {
    PTHREAD_CHECK(mutex_unlock(m_));
  }
};

}  // namespace base

#endif  // _BASE_PTHREAD_UTILS_H