// Copyright 2002 and onwards Google Inc.
//
// A collection of useful (static) bit-twiddling functions.

#include "base/integral_types.h"

#include "base/logging.h"
#include "base/macros.h"

#ifndef _BITS_H_
#define _BITS_H_

class Bits {
 public:
  // Return the number of one bits in the given integer.
  static int CountOnesInByte(unsigned char n);

  static int CountOnes(uint32 n) {
    n -= ((n >> 1) & 0x55555555);
    n = ((n >> 2) & 0x33333333) + (n & 0x33333333);
    return (((n + (n >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
  }

  // Count bits using sideways addition [WWG'57]. See Knuth TAOCP v4 7.1.3(59)
  static inline int CountOnes64(uint64 n) {
#if defined(__x86_64__)
    n -= (n >> 1) & 0x5555555555555555ULL;
    n = ((n >> 2) & 0x3333333333333333ULL) + (n & 0x3333333333333333ULL);
    return (((n + (n >> 4)) & 0xF0F0F0F0F0F0F0FULL)
            * 0x101010101010101ULL) >> 56;
#else
    return CountOnes(n >> 32) + CountOnes(n & 0xffffffff);
#endif
  }

  // Count bits using popcnt instruction (available on argo machines).
  // Doesn't check if the instruction exists.
  // Please use TestCPUFeature(POPCNT) from base/cpuid/cpuid.h before using this.
  static inline int CountOnes64withPopcount(uint64 n) {
#if defined(__x86_64__) && defined __GNUC__
    int64 count = 0;
    asm("popcnt %1,%0" : "=r"(count) : "rm"(n) : "cc");
    return count;
#else
    return CountOnes64(n);
#endif
  }

  // Returns LSB bit index (0-31). For 0 returns 32.
  static inline uint32_t Bsf(uint32 n) {
    if (n == 0) return 32;
    uint32_t idx;
    asm("bsf %1, %0": "=r" (idx) : "r" (n));
    return idx;
  }

  // Returns MSB index (0-31). For 0 returns 0.
  static inline uint32_t Bsr(uint32 v) {
    if (v == 0) return 0;
    uint32_t answer;
    asm("bsr %1, %0;" :"=r"(answer) :"r"(v));
    return answer;
  }

  // Reverse the bits in the given integer.
  static uint8 ReverseBits8(uint8 n);
  static uint32 ReverseBits32(uint32 n);
  static uint64 ReverseBits64(uint64 n);

  // Return floor(log2(n)) for positive integer n.  Returns -1 iff n == 0.
  static int Log2Floor(uint32 n);
  static int Log2Floor64(uint64 n);

  // Potentially faster version of Log2Floor() that returns an
  // undefined value if n == 0
  static int Log2FloorNonZero(uint32 n);
  static int Log2FloorNonZero64(uint64 n);

  // Return ceiling(log2(n)) for positive integer n.  Returns -1 iff n == 0.
  static int Log2Ceiling(uint32 n);
  static int Log2Ceiling64(uint64 n);

  // Return the first set least / most significant bit, 0-indexed.  Returns an
  // undefined value if n == 0.  FindLSBSetNonZero() is similar to ffs() except
  // that it's 0-indexed, while FindMSBSetNonZero() is the same as
  // Log2FloorNonZero().
  static int FindLSBSetNonZero(uint32 n);
  static int FindLSBSetNonZero64(uint64 n);
  static int FindMSBSetNonZero(uint32 n) { return Log2FloorNonZero(n); }
  static int FindMSBSetNonZero64(uint64 n) { return Log2FloorNonZero64(n); }

  // Portable implementations
  static int Log2Floor_Portable(uint32 n);
  static int Log2FloorNonZero_Portable(uint32 n);
  static int FindLSBSetNonZero_Portable(uint32 n);
  static int Log2Floor64_Portable(uint64 n);
  static int Log2FloorNonZero64_Portable(uint64 n);
  static int FindLSBSetNonZero64_Portable(uint64 n);

 private:
  static const char num_bits[];
  static const unsigned char bit_reverse_table[];
  DISALLOW_COPY_AND_ASSIGN(Bits);
};

// ------------------------------------------------------------------------
// Implementation details follow
// ------------------------------------------------------------------------

// use GNU builtins where available
#if defined(__GNUC__) && \
    ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || __GNUC__ >= 4)
inline int Bits::Log2Floor(uint32 n) {
  return n == 0 ? -1 : 31 ^ __builtin_clz(n);
}

inline int Bits::Log2FloorNonZero(uint32 n) {
  return 31 ^ __builtin_clz(n);
}

inline int Bits::FindLSBSetNonZero(uint32 n) {
  return __builtin_ctz(n);
}

inline int Bits::Log2Floor64(uint64 n) {
  return n == 0 ? -1 : 63 ^ __builtin_clzll(n);
}

inline int Bits::Log2FloorNonZero64(uint64 n) {
  return 63 ^ __builtin_clzll(n);
}

inline int Bits::FindLSBSetNonZero64(uint64 n) {
  return __builtin_ctzll(n);
}
#elif defined(_MSC_VER)
#include "base/bits-internal-windows.h"
#else
#include "base/bits-internal-unknown.h"
#endif

inline int Bits::CountOnesInByte(unsigned char n) {
  return num_bits[n];
}

inline uint8 Bits::ReverseBits8(unsigned char n) {
  n = ((n >> 1) & 0x55) | ((n & 0x55) << 1);
  n = ((n >> 2) & 0x33) | ((n & 0x33) << 2);
  return ((n >> 4) & 0x0f)  | ((n & 0x0f) << 4);
}

inline uint32 Bits::ReverseBits32(uint32 n) {
  n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);
  n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);
  n = ((n >> 4) & 0x0F0F0F0F) | ((n & 0x0F0F0F0F) << 4);
  n = ((n >> 8) & 0x00FF00FF) | ((n & 0x00FF00FF) << 8);
  return ( n >> 16 ) | ( n << 16);
}

inline uint64 Bits::ReverseBits64(uint64 n) {
#if defined(__x86_64__)
  n = ((n >> 1) & 0x5555555555555555ULL) | ((n & 0x5555555555555555ULL) << 1);
  n = ((n >> 2) & 0x3333333333333333ULL) | ((n & 0x3333333333333333ULL) << 2);
  n = ((n >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((n & 0x0F0F0F0F0F0F0F0FULL) << 4);
  n = ((n >> 8) & 0x00FF00FF00FF00FFULL) | ((n & 0x00FF00FF00FF00FFULL) << 8);
  n = ((n >> 16) & 0x0000FFFF0000FFFFULL) | ((n & 0x0000FFFF0000FFFFULL) << 16);
  return ( n >> 32 ) | ( n << 32);
#else
  return ReverseBits32( n >> 32 ) |
         (static_cast<uint64>(ReverseBits32( n &  0xffffffff )) << 32);
#endif
}

inline int Bits::Log2FloorNonZero_Portable(uint32 n) {
  // Just use the common routine
  return Log2Floor(n);
}

// Log2Floor64() is defined in terms of Log2Floor32(), Log2FloorNonZero32()
inline int Bits::Log2Floor64_Portable(uint64 n) {
  const uint32 topbits = static_cast<uint32>(n >> 32);
  if (topbits == 0) {
    // Top bits are zero, so scan in bottom bits
    return Log2Floor(static_cast<uint32>(n));
  } else {
    return 32 + Log2FloorNonZero(topbits);
  }
}

// Log2FloorNonZero64() is defined in terms of Log2FloorNonZero32()
inline int Bits::Log2FloorNonZero64_Portable(uint64 n) {
  const uint32 topbits = static_cast<uint32>(n >> 32);
  if (topbits == 0) {
    // Top bits are zero, so scan in bottom bits
    return Log2FloorNonZero(static_cast<uint32>(n));
  } else {
    return 32 + Log2FloorNonZero(topbits);
  }
}

// FindLSBSetNonZero64() is defined in terms of FindLSBSetNonZero()
inline int Bits::FindLSBSetNonZero64_Portable(uint64 n) {
  const uint32 bottombits = static_cast<uint32>(n);
  if (bottombits == 0) {
    // Bottom bits are zero, so scan in top bits
    return 32 + FindLSBSetNonZero(static_cast<uint32>(n >> 32));
  } else {
    return FindLSBSetNonZero(bottombits);
  }
}


#endif // _BITS_H_
