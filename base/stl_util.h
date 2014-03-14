// Copyright 2002 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// ---
//
//
// STL utility functions.  Usually, these replace built-in, but slow(!),
// STL functions with more efficient versions or provide a more convenient
// and Google friendly API.
//

#ifndef UTIL_GTL_STL_UTIL_H_
#define UTIL_GTL_STL_UTIL_H_

#include <stddef.h>
#include <string.h>  // for memcpy
#include <algorithm>
using std::copy;
using std::reverse;
using std::sort;
using std::swap;
#include <cassert>
#include <deque>
using std::deque;
#include <functional>
using std::binary_function;
#include <iterator>
using std::back_insert_iterator;
#include <memory>
#include <string>
using std::string;
#include <vector>
using std::vector;

#include "base/integral_types.h"
#include "base/macros.h"
#include "base/port.h"
#include "base/algorithm.h"
#include <ostream>

// Sort and remove duplicates of an STL vector or deque.
template<class T>
void STLSortAndRemoveDuplicates(T *v) {
  sort(v->begin(), v->end());
  v->erase(unique(v->begin(), v->end()), v->end());
}

// Clear internal memory of an STL object.
// STL clear()/reserve(0) does not always free internal memory allocated
// This function uses swap/destructor to ensure the internal memory is freed.
template<class T> void STLClearObject(T* obj) {
  T tmp;
  tmp.swap(*obj);
  obj->reserve(0);  // this is because sometimes "T tmp" allocates objects with
                    // memory (arena implementation?).  use reserve()
                    // to clear() even if it doesn't always work
}

// Specialization for deque. Same as STLClearObject but doesn't call reserve
// since deque doesn't have reserve.
template <class T, class A>
void STLClearObject(deque<T, A>* obj) {
  deque<T, A> tmp;
  tmp.swap(*obj);
}

// Reduce memory usage on behalf of object if its capacity is greater
// than or equal to "limit", which defaults to 2^20.
template <class T> inline void STLClearIfBig(T* obj, size_t limit = 1<<20) {
  if (obj->capacity() >= limit) {
    STLClearObject(obj);
  } else {
    obj->clear();
  }
}

// Specialization for deque, which doesn't implement capacity().
template <class T, class A>
inline void STLClearIfBig(deque<T, A>* obj, size_t limit = 1<<20) {
  if (obj->size() >= limit) {
    STLClearObject(obj);
  } else {
    obj->clear();
  }
}

// Reduce the number of buckets in a hash_set or hash_map back to the
// default if the current number of buckets is "limit" or more.
//
// Suppose you repeatedly fill and clear a hash_map or hash_set.  If
// you ever insert a lot of items, then your hash table will have lots
// of buckets thereafter.  (The number of buckets is not reduced when
// the table is cleared.)  Having lots of buckets is good if you
// insert comparably many items in every iteration, because you'll
// reduce collisions and table resizes.  But having lots of buckets is
// bad if you insert few items in most subsequent iterations, because
// repeatedly clearing out all those buckets can get expensive.
//
// One solution is to call STLClearHashIfBig() with a "limit" value
// that is a small multiple of the typical number of items in your
// table.  In the common case, this is equivalent to an ordinary
// clear.  In the rare case where you insert a lot of items, the
// number of buckets is reset to the default to keep subsequent clear
// operations cheap.  Note that the default number of buckets is 193
// in the Gnu library implementation as of Jan '08.
template <class T> inline void STLClearHashIfBig(T *obj, size_t limit) {
  if (obj->bucket_count() >= limit) {
    T tmp;
    tmp.swap(*obj);
  } else {
    obj->clear();
  }
}

// Reserve space for STL object.
// STL's reserve() will always copy.
// This function avoid the copy if we already have capacity
template<class T> void STLReserveIfNeeded(T* obj, int new_size) {
  if (obj->capacity() < new_size)   // increase capacity
    obj->reserve(new_size);
  else if (obj->size() > new_size)  // reduce size
    obj->resize(new_size);
}

