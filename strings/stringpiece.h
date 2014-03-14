// Copyright 2001, Google Inc.  All rights reserved.
// Maintainer: mec@google.com (Michael Chastain)
//
// A StringPiece points to part or all of a string, Cord, double-quoted string
// literal, or other string-like object.  A StringPiece does *not* own the
// string to which it points.  A StringPiece is not null-terminated.
//
// You can use StringPiece as a function or method parameter.  A StringPiece
// parameter can receive a double-quoted string literal argument, a "const
// char*" argument, a string argument, or a StringPiece argument with no data
// copying.  Systematic use of StringPiece for arguments reduces data
// copies and strlen() calls.
//
// You may pass a StringPiece argument by value or const reference.
// Passing by value generates slightly smaller code.
//   void MyFunction(const StringPiece& arg);
//   // Slightly better, but same lifetime requirements as const-ref parameter:
//   void MyFunction(StringPiece arg);
//
// StringPiece is also suitable for local variables if you know that
// the lifetime of the underlying object is longer than the lifetime
// of your StringPiece variable.
//
// Beware of binding a StringPiece to a temporary:
//   StringPiece sp = obj.MethodReturningString();  // BAD: lifetime problem
//
// This code is okay:
//   string str = obj.MethodReturningString();  // str owns its contents
//   StringPiece sp(str);  // GOOD, although you may not need sp at all
//
// StringPiece is sometimes a poor choice for a return value and usually a poor
// choice for a data member.  If you do use a StringPiece this way, it is your
// responsibility to ensure that the object pointed to by the StringPiece
// outlives the StringPiece.
//
// A StringPiece may represent just part of a string; thus the name "Piece".
// For example, when splitting a string, vector<StringPiece> is a natural data
// type for the output.  For another example, a Cord is a non-contiguous,
// potentially very long string-like object.  The Cord class has an interface
// that iteratively provides StringPiece objects that point to the
// successive pieces of a Cord object.
//
// A StringPiece is not null-terminated.  If you write code that scans a
// StringPiece, you must check its length before reading any characters.
// Common idioms that work on null-terminated strings do not work on
// StringPiece objects.
//
// There are several ways to create a null StringPiece:
//   StringPiece()
//   StringPiece(nullptr)
//   StringPiece(nullptr, 0)
// For all of the above, sp.data() == nullptr, sp.length() == 0,
// and sp.empty() == true.  Also, if you create a StringPiece with
// a non-nullptr pointer then sp.data() != non-nullptr.  Once created,
// sp.data() will stay either nullptr or not-nullptr, except if you call
// sp.clear() or sp.set().
//
// Thus, you can use StringPiece(nullptr) to signal an out-of-band value
// that is different from other StringPiece values.  This is similar
// to the way that const char* p1 = nullptr; is different from
// const char* p2 = "";.
//
// There are many ways to create an empty StringPiece:
//   StringPiece()
//   StringPiece(nullptr)
//   StringPiece(nullptr, 0)
//   StringPiece("")
//   StringPiece("", 0)
//   StringPiece("abcdef", 0)
//   StringPiece("abcdef"+6, 0)
// For all of the above, sp.length() will be 0 and sp.empty() will be true.
// For some empty StringPiece values, sp.data() will be nullptr.
// For some empty StringPiece values, sp.data() will not be nullptr.
//
// Be careful not to confuse: null StringPiece and empty StringPiece.
// The set of empty StringPieces properly includes the set of null StringPieces.
// That is, every null StringPiece is an empty StringPiece,
// but some non-null StringPieces are empty Stringpieces too.
//
// All empty StringPiece values compare equal to each other.
// Even a null StringPieces compares equal to a non-null empty StringPiece:
//  StringPiece() == StringPiece("", 0)
//  StringPiece(nullptr) == StringPiece("abc", 0)
//  StringPiece(nullptr, 0) == StringPiece("abcdef"+6, 0)
//
// Look carefully at this example:
//   StringPiece("") == nullptr
// True or false?  TRUE, because StringPiece::operator== converts
// the right-hand side from nullptr to StringPiece(nullptr),
// and then compares two zero-length spans of characters.
// However, we are working to make this example produce a compile error.
//
// Suppose you want to write:
//   bool TestWhat?(StringPiece sp) { return sp == nullptr; }  // BAD
// Do not do that.  Write one of these instead:
//   bool TestNull(StringPiece sp) { return sp.data() == nullptr; }
//   bool TestEmpty(StringPiece sp) { return sp.empty(); }
// The intent of TestWhat? is unclear.  Did you mean TestNull or TestEmpty?
// Right now, TestWhat? behaves likes TestEmpty.
// We are working to make TestWhat? produce a compile error.
// TestNull is good to test for an out-of-band signal.
// TestEmpty is good to test for an empty StringPiece.
//
// Caveats (again):
// (1) The lifetime of the pointed-to string (or piece of a string)
//     must be longer than the lifetime of the StringPiece.
// (2) There may or may not be a '\0' character after the end of
//     StringPiece data.
// (3) A null StringPiece is empty.
//     An empty StringPiece may or may not be a null StringPiece.

