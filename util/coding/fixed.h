// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef CODING_H
#define CODING_H

#include <string>
#include "base/endian.h"

namespace coding {

// const uint8 kMaxVarintBytes = 10;
// const uint8 kMaxVarint32Bytes = 5;
const uint8 kFixed32Bytes = 4;
const uint8 kFixed64Bytes = 8;

inline uint8* EncodeFixed32(uint32 value, uint8* buf) {
  LittleEndian::Store32(buf, value);
  return buf + kFixed32Bytes;
}

inline uint8* EncodeFixed64(uint64 value, uint8* buf) {
  LittleEndian::Store64(buf, value);
  return buf + kFixed64Bytes;
}

inline void AppendFixed32(uint32 value, std::string* dest) {
  uint8 buf[kFixed32Bytes];
  EncodeFixed32(value, buf);
  dest->append(reinterpret_cast<char*>(buf), kFixed32Bytes);
}

inline void AppendFixed64(uint64 value, std::string* dest) {
  uint8 buf[kFixed64Bytes];
  EncodeFixed64(value, buf);
  dest->append(reinterpret_cast<char*>(buf), kFixed64Bytes);
}

inline uint32 DecodeFixed32(const uint8* buf) {
  return LittleEndian::Load32(reinterpret_cast<const char*>(buf));
}

inline const uint8* DecodeFixed64(const uint8* buf, uint64* val) {
  *val = LittleEndian::Load64(reinterpret_cast<const char*>(buf));
  return buf + kFixed64Bytes;
}

/*
inline uint8* EncodeVarint32(uint8* ptr, uint32_t v) {
  // Operate on characters as unsigneds
  const uint8 B = 128;
  if (v < (1<<7)) {
    *(ptr++) = v;
  } else if (v < (1<<14)) {
    *(ptr++) = v | B;
    *(ptr++) = v>>7;
  } else if (v < (1<<21)) {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = v>>14;
  } else if (v < (1<<28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = (v>>14) | B;
    *(ptr++) = v>>21;
  } else {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = (v>>14) | B;
    *(ptr++) = (v>>21) | B;
    *(ptr++) = v>>28;
  }
  return ptr;
}

inline uint8* EncodeVarint64(uint8* ptr, uint64_t v) {
  const uint8 B = 128;
  while (v >= B) {
    *(ptr++) = (v & (B-1)) | B;
    v >>= 7;
  }
  *(ptr++) = static_cast<unsigned char>(v);
  return ptr;
}

inline uint8 VarintLength(uint64_t v) {
  uint8 len = 1;
  while (v >= 128) {
    v >>= 7;
    len++;
  }
  return len;
}

// Attempts to parse a varint32 from a prefix of the bytes in [ptr,limit-1].
// Never reads a character at or beyond limit.  If a valid/terminated varint32
// was found in the range, stores it in *OUTPUT and returns a pointer just
// past the last byte of the varint32. Else returns NULL.  On success,
// "result <= limit". (Taken from snappy)
inline const uint8* Parse32WithLimit(
  const uint8* p, const uint8* limit, uint32* OUTPUT) {
  uint32 b, result;
  if (p >= limit) return NULL;
  b = *(p++); result = b & 127; if (b < 128) goto done;
  if (p >= limit) return NULL;
  b = *(p++); result |= (b & 127) <<  7; if (b < 128) goto done;
  if (p >= limit) return NULL;
  b = *(p++); result |= (b & 127) << 14; if (b < 128) goto done;
  if (p >= limit) return NULL;
  b = *(p++); result |= (b & 127) << 21; if (b < 128) goto done;
  if (p >= limit) return NULL;
  b = *(p++); result |= (b & 127) << 28; if (b < 16) goto done;
  return NULL;       // Value is too long to be a varint32
 done:
  *OUTPUT = result;
  return p;
}
*/

}  // namespace coding

#endif  // CODING_H