// STLDeleteContainerPointers()
//  For a range within a container of pointers, calls delete
//  (non-array version) on these pointers.
// NOTE: for these three functions, we could just implement a DeleteObject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of <algorithm>, which seems expensive.
// For hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
// NOTE: If you're calling this on an entire container, you probably want
// to call STLDeleteElements(&container) instead, or use an ElementDeleter.
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin,
                                ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// STLDeleteContainerPairPointers()
//  For a range within a container of pairs, calls delete
//  (non-array version) on BOTH items in the pairs.
// NOTE: Like STLDeleteContainerPointers, it is important that this deletes
// behind the iterator because if both the key and value are deleted, the
// container may call the hash function on the iterator when it is advanced,
// which could result in the hash function trying to dereference a stale
// pointer.
template <class ForwardIterator>
void STLDeleteContainerPairPointers(ForwardIterator begin,
                                    ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
    delete temp->second;
  }
}

// STLDeleteContainerPairFirstPointers()
//  For a range within a container of pairs, calls delete (non-array version)
//  on the FIRST item in the pairs.
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
template <class ForwardIterator>
void STLDeleteContainerPairFirstPointers(ForwardIterator begin,
                                         ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
  }
}

// STLDeleteContainerPairSecondPointers()
//  For a range within a container of pairs, calls delete
//  (non-array version) on the SECOND item in the pairs.
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
// Deleting the value does not always invalidate the iterator, but it may
// do so if the key is a pointer into the value object.
// NOTE: If you're calling this on an entire container, you probably want
// to call STLDeleteValues(&container) instead, or use ValueDeleter.
template <class ForwardIterator>
void STLDeleteContainerPairSecondPointers(ForwardIterator begin,
                                          ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->second;
  }
}

template<typename T>
inline void STLAssignToVector(vector<T>* vec,
                              const T* ptr,
                              size_t n) {
  vec->resize(n);
  if (n == 0) return;
  memcpy(&vec->front(), ptr, n*sizeof(T));
}

// Not faster; but we need the specialization so the function works at all
// on the vector<bool> specialization.
template<>
inline void STLAssignToVector(vector<bool>* vec,
                              const bool* ptr,
                              size_t n) {
  vec->clear();
  if (n == 0) return;
  vec->insert(vec->begin(), ptr, ptr + n);
}

// A struct that mirrors the crosstool v16 implementation of a string.
struct InternalStringRepGCC4 {
  char*  _M_data;
  size_t _M_string_length;

  enum { _S_local_capacity = 15 };

  union {
    char             _M_local_data[_S_local_capacity + 1];
    size_t           _M_allocated_capacity;
  };
};

// Like str->resize(new_size), except any new characters added to
// "*str" as a result of resizing may be left uninitialized, rather
// than being filled with '0' bytes.  Typically used when code is then
// going to overwrite the backing store of the string with known data.
inline void STLStringResizeUninitialized(string* s, size_t new_size) {
  if (sizeof(*s) == sizeof(InternalStringRepGCC4)) {
    if (new_size > s->capacity()) {
      s->reserve(new_size);
    }
    // The line below depends on the layout of 'string'.  THIS IS
    // NON-PORTABLE CODE.  If our STL implementation changes, we will
    // need to change this as well.
    InternalStringRepGCC4* rep = reinterpret_cast<InternalStringRepGCC4*>(s);
    assert(rep->_M_data == s->data());
    assert(rep->_M_string_length == s->size());

    // We have to null-terminate the string for c_str() to work properly.
    // So we leave the actual contents of the string uninitialized, but
    // we set the byte one past the new end of the string to '\0'
    const_cast<char*>(s->data())[new_size] = '\0';
    rep->_M_string_length = new_size;
  } else {
    // Slow path: have to reallocate stuff, or an unknown string rep
    s->resize(new_size);
  }
}

