// Copyright 2015, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#ifndef _BASE_FREE_LIST_H
#define _BASE_FREE_LIST_H

#include <atomic>
#include <memory>
#include <type_traits>

#include "base/integral_types.h"

namespace base {

class FreeListBase {
public:
  std::atomic<uint32> slow_allocated, list_allocated;

protected:
  FreeListBase(unsigned size);
};

// Please note that this class implements thread-safe non-blocking freelist data structure
// for single allocator thread and multiple releasing threads.
// In other words, New() should be called from a single thread.
class FreeList : public FreeListBase {
public:
  typedef int T;
  explicit FreeList(unsigned size);

  T* New();
  void Release(T* t);


private:
  union Item {
    T t;
    uint32 next_index;
    Item() : next_index(kuint32max) {
      static_assert(std::is_pod<T>::value, "Must be pod type");
    }
  };

  uint32 size_;
  std::unique_ptr<Item[]> items_;
  std::atomic<uint32> next_available_;
};


inline FreeList::FreeList(unsigned size) : FreeListBase(size), size_(size), items_(new Item[size]),
                                           next_available_(0) {
  for (unsigned i = 0; i < size - 1; ++i) {
    items_[i].next_index = i + 1;
  }
}

inline FreeList::T* FreeList::New() {
  uint32 head = next_available_.load(std::memory_order_acquire);
  if (head >= size_) {
    slow_allocated.fetch_add(1, std::memory_order_relaxed);
    return new T;
  }

  list_allocated.fetch_add(1, std::memory_order_relaxed);

  // Conceptually we want to do: head = head->next;
  // Note that we have ABA problem here because we non-atomically read
  // next_index and swap next_available_. That's the reason why we allow only a single thread doing
  // allocations.
  // Also note that there is bug in GCC https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60272
  // but it should not affect us because we do not share "head" variable across threads.
  while (!next_available_.compare_exchange_weak(
    head, items_[head].next_index, std::memory_order_release, std::memory_order_relaxed))
  ;

  // TODO: to initialize/construct T for non-pod types.
  return &items_[head].t;
}

inline void FreeList::Release(T* t) {
  static_assert(std::is_standard_layout<Item>::value, "");
  Item* i = reinterpret_cast<Item*>(t);
  std::ptrdiff_t t_id = i - items_.get();
  if (t_id < 0 || unsigned(t_id) >= size_) {
    delete t;
    return;
  }

  // t belongs to items_.
  uint32 head = next_available_.load(std::memory_order_relaxed);
  do {
    i->next_index = head;
  } while(!next_available_.compare_exchange_weak(head, t_id, std::memory_order_release,
                                                 std::memory_order_relaxed));
}

}  // namespace base

#endif  // _BASE_FREE_LIST_H