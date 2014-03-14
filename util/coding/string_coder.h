// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _UTIL_CODING_STRING_CODER_H
#define _UTIL_CODING_STRING_CODER_H

#include <vector>
// #include <sparsehash/dense_hash_set>
#include "base/integral_types.h"
#include "base/status.h"
#include "strings/slice.h"
#include "strings/stringpiece.h"
// #include "strings/unique_strings.h"
#include "util/coding/int_coder.h"

namespace util {
class Sink;
namespace coding {

/*
  Header:
    Ideas: to use 256 bitmask to check the size of the alphabet. To use min/max to check
      the effective range as well. Probably needed only if we stay with snappy because stronger
      algorithms like gzip probably already do most of the stuff.
    HEADER:
         1+ byte: - 2 LSB bits for encoding type,
                     4 bits (2-5) are reserved.
                     2 MSB bits describe number of bytes needed to store byte size for
                       encoded lengths array.
                  optional header bytes depending on its type.
    Header is followed by numbers as specified by header bits. Numbers are stored in Big Endian.
    if strings count = 0, it finishes at this point otherwise data block continues.
    the next data blob is encoded lengths array and finally encoded data blob.

    RAW STRING encoding:
         1-4 bytes for encoded size
      count, string sizes, literal blob optionally compressed.
    COMPRESSED STRING:
      1 header byte:
        bits 2-3: compress method.
        bits 4-5: number of bytes after this byte that represent big endian integer
                  for uncompressed (original) block byte size.
    DICT_ENC: TBD.
          RAW STRING array containing the unique strings. Their index in the array is assigned to
                 the unique id used below..


*/
class StringEncoder {
  // typedef std::pair<uint32, uint32> VecSlice;  // offset, length pair.
  std::vector<uint8> buf_;             // continous char buffer.
  std::vector<uint8> buf2_;
  uint32 uncompr_sz_ = 0;
  uint8 header_ = 0;
  uint8 header_sz_ = 5;
#if 0
  class VecSliceTraits {
    const std::vector<uint8>* buf_;
  public:
    VecSliceTraits(const std::vector<uint8>& buf) : buf_(&buf) {}

    // hash function
    size_t operator()(VecSlice vs) const {
      return std::hash<strings::Slice>()(strings::Slice(buf_->data() + vs.first, vs.second));
    }

    // equality.
    bool operator()(VecSlice a, VecSlice b) const {
      if (a.second != b.second)
        return false;
      if (a.second == 0)
        return true;
      return memcmp(buf_->data() + a.first, buf_->data() + b.first, a.second) == 0;
    }
  };
#endif
  //::google::dense_hash_set<VecSlice, VecSliceTraits, VecSliceTraits> unique_strings_;
  // UInt32Encoder lengths_;  // for each string instance we add its length in buf array.
  std::vector<uint32> lengths_;
  uint32 total_size_ = 0;
  uint32 count_ = 0;
  enum State { APPEND, FINALIZE} state_ = APPEND;
  enum {RAW = 0, COMPRESSED = 1};
  enum {ZLIB_TYPE = 0};
  friend class StringDecoder;
public:
  StringEncoder();

  uint32 ByteSize() const;

  void AddStringPiece(StringPiece st) { Add(st.as_slice()); }
  void Add(strings::Slice st);

  void Finalize();

  base::Status SerializeTo(Sink* sink) const;
};

class StringDecoder {
  uint32 count_ = 0;
  UInt32Decoder length_dec_;
  strings::Slice raw_;
  std::vector<uint8> inflated_buf_;
public:
  base::Status Init(strings::Slice slice);

  uint32 size() const { return count_; }

  bool Next(strings::Slice* st);

  bool Next(StringPiece* st) {
    strings::Slice sl;
    if (!Next(&sl)) return false;
    st->set(sl.charptr(), sl.size());
    return true;
  }
};

}  // namespace coding
}  // namespace util

#endif  // _UTIL_CODING_STRING_CODER_H