// Return a mutable char* pointing to a string's internal buffer,
// which may not be null-terminated. Writing through this pointer will
// modify the string.
//
// string_as_array(&str)[i] is valid for 0 <= i < str.size() until the
// next call to a string method that invalidates iterators.
//
// Prior to C++11, there was no standard-blessed way of getting a mutable
// reference to a string's internal buffer. The requirement that string be
// contiguous is officially part of the C++11 standard [string.require]/5.
// According to Matt Austern, this should already work on all current C++98
// implementations.
inline char* string_as_array(string* str) {
  // DO NOT USE const_cast<char*>(str->data())! See the unittest for why.
  return str->empty() ? NULL : &*str->begin();
}


// The following functions are useful for cleaning up STL containers
// whose elements point to allocated memory.

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.  This function is suitable for use with a vector, set,
// hash_set, or any other STL container which defines sensible begin(), end(),
// and clear() methods.
//
// If container is NULL, this function is a no-op.
//
// As an alternative to calling STLDeleteElements() directly, consider
// ElementDeleter (defined below), which ensures that your container's elements
// are deleted when the ElementDeleter goes out of scope.
template <class T>
void STLDeleteElements(T *container) {
  if (!container) return;
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

// Given an STL container consisting of (key, value) pairs, STLDeleteValues
// deletes all the "value" components and clears the container.  Does nothing
// in the case it's given a NULL pointer.
template <class T>
void STLDeleteValues(T *v) {
  if (!v) return;
  STLDeleteContainerPairSecondPointers(v->begin(), v->end());
  v->clear();
}


// ElementDeleter and ValueDeleter provide a convenient way to delete all
// elements or values from STL containers when they go out of scope.  This
// greatly simplifies code that creates temporary objects and has multiple
// return statements.  Example:
//
// vector<MyProto *> tmp_proto;
// ElementDeleter d(&tmp_proto);
// if (...) return false;
// ...
// return success;

// A very simple interface that simply provides a virtual destructor.  It is
// used as a non-templated base class for the TemplatedElementDeleter and
// TemplatedValueDeleter classes.  Clients should not typically use this class
// directly.
class BaseDeleter {
 public:
  virtual ~BaseDeleter() {}

 protected:
  BaseDeleter() {}

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BaseDeleter);
};

// Given a pointer to an STL container, this class will delete all the element
// pointers when it goes out of scope.  Clients should typically use
// ElementDeleter rather than invoking this class directly.
template<class STLContainer>
class TemplatedElementDeleter : public BaseDeleter {
 public:
  explicit TemplatedElementDeleter<STLContainer>(STLContainer *ptr)
      : container_ptr_(ptr) {
  }

  virtual ~TemplatedElementDeleter<STLContainer>() {
    STLDeleteElements(container_ptr_);
  }

 private:
  STLContainer *container_ptr_;

  DISALLOW_EVIL_CONSTRUCTORS(TemplatedElementDeleter);
};

// Like TemplatedElementDeleter, this class will delete element pointers from a
// container when it goes out of scope.  However, it is much nicer to use,
// since the class itself is not templated.
class ElementDeleter {
 public:
  template <class STLContainer>
  explicit ElementDeleter(STLContainer *ptr)
      : deleter_(new TemplatedElementDeleter<STLContainer>(ptr)) {
  }

  ~ElementDeleter() {
    delete deleter_;
  }

 private:
  BaseDeleter *deleter_;

  DISALLOW_EVIL_CONSTRUCTORS(ElementDeleter);
};

// Given a pointer to an STL container this class will delete all the value
// pointers when it goes out of scope.  Clients should typically use
// ValueDeleter rather than invoking this class directly.
template<class STLContainer>
class TemplatedValueDeleter : public BaseDeleter {
 public:
  explicit TemplatedValueDeleter<STLContainer>(STLContainer *ptr)
      : container_ptr_(ptr) {
  }

