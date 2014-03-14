// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _STRINGS_HASH_H
#define _STRINGS_HASH_H

#include <functional>
#include "base/hash.h"
#include "strings/slice.h"
#include "strings/stringpiece.h"

namespace std {

template<> struct hash<strings::Slice> {
  size_t operator()(strings::Slice slice) const {
    return base::MurmurHash3_x86_32(slice.data(), slice.size(), 16785407UL);
  }
};

template<> struct hash<StringPiece> {
  size_t operator()(StringPiece sp) const {
    return hash<strings::Slice>()(sp.as_slice());
  }
};

}  // namespace std


#endif  // _STRINGS_HASH_H

