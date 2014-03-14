// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/bit_pack.h"
#include "base/logging.h"

#ifdef IS_BIG_ENDIAN
  #error Not implemented for big endian due to bitpack64 implementation.
#endif

namespace util {
namespace coding {

inline constexpr uint8 ones_mask8(uint8 bw) { return (1 << bw) - 1;}

static uint8* BitPackSlow32(const uint32* src, uint8 skip,
                            uint32 count, uint8 bit_width, uint8* dest) {
  DCHECK_LT(bit_width, 32);
  DCHECK_NE(0, bit_width % 8);

  uint8 offset = 0;
  memset(dest, 0, PackedByteCount(count, bit_width));

  uint8 full_bytes = bit_width / 8;
  if (full_bytes) {
    for (uint32 i = 0; i < count; ++i, src += skip) {
      uint32 val = *src;
      uint8 bw = bit_width;
      *dest++ |= uint8(val << offset);
      val >>= (8 - offset);
      bw -= (8 - offset);
      switch (full_bytes) {
        case 3:
          *dest++ = val & 0xFF;
          val >>= 8;
          bw -=8;
        case 2:
          *dest++ = val & 0xFF;
          val >>= 8;
          bw -=8;
        case 1:
          // Note that since we handled 8*k cases before, there are still some bits to write.
          *dest = val & ones_mask8(bw);
          val >>= 8;
          dest += (bw >= 8);
          offset = bw % 8;
      }
      // We have optional remainder to add to first offset bits of this byte.
      // Note that offset can be 0, in this case we should not add anything.
      // We assume that val had indeed at most bit_width, so now it should have only offset
      // bits active.
      DCHECK_LE(val, ones_mask8(offset));
      *dest |= val;
    }
  } else {
    for (uint32 i = 0; i < count; ++i, src += skip) {
      uint32 val = *src;
      uint8 left = 8 - offset;
      if (bit_width > left) {
        // we could make further optimizations for 2,4 bits.
        *dest++ |= uint8(val << offset);
        val >>= left;
        offset = bit_width - left;
        // Similar to above, val should have only remaining bits if any.
        DCHECK_LE(val, ones_mask8(offset));
        *dest |= val;
      } else {
        *dest |= uint8(val << offset);
        offset += bit_width;
        dest += (offset >= 8);
        offset %= 8;
      }
    }
  }
  if (offset)
    ++dest;
  return dest;
}

uint8* BitPack(const uint32* src, uint32 count, uint8 bit_width, uint8* dest) {
  DCHECK(bit_width <= sizeof(uint32)*8 && bit_width > 0);

  if (bit_width % 8 == 0) {
    uint8 bytes_width = bit_width / 8;
    for (uint32 i = 0; i < count; ++i, ++src) {
      memcpy(dest, src, bytes_width);
      dest += bytes_width;
    }
    return dest;
  }
  return BitPackSlow32(src, 1, count, bit_width, dest);
}

uint8* BitPack(const uint64* src, uint32 count, uint8 bit_width, uint8* dest) {
  DCHECK(bit_width <= sizeof(uint64)*8 && bit_width > 0);

  if (bit_width % 8 == 0) {
    uint8 bytes_width = bit_width / 8;
    for (uint32 i = 0; i < count; ++i, ++src) {
      memcpy(dest, src, bytes_width);
      dest += bytes_width;
    }
    return dest;
  }
  if (bit_width < 32) {
    return BitPackSlow32(reinterpret_cast<const uint32*>(src), 2, count, bit_width, dest);
  }
  memset(dest, 0, PackedByteCount(count, bit_width));
  uint8 offset = 0;
  for (uint32 i = 0; i < count; ++i, ++src) {
    uint64 val = *src;
    *reinterpret_cast<uint64*>(dest) |= uint64(val << offset);
    uint8 total_bits = offset + bit_width;
    dest += total_bits / 8;
    if (total_bits > 64) {
      *dest |= uint8(val >> (128 - total_bits));
    }
    offset = total_bits % 8;
  }
  if (offset)
    ++dest;
  return dest;
}

template<typename T> static const uint8* BitUnpackTempl(
    const uint8* src, uint32 count, uint8 bit_width,
    T* dest) {
  uint8 src_offset = 0;
  const T kNumMask = (1ULL << bit_width) - 1;

  for (uint32 i = 0; i < count; ++i, ++dest) {
    *dest = (*src >> src_offset);
    uint8 copied = 8 - src_offset;
    if (bit_width < copied) {
      src_offset += bit_width;
      DCHECK_LT(src_offset, 8);
      *dest &= kNumMask;
      continue;
    }
    while (bit_width >= copied + 8) {
      ++src;
      *dest |= (T(*src) << copied);
      copied += 8;
    }
    ++src;
    if (bit_width > copied) {
      *dest |= (T(*src) << copied);
      *dest &= kNumMask;
      src_offset = bit_width - copied;
    } else {
      DCHECK_EQ(bit_width, copied);
      src_offset = 0;
    }
  }
  if (src_offset > 0) {
    ++src;
  }
  return src;
}

const uint8* BitUnpack(const uint8* src, uint32 count, uint8 bit_width, uint32* dest) {
  return BitUnpackTempl(src, count, bit_width, dest);
}

const uint8* BitUnpack(const uint8* src, uint32 count, uint8 bit_width, uint64* dest) {
  return BitUnpackTempl(src, count, bit_width, dest);
}

}  // namespace coding
}  // namespace util