  virtual ~TemplatedValueDeleter<STLContainer>() {
    STLDeleteValues(container_ptr_);
  }

 private:
  STLContainer *container_ptr_;

  DISALLOW_EVIL_CONSTRUCTORS(TemplatedValueDeleter);
};

// Similar to ElementDeleter, but wraps a TemplatedValueDeleter rather than an
// TemplatedElementDeleter.
class ValueDeleter {
 public:
  template <class STLContainer>
  explicit ValueDeleter(STLContainer *ptr)
      : deleter_(new TemplatedValueDeleter<STLContainer>(ptr)) {
  }

  ~ValueDeleter() {
    delete deleter_;
  }

 private:
  BaseDeleter *deleter_;

  DISALLOW_EVIL_CONSTRUCTORS(ValueDeleter);
};


// STLElementDeleter and STLValueDeleter are similar to ElementDeleter and
// ValueDeleter, except that:
// - The classes are templated, making them less convenient to use.
// - Their destructors are not virtual, making them potentially more efficient.
// New code should typically use ElementDeleter and ValueDeleter unless
// efficiency is a large concern.

template<class STLContainer> class STLElementDeleter {
 public:
  STLElementDeleter<STLContainer>(STLContainer *ptr) : container_ptr_(ptr) {}
  ~STLElementDeleter<STLContainer>() { STLDeleteElements(container_ptr_); }
 private:
  STLContainer *container_ptr_;
};

template<class STLContainer> class STLValueDeleter {
 public:
  STLValueDeleter<STLContainer>(STLContainer *ptr) : container_ptr_(ptr) {}
  ~STLValueDeleter<STLContainer>() { STLDeleteValues(container_ptr_); }
 private:
  STLContainer *container_ptr_;
};


// STLSet{Difference,SymmetricDifference,Union,Intersection}(A a, B b, C *c)
// *APPEND* the set {difference, symmetric difference, union, intersection} of
// the two sets a and b to c.
// STLSet{Difference,SymmetricDifference,Union,Intersection}(T a, T b) do the
// same but return the result by value rather than by the third pointer
// argument.  The result type is the same as both of the inputs in the two
// argument case.
//
// Requires:
//   a and b must be STL like containers that contain sorted data (as defined
//   by the < operator).
//   For the 3 argument version &a == c or &b == c are disallowed.  In those
//   cases the 2 argument version is probably what you want anyway:
//   a = STLSetDifference(a, b);
//
// These function are convenience functions.  The code they implement is
// trivial (at least for now).  The STL incantations they wrap are just too
// verbose for programmers to use then and they are unpleasant to the eye.
// Without these convenience versions people will simply keep writing one-off
// for loops which are harder to read and more error prone.
//
// Note that for initial construction of an object it is just as efficient to
// use the 2 argument version as the 3 version due to RVO (return value
// optimization) of modern C++ compilers:
//   set<int> c = STLSetDifference(a, b);
// is an example of where RVO comes into play.

template<typename SortedSTLContainerA,
         typename SortedSTLContainerB,
         typename SortedSTLContainerC>
void STLSetDifference(const SortedSTLContainerA &a,
                      const SortedSTLContainerB &b,
                      SortedSTLContainerC *c) {
  // The qualified name avoids an ambiguity error, particularly with C++11:
  assert(util::gtl::is_sorted(a.begin(), a.end()));
  assert(util::gtl::is_sorted(b.begin(), b.end()));
  assert(static_cast<const void *>(&a) !=
         static_cast<const void *>(c));
  assert(static_cast<const void *>(&b) !=
         static_cast<const void *>(c));
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                      std::inserter(*c, c->end()));
}

template<typename SortedSTLContainer>
SortedSTLContainer STLSetDifference(const SortedSTLContainer &a,
                                    const SortedSTLContainer &b) {
  SortedSTLContainer c;
  STLSetDifference(a, b, &c);
  return c;
}

