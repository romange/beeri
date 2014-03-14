// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef BASE_HASH_H
#define BASE_HASH_H

#include <cstdint>

namespace base {

uint32_t MurmurHash3_x86_32(const uint8_t* data, uint32_t len, uint32_t seed);
uint32_t CityHash32(uint64_t val);

}  // namespace base


#endif  // BASE_HASH_H

