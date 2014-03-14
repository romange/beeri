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

namespace base {

/*inline bool tm_less(const timespec& a, const timespec& b) {
  if(a.tv_sec < b.tv_sec) return true;
  if(a.tv_sec > b.tv_sec) return false;
  return a.tv_nsec < b.tv_nsec;
}*/

inline void tm_add(const timespec& a, timespec* b) {
  b->tv_sec += a.tv_sec;
  b->tv_nsec += a.tv_nsec;
  if (b->tv_nsec >= 1000000000L) {
    ++b->tv_sec;
    b->tv_nsec -= 1000000000L;
  }
}

/*inline void tm_sub(const timespec& a, timespec* b) {
  b->tv_sec -= a.tv_sec;
  b->tv_nsec -= a.tv_nsec;
}*/

template <typename T> class sync_queue {
  std::deque<T>   m_queue_;
  mutable pthread_mutex_t m_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t  m_condv_ = PTHREAD_COND_INITIALIZER;
  size_t max_size_;

  static const size_t kUnlimited = size_t(-1);

public:
  explicit sync_queue(size_t max_size = kUnlimited) : max_size_(max_size) {
  }

  ~sync_queue() {
    pthread_mutex_destroy(&m_mutex_);
    pthread_cond_destroy(&m_condv_);
  }

  void push(const T& item) {
    pthread_mutex_lock(&m_mutex_);
    while (m_queue_.size() >= max_size_) {
      pthread_cond_wait(&m_condv_, &m_mutex_);
    }
    m_queue_.push_back(item);
    pthread_cond_signal(&m_condv_);
    pthread_mutex_unlock(&m_mutex_);
  }


  T pop() {
    pthread_mutex_lock(&m_mutex_);
    while (m_queue_.empty()) {
      pthread_cond_wait(&m_condv_, &m_mutex_);
    }
    T item{m_queue_.front()};
    if (m_queue_.size() == max_size_)
      pthread_cond_signal(&m_condv_);
    m_queue_.pop_front();

    pthread_mutex_unlock(&m_mutex_);
    return item;
  }

  bool pop(uint32 ms, T* dest) {
    pthread_mutex_lock(&m_mutex_);
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    timespec tm = {ms / 1000, (ms % 1000) * 1000000L};
    tm_add(tm, &abstime);
    while (m_queue_.empty()) {
      int res = pthread_cond_timedwait(&m_condv_, &m_mutex_, &abstime);
      if (ETIMEDOUT == res) {
        pthread_mutex_unlock(&m_mutex_);
        return false;
      }
      if (res == EINVAL) {
        LOG(ERROR) << "pthread_cond_timedwait Error " << abstime.tv_nsec << " " << abstime.tv_sec;
        pthread_mutex_unlock(&m_mutex_);
        return false;
      }
      CHECK_EQ(0, res);
    }
    *dest = m_queue_.front();
    if (m_queue_.size() == max_size_)
      pthread_cond_signal(&m_condv_);
    m_queue_.pop_front();

    pthread_mutex_unlock(&m_mutex_);
    return true;
  }

  size_t size() const {
    pthread_mutex_lock(&m_mutex_);
    size_t res = m_queue_.size();
    pthread_mutex_unlock(&m_mutex_);
    return res;
  }

  sync_queue(const sync_queue& s) = delete;
  sync_queue& operator=(const sync_queue& s) = delete;
};

}  // namespace base

#endif  // SYNC_QUEUE_H