template<typename SortedSTLContainerA,
         typename SortedSTLContainerB,
         typename SortedSTLContainerC>
void STLSetUnion(const SortedSTLContainerA &a,
                 const SortedSTLContainerB &b,
                 SortedSTLContainerC *c) {
  assert(util::gtl::is_sorted(a.begin(), a.end()));
  assert(util::gtl::is_sorted(b.begin(), b.end()));
  assert(static_cast<const void *>(&a) !=
         static_cast<const void *>(c));
  assert(static_cast<const void *>(&b) !=
         static_cast<const void *>(c));
  std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                 std::inserter(*c, c->end()));
}

template<typename SortedSTLContainerA,
         typename SortedSTLContainerB,
         typename SortedSTLContainerC>
void STLSetSymmetricDifference(const SortedSTLContainerA &a,
                               const SortedSTLContainerB &b,
                               SortedSTLContainerC *c) {
  assert(util::gtl::is_sorted(a.begin(), a.end()));
  assert(util::gtl::is_sorted(b.begin(), b.end()));
  assert(static_cast<const void *>(&a) !=
         static_cast<const void *>(c));
  assert(static_cast<const void *>(&b) !=
         static_cast<const void *>(c));
  std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(),
                                std::inserter(*c, c->end()));
}

template<typename SortedSTLContainer>
SortedSTLContainer STLSetSymmetricDifference(const SortedSTLContainer &a,
                                             const SortedSTLContainer &b) {
  SortedSTLContainer c;
  STLSetSymmetricDifference(a, b, &c);
  return c;
}

template<typename SortedSTLContainer>
SortedSTLContainer STLSetUnion(const SortedSTLContainer &a,
                               const SortedSTLContainer &b) {
  SortedSTLContainer c;
  STLSetUnion(a, b, &c);
  return c;
}

template<typename SortedSTLContainerA,
         typename SortedSTLContainerB,
         typename SortedSTLContainerC>
void STLSetIntersection(const SortedSTLContainerA &a,
                        const SortedSTLContainerB &b,
                        SortedSTLContainerC *c) {
  assert(util::gtl::is_sorted(a.begin(), a.end()));
  assert(util::gtl::is_sorted(b.begin(), b.end()));
  assert(static_cast<const void *>(&a) !=
         static_cast<const void *>(c));
  assert(static_cast<const void *>(&b) !=
         static_cast<const void *>(c));
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                        std::inserter(*c, c->end()));
}

template<typename SortedSTLContainer>
SortedSTLContainer STLSetIntersection(const SortedSTLContainer &a,
                                      const SortedSTLContainer &b) {
  SortedSTLContainer c;
  STLSetIntersection(a, b, &c);
  return c;
}

// Similar to STLSet{Union,Intesection,etc}, but simpler because the result is
// always bool.
template<typename SortedSTLContainerA,
         typename SortedSTLContainerB>
bool STLIncludes(const SortedSTLContainerA &a,
                 const SortedSTLContainerB &b) {
  assert(util::gtl::is_sorted(a.begin(), a.end()));
  assert(util::gtl::is_sorted(b.begin(), b.end()));
  return std::includes(a.begin(), a.end(),
                       b.begin(), b.end());
}

// Functors that compose arbitrary unary and binary functions with a
// function that "projects" one of the members of a pair.
// Specifically, if p1 and p2, respectively, are the functions that
// map a pair to its first and second, respectively, members, the
// table below summarizes the functions that can be constructed:
//
// * UnaryOperate1st<pair>(f) returns the function x -> f(p1(x))
// * UnaryOperate2nd<pair>(f) returns the function x -> f(p2(x))
// * BinaryOperate1st<pair>(f) returns the function (x,y) -> f(p1(x),p1(y))
// * BinaryOperate2nd<pair>(f) returns the function (x,y) -> f(p2(x),p2(y))
//
// A typical usage for these functions would be when iterating over
// the contents of an STL map. For other sample usage, see the unittest.

