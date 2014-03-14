/**
 * This code is released under the
 * Apache License Version 2.0 http://www.apache.org/licenses/.
 *
 * (c) Daniel Lemire, http://lemire.me/en/
 * and Owen Kaser
 */

#ifndef _UTIL_CODING_PFOR_UTIL
#define _UTIL_CODING_PFOR_UTIL

#include <cstdint>

/**
 * Computes the greatest common divisor
 */
constexpr __attribute__ ((const))
uint32_t gcd(uint32_t x, uint32_t y) {
    return (x % y) == 0 ? y :  gcd(y,x % y);
}

template <class T>
__attribute__ ((const))
T * padTo32bits(T * inbyte) {
    return reinterpret_cast< T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 3) & ~3);
}

template <class T>
__attribute__ ((const))
const T * padTo32bits(const T * inbyte) {
    return reinterpret_cast<const T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 3) & ~3);
}

template <class T>
__attribute__ ((const))
T * padTo64bits(T * inbyte) {
    return reinterpret_cast<T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 7) & ~7);
}

template <class T>
__attribute__ ((const))
const T * padTo64bits(const  T * inbyte) {
    return reinterpret_cast<const T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 7) & ~7);
}

template <class T>
__attribute__ ((const))
T * padTo128bits(T * inbyte) {
    return reinterpret_cast<T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 15) & ~15);
}

template <class T>
__attribute__ ((const))
const T * padTo128bits(const T * inbyte) {
    return reinterpret_cast<const T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 15) & ~15);
}

template <class T>
__attribute__ ((const))
T * padTo64bytes(T * inbyte) {
    return reinterpret_cast<T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 63) & ~63);
}

template <class T>
__attribute__ ((const))
const T * padTo64bytes(const T * inbyte) {
    return reinterpret_cast<T *> ((reinterpret_cast<uintptr_t> (inbyte)
            + 63) & ~63);
}

template <class T>
__attribute__ ((const))
bool needPaddingTo32Bits(const T * inbyte) {
    return (reinterpret_cast<uintptr_t> (inbyte) & 3) != 0;
}

template <class T>
__attribute__ ((const))
bool needPaddingTo64Bits(const T * inbyte) {
    return (reinterpret_cast<uintptr_t> (inbyte) & 7) != 0;
}

template <class T>
__attribute__ ((const))
bool needPaddingTo128Bits(const T * inbyte) {
    return (reinterpret_cast<uintptr_t> (inbyte) & 15) != 0;
}

template <class T>
bool  needPaddingTo64bytes(const T * inbyte) {
    return (reinterpret_cast<uintptr_t> (inbyte) & 63) != 0;
}

__attribute__ ((const))
inline uint32_t gccbits(const uint32_t v) {
#ifdef _MSC_VER
    if (v == 0) {
        return 0;
    }
    unsigned long answer;
    _BitScanReverse(&answer, v);
    return answer + 1;
#else
    return v == 0 ? 0 : 32 - __builtin_clz(v);
#endif
}

__attribute__ ((const))
inline bool divisibleby(size_t a, uint32_t x) {
    return (a % x == 0);
}

/**
 * compute the deltas, you do not want to use this
 * function if speed matters. This is only for convenience.
 */
template <class container>
container diffs(const container & in, const bool aredistinct) {
    container out;
    if (in.empty())
        return out;
    out.resize(in.size()-1);
    for (size_t k = 0; k < in.size() - 1; ++k)
        if (aredistinct)
            out.push_back(in[k + 1] - in[k] - 1);
        else
            out.push_back(in[k + 1] - in[k]);
    return out;
}

__attribute__ ((const))
inline uint32_t asmbits(const uint32_t v) {
#ifdef _MSC_VER
    return gccbits(v);
#else
    if (v == 0)
        return 0;
    uint32_t answer;
    asm("bsr %1, %0;" :"=r"(answer) :"r"(v));
    return answer + 1;
#endif
}

__attribute__ ((const))
inline uint32_t slowbits(uint32_t v) {
    uint32_t r = 0;
    while (v) {
        r++;
        v = v >> 1;
    }
    return r;
}

__attribute__ ((const))
inline uint32_t bits(uint32_t v) {
    uint32_t r(0);
    if (v >= (1U << 15)) {
        v >>= 16;
        r += 16;
    }
    if (v >= (1U << 7)) {
        v >>= 8;
        r += 8;
    }
    if (v >= (1U << 3)) {
        v >>= 4;
        r += 4;
    }
    if (v >= (1U << 1)) {
        v >>= 2;
        r += 2;
    }
    if (v >= (1U << 0)) {
        v >>= 1;
        r += 1;
    }
    return r;
}

#ifndef _MSC_VER
__attribute__ ((const))
constexpr uint32_t constexprbits(uint32_t v) {
    return v >= (1U << 15) ? 16 + constexprbits(v>>16) :
            (v >= (1U << 7)) ? 8 + constexprbits(v>>8) :
                    (v >= (1U << 3)) ? 4 + constexprbits(v>>4) :
                            (v >= (1U << 1)) ? 2 + constexprbits(v>>2) :
                                    (v >= (1U << 0)) ? 1 + constexprbits(v>>1) :
                                            0;
}
#else

template <int N>
struct exprbits
{
    enum { value = 1 + exprbits<(N >> 1)>::value };
};

template <>
struct exprbits<0>
{
    enum { value = 0 };
};

#define constexprbits(n) exprbits<n>::value

#endif

constexpr uint32_t div_roundup(uint32_t v, uint32_t divisor) {
    return (v + (divisor - 1)) / divisor;
}

template<class iterator>
__attribute__ ((pure))
uint32_t maxbits(const iterator & begin, const iterator & end) {
    uint32_t accumulator = 0;
    for (iterator k = begin; k != end; ++k) {
        accumulator |= *k;
    }
    return gccbits(accumulator);
}

template<class iterator>
uint32_t slowmaxbits(const iterator & begin, const iterator & end) {
    uint32_t accumulator = 0;
    for (iterator k = begin; k != end; ++k) {
        const uint32_t tb = gccbits(*k);
        if (tb > accumulator)
            accumulator = tb;
    }
    return accumulator;
}


#endif  // _UTIL_CODING_PFOR_UTIL
