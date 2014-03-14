// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _UTIL_CODING_INT_CODER_H
#define _UTIL_CODING_INT_CODER_H

#include <vector>
#include "base/integral_types.h"
#include "base/status.h"
#include "strings/slice.h"

namespace util {
class Sink;
namespace coding {

// see https://issues.apache.org/jira/browse/HIVE-4123 for inspiration.
// header:
/*
REPEAT: Used for repeated integer sequences. Stored repeated count is computed as
        repeated count - MIN_COUNT.
  1(or 2) byte header:

  3 bits for encoding type.
  5 bits Stores repeat count until 28 (included). For values 29-31, it denotes number of
         subsequent bytes required to represent repeat count >= than 28 (-28).
  optional 1-3 bytes  for storing count greater than 28 (Big endian - simpler to read))
  MAX REPEAT COUNT: 2^24 + 28.
  Blob - repeat value (varint width)

DIRECT32_256: Used for short integer sequences upto 256 integers of bitwidth <= 32.
  1st byte
    3 bits for encoding type
    5 bits for fixed bit width-1 of values in blob (32bits).
  1 byte of run-length.
  Blob - fixed width * (run length + 1) bits long (rounded up to byte alignment).

DIRECT_FPFOR - used for long integer sequences longer than 128 integers:
  1st byte- 3 bits for encoding type
  4 bytes - size of fpfor blob in bytes.
  Bpob - FPFOR blob
DELTA (1byte+ preheader):
  3 bits for encoding type
  2 bits for base number length in bytes. (32bit)
  1 bit for delta sign. Only 0 is supported.
  2 bits reserved.
  2nd byte and more - BASE NUMBER with bytes count denoted above (Big endian).
  next bytes - recusrive encoding of REPEAT or DIRECT.
DICTIONARY (followed by a single direct* header:
  1st byte:
    3 bits for encoding type.
    5 bits reserved.
  DIRECT header denoting integers to map, where the index of each integer is the value used
         later in this coder.
*/


class UInt32Encoder {
  typedef uint32 T;
public:
  typedef T value_type;

  template<typename Cont> size_t Encode(const Cont& src, bool encode_everything) {
    return Encode(src.data(), src.size(), encode_everything);
  }

  size_t Encode(const uint32* src, size_t length, bool encode_everything);

  void Reset() {
    buffer_.clear();
    values_.clear();
    direct_overhead_ = repeated_overhead_ = delta_overhead_ = 0;
  }

  void Swap(std::vector<uint8>* dest) { buffer_.swap(*dest); }

  size_t ByteSize() const { return buffer_.size(); }
  const std::vector<uint8>& buffer() const { return buffer_; }
  strings::Slice slice() const { return strings::Slice(buffer_.data(), buffer_.size()); }
  uint32 header_overhead() const {
    return direct_overhead_ + repeated_overhead_ + delta_overhead_;
  }
  uint32 repeated_overhead() const {
    return repeated_overhead_;
  }
  uint32 direct_overhead() const {
    return direct_overhead_;
  }
  uint32 delta_overhead() const {
    return delta_overhead_;
  }
private:
  void AddRepeatChunk(T val, uint32 count);

  void EncodeDirect(const uint32* start, const uint32* end, const uint8 bit_width);

  struct DeltaResult {
    uint8 max_delta_width;
    uint8 max_prebase_width;
    bool is_repeated = false;
    uint32 rep_delta;
  };

  static bool ShouldEncodeDelta(const uint32* start, const uint32* end, uint32 delta_cnt,
                                const uint8 max_width, DeltaResult* result);
  bool MaybeEncodeDelta(const uint32* start, const uint32* end, uint32 delta_cnt,
                        const uint8 max_width);

  void EncodeDelta(const uint32* start, const uint32 delta_cnt, const DeltaResult& result);

  std::vector<uint8> buffer_;
  std::vector<T> values_;
  uint32 repeated_overhead_ = 0;
  uint32 delta_overhead_ = 0;
  uint32 direct_overhead_ = 0;
};

class UInt64Encoder {
public:
  template<typename Cont> size_t Encode(const Cont& src, bool encode_everything) {
    return Encode(src.data(), src.size(), encode_everything);
  }
  size_t Encode(const uint64* src, size_t length, bool encode_everything);
  base::Status SerializeTo(Sink* sink) const;

  uint32 ByteSize() const { return hi_.ByteSize() + lo_.ByteSize() + 4;}
private:
  UInt32Encoder hi_, lo_;
};

class UInt32Decoder {
  typedef uint32 T;
public:
  typedef uint32 value_type;
  void Restart() {
    next_ = start_;
    delta_sign_ = delta_cnt_ = direct_count_ = repeated_count_ = buf_size_ = 0;
  }

  void Init(const uint8* buffer, uint32 size) {
    start_ = buffer;
    end_ = buffer + size;
    Restart();
  }
  // waiting for gcc 4.8 :( using UInt32Decoder::UInt32Decoder;

  UInt32Decoder(const uint8* buffer, uint32 size) : start_(buffer), end_(buffer + size),
      next_(buffer) {}