template<typename Pair, typename UnaryOp>
class UnaryOperateOnFirst
    : public std::unary_function<Pair, typename UnaryOp::result_type> {
 public:
  UnaryOperateOnFirst() {
  }

  UnaryOperateOnFirst(const UnaryOp& f) : f_(f) {
  }

  typename UnaryOp::result_type operator()(const Pair& p) const {
    return f_(p.first);
  }

 private:
  UnaryOp f_;
};

template<typename Pair, typename UnaryOp>
UnaryOperateOnFirst<Pair, UnaryOp> UnaryOperate1st(const UnaryOp& f) {
  return UnaryOperateOnFirst<Pair, UnaryOp>(f);
}

template<typename Pair, typename UnaryOp>
class UnaryOperateOnSecond
    : public std::unary_function<Pair, typename UnaryOp::result_type> {
 public:
  UnaryOperateOnSecond() {
  }

  UnaryOperateOnSecond(const UnaryOp& f) : f_(f) {
  }

  typename UnaryOp::result_type operator()(const Pair& p) const {
    return f_(p.second);
  }

 private:
  UnaryOp f_;
};

template<typename Pair, typename UnaryOp>
UnaryOperateOnSecond<Pair, UnaryOp> UnaryOperate2nd(const UnaryOp& f) {
  return UnaryOperateOnSecond<Pair, UnaryOp>(f);
}

template<typename Pair, typename BinaryOp>
class BinaryOperateOnFirst
    : public std::binary_function<Pair, Pair, typename BinaryOp::result_type> {
 public:
  BinaryOperateOnFirst() {
  }

  BinaryOperateOnFirst(const BinaryOp& f) : f_(f) {
  }

  typename BinaryOp::result_type operator()(const Pair& p1,
                                            const Pair& p2) const {
    return f_(p1.first, p2.first);
  }

 private:
  BinaryOp f_;
};

template<typename Pair, typename BinaryOp>
BinaryOperateOnFirst<Pair, BinaryOp> BinaryOperate1st(const BinaryOp& f) {
  return BinaryOperateOnFirst<Pair, BinaryOp>(f);
}

template<typename Pair, typename BinaryOp>
class BinaryOperateOnSecond
    : public std::binary_function<Pair, Pair, typename BinaryOp::result_type> {
 public:
  BinaryOperateOnSecond() {
  }

  BinaryOperateOnSecond(const BinaryOp& f) : f_(f) {
  }

  typename BinaryOp::result_type operator()(const Pair& p1,
                                            const Pair& p2) const {
    return f_(p1.second, p2.second);
  }

 private:
  BinaryOp f_;
};

template<typename Pair, typename BinaryOp>
BinaryOperateOnSecond<Pair, BinaryOp> BinaryOperate2nd(const BinaryOp& f) {
  return BinaryOperateOnSecond<Pair, BinaryOp>(f);
}

// Functor that composes a binary functor h from an arbitrary binary functor
// f and two unary functors g1, g2, so that:
//
// BinaryCompose1(f, g) returns function (x, y) -> f(g(x), g(y))
// BinaryCompose2(f, g1, g2) returns function (x, y) -> f(g1(x), g2(y))
//
// This is a generalization of the BinaryOperate* functors for types other
// than pairs.
//
// For sample usage, see the unittest.
//
// F has to be a model of AdaptableBinaryFunction.
// G1 and G2 have to be models of AdabtableUnaryFunction.
template<typename F, typename G1, typename G2>
class BinaryComposeBinary : public binary_function<typename G1::argument_type,
                                                   typename G2::argument_type,
                                                   typename F::result_type> {
 public:
  BinaryComposeBinary(F f, G1 g1, G2 g2) : f_(f), g1_(g1), g2_(g2) { }

  typename F::result_type operator()(typename G1::argument_type x,
                                     typename G2::argument_type y) const {
    return f_(g1_(x), g2_(y));
  }

 private:
  F f_;
  G1 g1_;
  G2 g2_;
};

