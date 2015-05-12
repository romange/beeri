// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <errno.h>
#include <pthread.h>
#include <deque>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/pthread_utils.h"

namespace base {

inline void tm_add(const timespec& a, timespec* b) {
  b->tv_sec += a.tv_sec;
  b->tv_nsec += a.tv_nsec;
  if (b->tv_nsec >= 1000000000L) {
    ++b->tv_sec;
    b->tv_nsec -= 1000000000L;
  }
}


template <typename T> class sync_queue {
  std::deque<T>   m_queue_;
  mutable pthread_mutex_t m_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t  m_condv_;
  size_t max_size_;
public:
  static constexpr size_t kUnlimited = size_t(-1);

  explicit sync_queue(size_t max_size = kUnlimited) : max_size_(max_size) {
    InitCondVarWithClock(CLOCK_MONOTONIC, &m_condv_);
  }

  ~sync_queue() {
    PTHREAD_CHECK(mutex_destroy(&m_mutex_));
    PTHREAD_CHECK(cond_destroy(&m_condv_));
  }

  void push(const T& item) {
    PTHREAD_CHECK(mutex_lock(&m_mutex_));
    while (m_queue_.size() >= max_size_) {
      PTHREAD_CHECK(cond_wait(&m_condv_, &m_mutex_));
    }
    bool do_signal = m_queue_.empty();
    m_queue_.push_back(item);
    PTHREAD_CHECK(mutex_unlock(&m_mutex_));
    if (do_signal)
      PTHREAD_CHECK(cond_signal(&m_condv_));
  }


  T pop() {
    PMutexGuard g(m_mutex_);
    while (m_queue_.empty()) {
      PTHREAD_CHECK(cond_wait(&m_condv_, &m_mutex_));
    }
    T item{m_queue_.front()};
    if (m_queue_.size() == max_size_)
      PTHREAD_CHECK(cond_signal(&m_condv_));
    m_queue_.pop_front();

    return item;
  }

  bool pop(uint32 ms, T* dest) {
    PMutexGuard g(m_mutex_);
    if (m_queue_.empty()) {
      struct timespec abstime;

      clock_gettime(CLOCK_MONOTONIC_COARSE, &abstime);
      timespec tm = {ms / 1000, (ms % 1000) * 1000000L};
      tm_add(tm, &abstime);
      while (m_queue_.empty()) {
        int res = pthread_cond_timedwait(&m_condv_, &m_mutex_, &abstime);
        if (ETIMEDOUT == res) {
          return false;
        }
        CHECK_EQ(0, res);
      }
    }
    *dest = m_queue_.front();
    if (m_queue_.size() == max_size_)
      PTHREAD_CHECK(cond_signal(&m_condv_));
    m_queue_.pop_front();

    return true;
  }

  size_t size() const {
    PMutexGuard g(m_mutex_);
    return m_queue_.size();
  }

  // Hack routine.
  void WaitTillEmpty() {
    PTHREAD_CHECK(mutex_lock(&m_mutex_));
    while (!m_queue_.empty()) {
      PTHREAD_CHECK(mutex_unlock(&m_mutex_));
      usleep(1000);
      PTHREAD_CHECK(mutex_lock(&m_mutex_));
    }
    PTHREAD_CHECK(mutex_unlock(&m_mutex_));
  }

  sync_queue(const sync_queue& s) = delete;
  sync_queue& operator=(const sync_queue& s) = delete;
};

}  // namespace base

#endif  // SYNC_QUEUE_H