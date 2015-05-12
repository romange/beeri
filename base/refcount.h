// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _REFCOUNT_H
#define _REFCOUNT_H

#include <atomic>

namespace base {

/* Usage:
  class Foo : public RefCount<Foo> {
    ...
  };

  ....
  Foo* foo = new Foo();
  foo->AddRef();
  foo->DecRef();
  foo->DecRef();  //  is self deleted here.
*/
// Never declare RefCount on the stack. I am not sure how to implement this policy via RefCount.

class RefCountBase {
public:
  void AddRef() { count_.fetch_add(1, std::memory_order_release);}
protected:
  RefCountBase() : count_(1) {}

  std::atomic_uint count_;
};

template<typename T> class RefCount : public RefCountBase {
public:
  // Returns true if object was deleted.
  bool DecRef() {
    if (count_.fetch_sub(1, std::memory_order_acquire) == 1) {
      delete static_cast<T*>(this);
      return true;
    }
    return false;
  }
};

}  // namespace base

#endif  // _REFCOUNT_H