template<typename F, typename G>
BinaryComposeBinary<F, G, G> BinaryCompose1(F f, G g) {
  return BinaryComposeBinary<F, G, G>(f, g, g);
}

template<typename F, typename G1, typename G2>
BinaryComposeBinary<F, G1, G2> BinaryCompose2(F f, G1 g1, G2 g2) {
  return BinaryComposeBinary<F, G1, G2>(f, g1, g2);
}

// This is a wrapper for an STL allocator which keeps a count of the
// active bytes allocated by this class of allocators.  This is NOT
// THREAD SAFE.  This should only be used in situations where you can
// ensure that only a single thread performs allocation and
// deallocation.
template <typename T, typename Alloc = std::allocator<T> >
class STLCountingAllocator : public Alloc {
 public:
  typedef typename Alloc::pointer pointer;
  typedef typename Alloc::size_type size_type;

  STLCountingAllocator() : bytes_used_(NULL) { }
  STLCountingAllocator(int64* b) : bytes_used_(b) {}

  // Constructor used for rebinding
  template <class U>
  STLCountingAllocator(const STLCountingAllocator<U>& x)
      : Alloc(x),
        bytes_used_(x.bytes_used()) {
  }

  pointer allocate(size_type n, std::allocator<void>::const_pointer hint = 0) {
    assert(bytes_used_ != NULL);
    *bytes_used_ += n * sizeof(T);
    return Alloc::allocate(n, hint);
  }

  void deallocate(pointer p, size_type n) {
    Alloc::deallocate(p, n);
    assert(bytes_used_ != NULL);
    *bytes_used_ -= n * sizeof(T);
  }

  // Rebind allows an allocator<T> to be used for a different type
  template <class U> struct rebind {
    typedef STLCountingAllocator<U,
                                 typename Alloc::template
                                 rebind<U>::other> other;
  };

  int64* bytes_used() const { return bytes_used_; }

 private:
  int64* bytes_used_;
};

// These functions return true if there is some element in the sorted range
// [begin1, end) which is equal to some element in the sorted range [begin2,
// end2). The iterators do not have to be of the same type, but the value types
// must be less-than comparable. (Two elements a,b are considered equal if
// !(a < b) && !(b < a).
template<typename InputIterator1, typename InputIterator2>
bool SortedRangesHaveIntersection(InputIterator1 begin1, InputIterator1 end1,
                                  InputIterator2 begin2, InputIterator2 end2) {
  assert(util::gtl::is_sorted(begin1, end1));
  assert(util::gtl::is_sorted(begin2, end2));
  while (begin1 != end1 && begin2 != end2) {
    if (*begin1 < *begin2) {
      ++begin1;
    } else if (*begin2 < *begin1) {
      ++begin2;
    } else {
      return true;
    }
  }
  return false;
}

// This is equivalent to the function above, but using a custom comparison
// function.
template<typename InputIterator1, typename InputIterator2, typename Comp>
bool SortedRangesHaveIntersection(InputIterator1 begin1, InputIterator1 end1,
                                  InputIterator2 begin2, InputIterator2 end2,
                                  Comp comparator) {
  assert(util::gtl::is_sorted(begin1, end1, comparator));
  assert(util::gtl::is_sorted(begin2, end2, comparator));
  while (begin1 != end1 && begin2 != end2) {
    if (comparator(*begin1, *begin2)) {
      ++begin1;
    } else if (comparator(*begin2, *begin1)) {
      ++begin2;
    } else {
      return true;
    }
  }
  return false;
}

template<typename T> std::ostream& operator<<(std::ostream& o, const vector<T>& vec) {
  o << "[";
  for (size_t i = 0; i < vec.size(); ++i) {
    o << vec[i];
    if (i + 1 < vec.size()) o << ",";
  }
  o << "]";
  return o;
}

#endif  // UTIL_GTL_STL_UTIL_H_
