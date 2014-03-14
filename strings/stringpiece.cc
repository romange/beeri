// Copyright 2004 and onwards Google Inc.
//
//

#include "strings/stringpiece.h"

#include <string.h>
#include <algorithm>
using std::copy;
using std::max;
using std::min;
using std::reverse;
using std::sort;
using std::swap;
#include <climits>
#include <string>
using std::string;


#include "base/logging.h"
#include "strings/memutil.h"
// #include "base/stl_util.h"
#include "strings/slice.h"

std::ostream& operator<<(std::ostream& o, StringPiece piece) {
  o.write(piece.data(), piece.size());
  return o;
}

void StringPiece::CopyToString(string* target) const {
  target->assign(ptr_, length_);
}

void StringPiece::AppendToString(string* target) const {
  target->append(ptr_, length_);
}

size_t StringPiece::copy(char* buf, size_type n, size_type pos) const {
  size_t ret = std::min(length_ - pos, n);
  memcpy(buf, ptr_ + pos, ret);
  return ret;
}

bool StringPiece::contains(StringPiece s) const {
  return find(s, 0) != npos;
}

/*
size_t StringPiece::find(char c, size_type pos) const {
  const char* result = static_cast<const char*>(
      memchr(ptr_ + pos, c, length_ - pos));
  return result != NULL ? result - ptr_ : npos;
}


*/
// For each character in characters_wanted, sets the index corresponding
// to the ASCII code of that character to 1 in table.  This is used by
// the find_.*_of methods below to tell whether or not a character is in
// the lookup table in constant time.
// The argument `table' must be an array that is large enough to hold all
// the possible values of an unsigned char.  Thus it should be be declared
// as follows:
//   bool table[UCHAR_MAX + 1]
static inline void BuildLookupTable(StringPiece characters_wanted,
                                    bool* table) {
  const size_t length = characters_wanted.length();
  const char* const data = characters_wanted.data();
  for (size_t i = 0; i < length; ++i) {
    table[static_cast<unsigned char>(data[i])] = true;
  }
}

size_t StringPiece::find_first_of(StringPiece s, size_type pos) const {
  if (length_ <= 0 || s.length_ <= 0) {
    return npos;
  }
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_t i = pos; i < length_; ++i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

size_t StringPiece::find_first_not_of(StringPiece s, size_type pos) const {
  if (length_ <= 0) return npos;
  if (s.length_ <= 0) return 0;
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_not_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_t i = pos; i < length_; ++i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

size_t StringPiece::find_first_not_of(char c,
                                                      size_type pos) const {
  if (length_ <= 0) return npos;

  for (; pos < static_cast<size_type>(length_); ++pos) {
    if (ptr_[pos] != c) {
      return pos;
    }
  }
  return npos;
}

size_t StringPiece::find_last_of(StringPiece s, size_type pos) const {
  if (length_ <= 0 || s.length_ <= 0) return npos;
  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return Base::rfind(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (int i =
       std::min(pos, static_cast<size_type>(length_ - 1)); i >= 0; --i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

size_t StringPiece::find_last_not_of(StringPiece s, size_type pos) const {
  if (length_ <= 0) return npos;

  int i = std::min(pos, static_cast<size_type>(length_ - 1));
  if (s.length_ <= 0) return i;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) return Base::find_last_not_of(s.ptr_[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (; i >= 0; --i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}


StringPiece StringPiece::substr(size_type pos, size_type length) const {
  if (pos > length_) pos = length_;
  if (length > length_ - pos) length = length_ - pos;
  return StringPiece(ptr_ + pos, length);
}

size_t StringPiece::find_nth(char c, uint32 index) const {
  if (length_ <= 0) return npos;

  for (size_t i = 0; i < length_; ++i) {
    if (ptr_[i] == c) {
      if (index-- == 0)
        return i;
    }
  }
  return npos;
}