#ifndef STRINGS_STRINGPIECE_H_
#define STRINGS_STRINGPIECE_H_

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iosfwd>
#include <limits>
#include <string>
using std::string;

#include "base/integral_types.h"
#include "base/port.h"
#include "strings/fastmem.h"
#include "strings/slice.h"

class StringPiece : public strings::SliceBase<char> {
  typedef strings::SliceBase<char> Base;

 public:
  // Constructor inheritance not yet available in g++.
  // using SliceBase<char>::SliceBase;
  StringPiece() : Base() {}
  StringPiece(const char* str) : Base(str, 0) {
    if (str != nullptr) {
      length_ = strlen(str);
    }
  }

  StringPiece(const std::string& str)  // NOLINT(runtime/explicit)
      : Base(str.data(), str.size()) {
  }

  StringPiece(const char* offset, size_t len)
      : Base(offset, len) {}

  // Substring of another StringPiece.
  // pos must be non-negative and <= x.length().
  StringPiece(const StringPiece& x, size_t pos) : Base(x, pos) {}

  // Substring of another StringPiece.
  // pos must be non-negative and <= x.length().
  // len must be non-negative and will be pinned to at most x.length() - pos.
  StringPiece(const StringPiece& x, size_t pos, size_t len) : Base(x, pos, len) {}
  StringPiece(strings::Slice s) : Base(s.charptr(), s.size()) {}

  using Base::set;

  void set(const char* str) {
    ptr_ = str;
    if (str != nullptr)
      length_ = strlen(str);
    else
      length_ = 0;
  }

  string as_string() const {
    return ToString();
  }

  strings::Slice as_slice() const {
    return strings::Slice(reinterpret_cast<const uint8*>(ptr_), length_);
  }

  bool starts_with(StringPiece x) const {
    return (length_ >= x.length_) && (memcmp(ptr_, x.ptr_, x.length_) == 0);
  }

  bool ends_with(StringPiece x) const {
    return ((length_ >= x.length_) &&
            (memcmp(ptr_ + (length_-x.length_), x.ptr_, x.length_) == 0));
  }
  // We also define ToString() here, since many other string-like
  // interfaces name the routine that converts to a C++ string
  // "ToString", and it's confusing to have the method that does that
  // for a StringPiece be called "as_string()".  We also leave the
  // "as_string()" method defined here for existing code.
  string ToString() const {
    if (ptr_ == nullptr) return string();
    return string(data(), size());
  }

  void CopyToString(string* target) const;
  void AppendToString(string* target) const;

  // cpplint.py emits a false positive [build/include_what_you_use]
  size_type copy(char* buf, size_type n, size_type pos = 0) const;  // NOLINT

  bool contains(StringPiece s) const;

  using Base::rfind;
  size_type rfind(StringPiece s, size_type pos = npos) const;
  size_type find_first_of(char c, size_type pos = 0) const { return Base::find(c, pos); }
  size_type find_first_of(StringPiece s, size_type pos = 0) const;
  size_type find_first_not_of(StringPiece s, size_type pos = 0) const;
  size_type find_first_not_of(char c, size_type pos = 0) const;
  size_type find_last_of(char c, size_type pos = npos) const { return Base::rfind(c, pos); }
  size_type find_last_of(StringPiece s, size_type pos = npos) const;
  size_type find_last_not_of(StringPiece s, size_type pos = npos) const;
  StringPiece substr(size_type pos, size_type length = npos) const;

  // finds nth occurrance of the character. the index starts from 0.
  size_type find_nth(char c, uint32 index) const;
} __attribute__((packed));

// This large function is defined inline so that in a fairly common case where
// one of the arguments is a literal, the compiler can elide a lot of the
// following comparisons.
inline bool operator==(StringPiece x, StringPiece y) {
  size_t len = x.size();
  if (len != y.size()) {
    return false;
  }

  return x.data() == y.data() || len <= 0 ||
      strings::memeq(x.data(), y.data(), len);
}

inline bool operator!=(StringPiece x, StringPiece y) {
  return !(x == y);
}

inline bool operator<(StringPiece x, StringPiece y) {
  const size_t min_size =
      x.size() < y.size() ? x.size() : y.size();
  const int r = memcmp(x.data(), y.data(), min_size);
  return (r < 0) || (r == 0 && x.size() < y.size());
}

inline bool operator>(StringPiece x, StringPiece y) {
  return y < x;
}

inline bool operator<=(StringPiece x, StringPiece y) {
  return !(x > y);
}

inline bool operator>=(StringPiece x, StringPiece y) {
  return !(x < y);
}

// allow StringPiece to be logged
extern std::ostream& operator<<(std::ostream& o, StringPiece piece);


#endif  // STRINGS_STRINGPIECE_H__
