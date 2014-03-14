// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _UTIL_CODING_BIT_PACK_H
#define _UTIL_CODING_BIT_PACK_H

#include "base/integral_types.h"

namespace util {
namespace coding {

constexpr unsigned BIT_PACK_MARGIN = 4;

// Fixed bit width integer bit packing and unpacking.
// count - number of integers to pack. dest must be large enough to contain packed array
// (at least PackedByteCount + BIT_PACK_MARGIN bytes).
// We need BIT_PACK_MARGIN in order not to overrun the memory in some cases and make packing faster.
// The returned pointer is exact though, i.e. points exactly at PackedByteCount limit.
uint8* BitPack(const uint32* src, uint32 count, uint8 bit_width, uint8* dest);
uint8* BitPack(const uint64* src, uint32 count, uint8 bit_width, uint8* dest);

// Returns number of bytes needed to contain count integer of fixed bit_width.
inline uint32 PackedByteCount(uint32 count, uint8 bit_width) {
  return (count*bit_width + 7)/8;
}

// count - number of numbers to decode.
const uint8* BitUnpack(const uint8* src, uint32 count, uint8 bit_width, uint32* dest);
const uint8* BitUnpack(const uint8* src, uint32 count, uint8 bit_width, uint64* dest);

}  // namespace coding
}  // namespace util

#endif  // _UTIL_CODING_BIT_PACK_H