  UInt32Decoder()  {}

  bool Next(T* t);
private:
  T UnrollDeltaIfNeeded(T b) {
    if (delta_cnt_ == 1) {
      b = delta_base_ + b * delta_sign_;
      delta_base_ = b;
    }
    return b;
  }

  void LoadFirstDirectChunk();

  static constexpr unsigned int BUF_SIZE = 64;

  T tmp_buf_[BUF_SIZE];
  T delta_base_ = 0;

  const uint8* start_ = nullptr;
  const uint8* end_ = nullptr;
  const uint8* next_ = nullptr;

  uint32 direct_count_ = 0;  // how many direct ints are currently processed.
  uint32 repeated_count_ = 0;
  uint8 buf_size_ = 0;
  uint8 consumed_in_buf_ = 0;
  uint8 bit_width_ = 0;

  // Delta related variables.
  int8 delta_sign_ = 0;
  int8 delta_cnt_ = 0;
  std::vector<uint32> pfor_vec_;
  uint32 next_pfor_var_ = 0;
};

class UInt64Decoder {
public:
  UInt64Decoder(const uint8* buffer, uint32 size);
  UInt64Decoder(strings::Slice slice) : UInt64Decoder(slice.data(), slice.size()) {}

  bool Next(uint64* t);
  typedef uint64 value_type;

private:
  UInt32Decoder hi_, lo_;
};

/*
  Based on PLWAH algorithm for encoding bit stream.
  Each word is either literal or fill words.
  Literal word:
  MSB=0 and then 31 bits as is.
  Fill word: 25 bits LSB for the counter, then 5 bits denoting optional following word
             with only one bit that differs from the current fill, 1 fill bit and MSB=1,
  See papers: "Concise: Compressed ’n’ Composable Integer Set" and
              "Position List Word Aligned Hybrid"
*/
class BitArray {
  static constexpr uint32 FILL_WORD = 1U << 31;
  static constexpr uint32 MAX_COUNT = (1 << 25) - 1;

  static uint32 fill_word_count(const uint32 val) { return ((val & MAX_COUNT) + 1) * 31; }

  void FlushCount();
  void FlushFullLiteral();
public:
  BitArray() {}
  BitArray(uint32 sz, strings::Slice slice);

  // Adds bit to the array.
  void Push(bool b);

  void Finalize();

  uint32 size() const { return size_;}

  // index - zero based.
  bool Get(uint32 index) const;

  // Approximated if Finalize was not called.
  uint32 ByteSize() const { return data_.size() * sizeof(uint32); }

  void Clear();

  const std::vector<uint32>& data() const { return data_;};

  strings::Slice slice() const {
    return strings::Slice(reinterpret_cast<const uint8*>(data_.data()),
                          data_.size() * sizeof(uint32));
  }

  class Iterator;
  Iterator begin() const;
private:
  uint32 size_ = 0;

  // number of fill bits if rep_bit_val_ < 2 or number of defined bits in lit_word_ otherwise.
  uint32 bit_cnt_ = 0;

  // Currently filled lit_word_.
  uint32 lit_word_ = 0;

  // state variable: can be 0,1 (if we add the same bit) or two if we fill lit_word_.
  uint8 rep_bit_val_ = 2;

  std::vector<uint32> data_;
};

class BitArray::Iterator {
  const BitArray* bit_array_ = nullptr;
  uint32 data_indx_ = 0;
  uint32 val_ = 0;
  uint32 cnt_ = 0;
  uint32 total_ = 0;  // the total position from the beginning of iteration.

  void Advance() {
    if (--cnt_ > 0) {
      if ((val_ & FILL_WORD) == 0)
        val_ >>= 1;
      return;
    }
    uint32 diff = val_ & (31 << 25);  // diff position.
    if (diff) {
      if (total_ + 31 > bit_array_->size_) {
        cnt_ = bit_array_->size_ - total_;
      } else {
        cnt_ = 31;
      }
      uint32 single_bit = 1 << ((diff >> 25) - 1);
      val_ = (val_ & 1) ? (~single_bit) & ~FILL_WORD : single_bit;
      total_ += cnt_;
      return;
    }
    ++data_indx_;
    SetFromData();
  }

  void SetFromData();
public:
  Iterator() {}
  explicit Iterator(const BitArray& bit_arr);
  bool operator*() const {return (val_ & 1) == 1; }

  // Requires : !Done().
  Iterator& operator++() {
    Advance();
    return *this;
  }

  bool Done() const { return cnt_ == 0; }
};

inline BitArray::Iterator BitArray::begin() const {return Iterator(*this); }

inline void BitArray::Push(bool b) {
  ++size_;
  if (rep_bit_val_ == b) {
    ++bit_cnt_;
    return;
  }
  if (rep_bit_val_ == !b) {
    FlushCount();
  }
  // At this point we should put b into lit_word.
  lit_word_ |= (uint32(b) << bit_cnt_);
  if (++bit_cnt_ == 31) {
    FlushFullLiteral();
  }
}

}  // namespace coding
}  // namespace util

#endif  // _UTIL_CODING_INT_CODER_H