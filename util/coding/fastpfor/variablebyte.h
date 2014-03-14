/**
 * This code is released under the
 * Apache License Version 2.0 http://www.apache.org/licenses/.
 *
 * (c) Daniel Lemire, http://lemire.me/en/
 */

#ifndef _UTIL_CODING_PFOR_VARIABLEBYTE_H_
#define _UTIL_CODING_PFOR_VARIABLEBYTE_H_

#include <cassert>
#include "util/coding/fastpfor/util.h"
#include <string>

class VariableByte {
public:
  static void encodeArray(const uint32_t *in, const size_t length, uint32_t *out,
          size_t &nvalue);

  static const uint32_t * decodeArray(const uint32_t *in, const size_t length,
                                      uint32_t *out, size_t & nvalue);

  const char* name() const {
    return "VariableByte";
  }

  static uint8_t* encodeNum(uint32_t val, uint8_t* bout);

  static const uint8_t* decodeNum(const uint8_t* in, const uint8_t* end, uint32_t* out) {
    uint8_t shift = 0;
    for (uint32_t v = 0; end > in; shift += 7) {
        uint8_t c = *in++;
        v += ((c & 127) << shift);
        if (c & 128) {
          *out = v;
          return in;
        }
    }
    return nullptr;
  }
private:
  template<uint32_t i> static
  uint8_t extract7bits(const uint32_t val) {
      return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
  }

  template<uint32_t i> static
  uint8_t extract7bitsmaskless(const uint32_t val) {
      return static_cast<uint8_t>((val >> (7 * i)));
  }
};

inline void VariableByte::encodeArray(const uint32_t *in, const size_t length, uint32_t *out,
                                      size_t &nvalue) {
  uint8_t * bout = reinterpret_cast<uint8_t *> (out);
  const uint8_t * const initbout = reinterpret_cast<uint8_t *> (out);
  for (size_t k = 0; k < length; ++k) {
    bout = encodeNum(in[k], bout);
  }
  while ((bout - initbout) % 4 != 0) {
    *bout++ = 0;
  }
  const size_t storageinbytes = bout - initbout;
  nvalue = storageinbytes / 4;
}

inline const uint32_t * VariableByte::decodeArray(const uint32_t *in, const size_t length,
                                                  uint32_t *out, size_t & nvalue) {
    if (length == 0) {
      nvalue = 0;
      return in;//abort
    }
    const uint8_t * inbyte = reinterpret_cast<const uint8_t *> (in);
    const uint8_t * const endbyte = reinterpret_cast<const uint8_t *> (in
                    + length);
    const uint32_t * const initout(out);

    while (inbyte < endbyte) {
        unsigned int shift = 0;

        for (uint32_t v = 0; endbyte > inbyte; shift += 7) {
            uint8_t c = *inbyte++;
            v += ((c & 127) << shift);
            if ((c & 128)) {
                *out++ = v;
                break;
            }
        }
    }
    nvalue = out - initout;
    return reinterpret_cast<const uint32_t *> (inbyte);
}

inline uint8_t* VariableByte::encodeNum(uint32_t val, uint8_t* bout) {
  /**
       * Code below could be shorter. Whether it could be faster
       * depends on your compiler and machine.
       */
  if (val < (1U << 7)) {
    *bout++ = static_cast<uint8_t>(val | (1U << 7));
  } else if (val < (1U << 14)) {
      *bout++ = extract7bits<0> (val);
      *bout++ = extract7bitsmaskless<1> (val) | (1U << 7);
  } else if (val < (1U << 21)) {
      *bout++ = extract7bits<0> (val);
      *bout++ = extract7bits<1> (val);
      *bout++ = extract7bitsmaskless<2> (val) | (1U << 7);
  } else if (val < (1U << 28)) {
      *bout++ = extract7bits<0> (val);
      *bout++ = extract7bits<1> (val);
      *bout++ = extract7bits<2> (val);
      *bout++ = extract7bitsmaskless<3> (val) | (1U << 7);
  } else {
      *bout++ = extract7bits<0> (val);
      *bout++ = extract7bits<1> (val);
      *bout++ = extract7bits<2> (val);
      *bout++ = extract7bits<3> (val);
      *bout++ = extract7bitsmaskless<4> (val) | (1U << 7);
  }
  return bout;
}

#endif /* _UTIL_CODING_PFOR_VARIABLEBYTE_H_ */
