// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/int_coder.h"

#include "base/logging.h"
#include "base/bits.h"
#include "base/endian.h"
#include "util/coding/bit_pack.h"
#include "util/coding/fastpfor/fastpfor.h"
#include "util/coding/varint.h"
#include "util/sinksource.h"

using std::vector;

namespace util {
namespace coding {

namespace {

constexpr uint8 kExtRepCnt = 29;

inline uint32 LoadBigEndian(uint8 bc, const uint8* & next) {
  uint32 res = *next++;
  switch(bc) {
    case 3:
      res <<= 8;
      res |= *next++;
    case 2:
      res <<= 8;
      res |= *next++;
    case 1:
      res <<= 8;
      res |= *next++;
    break;
  }
  return res;
}

namespace format {
  enum EncodingType {REPEATED_ENC = 0, DELTA_ENC = 1, DIRECT_256 = 2, DIRECT_PFOR = 3};

  static constexpr uint32 kDeltaThreshold = 8;
  // REPEAT chunk related constants.
  static constexpr uint32 kMinRepeatCnt = 8;
};
constexpr uint8 kHeaderTypeBits = 3;

}  // namespace

bool UInt32Encoder::ShouldEncodeDelta(const uint32* start, const uint32* end, uint32 delta_cnt,
                                      const uint8 max_width, DeltaResult* result) {
  // if numbers are: 1000, 800, 270, 275, 280, 285, end.
  // delta_cnt = 3, end = start + 6, base = end - 4.
  DCHECK_GT(delta_cnt, 0);
  DCHECK(start + delta_cnt < end);
  uint32 direct_price = PackedByteCount(end - start, max_width);
  const uint32* base = end - delta_cnt - 1;
  if (direct_price < 32 && base != start)  // do not bother create fragmented parts.
    return false;
  if (PackedByteCount(delta_cnt, max_width) < 10) // do not bother optimizing small chunks.
    return false;
  DCHECK_GE(base, start);
  uint32 max_delta = 0;
  uint32 min_delta = kuint32max;
  uint32 prev = *base;
  for (const uint32* p = base + 1; p != end; ++p) {
    DCHECK_GE(*p, prev);
    uint32 d = (*p) - prev;
    max_delta |= d;
    min_delta &= d;
    prev = *p;
  }
  uint32 delta_price;
  if (max_delta == min_delta && delta_cnt >= format::kMinRepeatCnt) {
    result->is_repeated = true;
    result->rep_delta = max_delta;
    delta_price = 7;
  } else {
    result->max_delta_width = Bits::Bsr(max_delta) + 1;
    delta_price = PackedByteCount(delta_cnt, result->max_delta_width) + 5;
    if (delta_price >= direct_price)
      return false;
  }
  if (base != start) {
    uint32 max_prebase_value = *start;
    for (const uint32* i = start + 1; i != base; ++i)
      max_prebase_value |= *i;
    result->max_prebase_width = Bits::Bsr(max_prebase_value) + 1;
    delta_price += (7 + PackedByteCount(base - start, result->max_prebase_width));
  }
  return delta_price < direct_price;
}

inline bool UInt32Encoder::MaybeEncodeDelta(
    const uint32* start, const uint32* end, uint32 delta_cnt, const uint8 max_width) {
  DeltaResult result;
  if (!ShouldEncodeDelta(start, end, delta_cnt, max_width, &result))
    return false;

  // if numbers are: 1000, 800, 270, 275, 280, 285, end.
  // delta_cnt = 3, end = start + 6, base = end - 4.
  const uint32* base = end - delta_cnt - 1;
  EncodeDirect(start, base, result.max_prebase_width);
  EncodeDelta(base, delta_cnt, result);
  return true;
}

size_t UInt32Encoder::Encode(const uint32* src, size_t length, bool encode_everything) {
  if (length == 0)
    return 0;
  const uint32* start = src;
  uint32 repeated_cnt = 1;
  uint32 delta_cnt = 0;
  T prev = *src, cur = 0;
  T max_val = prev;
  const uint32* in = src + 1;
  for (size_t index = 1; index <= length; ++index, prev = cur, ++in) {
    if (index < length) {
      cur = *in;
      if (cur == prev) {
        ++repeated_cnt;
        continue;
      }
    }
    if (repeated_cnt > 1) {
      if (repeated_cnt >= format::kMinRepeatCnt) {
        uint32 repeated_switch_cost = 12; // 10 bytes will cost to break started direct encoding.
        const uint32* end_direct = in - repeated_cnt;
        if (src == end_direct)
          repeated_switch_cost = 0;
        const uint8 max_width = Bits::Bsr(max_val) + 1;
        if (PackedByteCount(repeated_cnt, max_width) > repeated_switch_cost) {
          // encode all the numbers before repeated block.
          // In case we have delta encoding before, we need to decrease delta_cnt
          // because the last one in delta encoding is also the first one in repeated sequence.
          bool delta_flushed = (delta_cnt > format::kDeltaThreshold+1 &&
              MaybeEncodeDelta(src, in - repeated_cnt, delta_cnt - 1, max_width));
          if (!delta_flushed)
            EncodeDirect(src, in - repeated_cnt, max_width);
          // encode repeated block.
          AddRepeatChunk(prev, repeated_cnt);
          src = in;
          max_val = cur;  // reset max_val
          delta_cnt = 0;
          repeated_cnt = 1;
          continue;
        }
      }
      // We might still delta encode those series.
      delta_cnt += repeated_cnt - 1;
      repeated_cnt = 1;
    }
    if (cur > prev) {
      ++delta_cnt;
      max_val = std::max(max_val, cur);
      continue;
    }
    if (delta_cnt > format::kDeltaThreshold) {
      const uint8 max_width = Bits::Bsr(max_val) + 1;
      if (MaybeEncodeDelta(src, in, delta_cnt, max_width)) {
        src = in;
        max_val = cur;  // reset max_val
      }
    }
    delta_cnt = 0;
  }
  --in;
  DCHECK_LE(src, in);
  if (encode_everything || in - src >= 1024) {
    EncodeDirect(src, in, Bits::Bsr(max_val) + 1);
    src = in;
  }
  return src - start;
}

void UInt32Encoder::EncodeDirect(const uint32* start, const uint32* end, const uint8 bit_width) {
  uint32 size = end - start;
  if (size == 0) return;
  uint8* dest = nullptr;
  uint32 prev_size = buffer_.size();
  if (size < 128) {
    uint8 header = format::DIRECT_256 | ((bit_width - 1) << kHeaderTypeBits);
    uint32 bytes_count = PackedByteCount(size, bit_width) + BIT_PACK_MARGIN;
    // 1 header byte + 1 byte size.
    buffer_.resize(prev_size + 1 + 1 + bytes_count);
    dest = &buffer_.front() + prev_size;
    *dest++ = header;
    *dest++ = size - 1;
    dest = BitPack(start, size, bit_width, dest);
    direct_overhead_ += 2;
  } else {
    FastPFor pfor;
    size_t ints_written = pfor.maxCompressedLength(size);
    uint32 bytes_count = ints_written * sizeof(uint32);
    buffer_.resize(prev_size + 1 + 4 + bytes_count);
    dest = &buffer_.front() + prev_size;
    *dest++ = format::DIRECT_PFOR;
    pfor.encodeArray(start, size, reinterpret_cast<uint32_t*>(dest + 4), ints_written);
    LittleEndian::Store32(dest, ints_written * sizeof(uint32));
    dest += ints_written*sizeof(uint32_t) + 4;
    CHECK_LE(dest - buffer_.data(), buffer_.size());
    direct_overhead_ += 5;
  }
  buffer_.resize(dest - buffer_.data());
  VLOG(1) << "FlushDirect: sz " << size << " bit_width: " << int(bit_width) << " bytesize: "
          << buffer_.size() - prev_size << ", total: " << buffer_.size();
}

void UInt32Encoder::AddRepeatChunk(T val, uint32 count) {
  uint8 header = format::REPEATED_ENC;
  DCHECK_GE(count, format::kMinRepeatCnt);

  uint32 written_count = count - format::kMinRepeatCnt;
  size_t sz = buffer_.size();
  size_t max_sz = sz + Varint::MaxSize<T>() + 4;  // TYPE DEPENDENT.
  buffer_.resize(max_sz);
  uint8* dest = &buffer_.front() + sz;
  if (written_count >= kExtRepCnt) {
    written_count -= kExtRepCnt;
    uint8 bytes = Bits::Bsr(written_count) / 8;
    DCHECK_LT(bytes, 3);
    header |= ((kExtRepCnt + bytes) << kHeaderTypeBits);
    *dest++ = header;
    for (int8 j = bytes; j >= 0; --j) {  // Big endian.
      *dest++ = (written_count >> (j*8)) & 0xFF;
    }
    repeated_overhead_+= (bytes + 2);
  } else {
    header |= (written_count << kHeaderTypeBits);
    *dest++ = header;
    repeated_overhead_++;
  }
  dest = Varint::Encode(dest, val);
  VLOG(1) << "AddRepeatChunk: val " << val << ", count: " << count << " bytesize: "
          << dest - buffer_.data() - sz;
  buffer_.resize(dest - buffer_.data());
}

void UInt32Encoder::EncodeDelta(const uint32* start, const uint32 delta_cnt,
                                const DeltaResult& result) {
  DCHECK_GT(delta_cnt, 0);

  // CHECK_GE(values_.size(), format::kDeltaThreshold);
  // CHECK_EQ(delta_count_, values_.size() - 1);
  T base = *start++;
  uint8 bc = Bits::Bsr(base) / 8;

  size_t sz = buffer_.size();
  buffer_.resize(sz + bc + 1 + 1);  // bc is really byte count of base - 1.
  uint8* dest = &buffer_.front() + sz;
  *dest++ = format::DELTA_ENC | (bc << kHeaderTypeBits);

  // Store base.
  for (int8 i = bc; i >= 0; --i) {
    // VLOG(3) << "baseb: " << (base & 0xFF);
    *dest++ = (base >> i*8) & 0xFF;
  }
  repeated_overhead_+= (bc + 2);
  if (result.is_repeated) {
    AddRepeatChunk(result.rep_delta, delta_cnt);
  } else {
    values_.resize(delta_cnt);
    auto it = values_.begin();
    const uint32* end = start + delta_cnt;
    for (; start != end; ++start) {
      DCHECK_LE(base, *start);
      T delta = *start - base;
      *it++ = delta;
      base = *start;
    }
    EncodeDirect(values_.data(), values_.data() + delta_cnt, result.max_delta_width);
  }
  VLOG(1) << "FlushDelta: base " << base << " with delta count " << delta_cnt
          << " bytesize: " << buffer_.size() - sz;
}

size_t UInt64Encoder::Encode(const uint64* src, size_t length, bool encode_everything) {
  UInt32Encoder coder;
  vector<uint32> vals(length);
  for (size_t i = 0; i < length; ++i) {
    vals[i] = src[i];
  }
  size_t length2 = lo_.Encode(vals.data(), length, encode_everything);
  for (size_t i = 0; i < length2; ++i) {
    vals[i] = src[i] >> 32;
  }
  hi_.Encode(vals.data(), length2, true);
  return length2;
}

base::Status UInt64Encoder::SerializeTo(Sink* sink) const {
  uint8 buf[4];
  LittleEndian::Store32(buf, lo_.ByteSize());
  RETURN_IF_ERROR(sink->Append(strings::Slice(buf, 4)));
  RETURN_IF_ERROR(sink->Append(lo_.slice()));
  RETURN_IF_ERROR(sink->Append(hi_.slice()));

  return Status::OK;
}

bool UInt32Decoder::Next(T* t) {
  // Note - we could collapse those ifs into one single switch-case...
  if (repeated_count_ > 0) {
    --repeated_count_;
    *t = UnrollDeltaIfNeeded(*tmp_buf_);
    return true;
  }
  if (buf_size_ > consumed_in_buf_) {
    *t = UnrollDeltaIfNeeded(tmp_buf_[consumed_in_buf_++]);
    return true;
  }
  if (next_pfor_var_ < pfor_vec_.size()) {
    *t = UnrollDeltaIfNeeded(pfor_vec_[next_pfor_var_++]);
    if (next_pfor_var_ == pfor_vec_.size()) {
      next_pfor_var_ = 0;
      pfor_vec_.clear();
    }
    return true;
  }
  if (direct_count_ > 0) {
    buf_size_ = direct_count_ > 64 ? 64 : direct_count_;
    consumed_in_buf_ = 1;
    direct_count_ -= buf_size_;
    next_ = BitUnpack(next_, buf_size_, bit_width_, tmp_buf_);
    *t = UnrollDeltaIfNeeded(*tmp_buf_);
    return true;
  }
  if (next_ == end_) return false;
  DCHECK(direct_count_ == 0 && repeated_count_ == 0);
  // Reading the next header.
  uint8 header = *next_++;
  uint8 type = header & ((1 << kHeaderTypeBits) - 1);
  header >>= kHeaderTypeBits;
  bit_width_ = 0;
  switch (type) {
    case format::REPEATED_ENC:
      delta_cnt_ >>= 1;
      if (header < kExtRepCnt) {
        repeated_count_ = header + format::kMinRepeatCnt - 1;
      } else {
        repeated_count_ = LoadBigEndian(header - kExtRepCnt, next_);
        repeated_count_ += (format::kMinRepeatCnt + kExtRepCnt - 1);
      }
      VLOG(2) << "Reading repeated chunk with count " << repeated_count_;
      next_ = Varint::Parse(next_, tmp_buf_);
    break;
    case format::DELTA_ENC: {
      DCHECK_LE(delta_cnt_, 1);
      uint8 base_bc = header & 7;
      delta_sign_ = 1 - 2 * ((header >> 3) & 1); // 0 -> 1, 1 -> -1.
      delta_base_ = *next_++;
      for (uint8 j = 0; j < base_bc; ++j) {
        delta_base_ <<= 8;
        delta_base_ |= *next_++;
      }
      VLOG(2) << "Reading delta chunk with base " << delta_base_;
      delta_cnt_ = 2;
      *t = delta_base_;
      return true;
    }
    case format::DIRECT_256:
      delta_cnt_ >>= 1;
      bit_width_ = header + 1;
      direct_count_ = *next_++;
      ++direct_count_;
      LoadFirstDirectChunk();
    break;
    case format::DIRECT_PFOR: {
      // reset delta_cnt. This ensures that delta state is kept only
      // once after going through delta header.
      delta_cnt_ >>= 1;
      uint32 num_ints = LittleEndian::Load32(next_);
      next_ += 4;
      CHECK_EQ(0, num_ints % 4);
      const uint32_t* src = reinterpret_cast<const uint32_t*>(next_);
      next_ += num_ints;
      num_ints /= sizeof(uint32);
      size_t uncompressed_size = FastPFor::uncompressedLength(src, num_ints);
      VLOG(1) << "Reading " << num_ints*4 << " pfor bytes and uncompress them into "
              << uncompressed_size << " ints";
      pfor_vec_.resize(uncompressed_size);
      FastPFor pfor;
      pfor.decodeArray(src, num_ints, &pfor_vec_.front(), uncompressed_size);
      CHECK_EQ(uncompressed_size, pfor_vec_.size());
      next_pfor_var_ = 1;
      *t = UnrollDeltaIfNeeded(pfor_vec_.front());
      return true;
    }
  break;
  default:
    LOG(FATAL) << "Unknown header " << (header & 7);
  }
  *t = UnrollDeltaIfNeeded(*tmp_buf_);
  DCHECK_LE(next_, end_);
  return true;
}

void UInt32Decoder::LoadFirstDirectChunk() {
  VLOG(1) << "Reading " << direct_count_ << " numbers with width " << int(bit_width_);

  // We must read multiples of 8 numbers in order to decode full bytes.
  // Since we store num numbers minus 1, we limit here til 63.
  buf_size_ = direct_count_ > BUF_SIZE ? BUF_SIZE : direct_count_;
  direct_count_ -= buf_size_;
  next_ = BitUnpack(next_, buf_size_, bit_width_, tmp_buf_);
  consumed_in_buf_  = 1;
}

UInt64Decoder::UInt64Decoder(const uint8* buffer, uint32 size) {
  uint32 lo_size = LittleEndian::Load32(buffer);
  CHECK_LE(lo_size + 4, size);
  buffer += 4;
  lo_.Init(buffer, lo_size);
  buffer += lo_size;
  hi_.Init(buffer, size - lo_size - 4);
}

bool UInt64Decoder::Next(uint64* t) {
  uint32* ptr = reinterpret_cast<uint32*>(t);

  return lo_.Next(ptr) && hi_.Next(ptr + 1);
}

inline constexpr bool is_power_2(uint32 u) { return ((u-1) & u) == 0; }
inline bool fill_bit(uint32 v) { return ((v >> 30) & 1) == 1; }

BitArray::BitArray(uint32 sz, strings::Slice slice) : size_(sz) {
  CHECK_EQ(0, slice.size() % sizeof(uint32));
  data_.resize(slice.size() / sizeof(uint32));
  const uint32* src = reinterpret_cast<const uint32*>(slice.begin());
  std::copy(src, src + data_.size(), &data_.front());
}

void BitArray::FlushCount() {
  uint32 fill_word_cnt = bit_cnt_ / 31;
  uint32 remainder = bit_cnt_ % 31;
  // Each fill word can describe upto 1 << 25 31-bit words.
  uint32 max_word_cnt = fill_word_cnt >> 25;
  uint32 fill_word_base = (rep_bit_val_ == 0) ? FILL_WORD : FILL_WORD | (1 << 30);
  if (max_word_cnt) {
    data_.insert(data_.end(), max_word_cnt, fill_word_base | MAX_COUNT);
    DVLOG(2) << "Inserting " << max_word_cnt << " " << uint32(rep_bit_val_)
            << "-fill words with maxcount";
  }
  fill_word_cnt &= MAX_COUNT;
  if (fill_word_cnt) {
    --fill_word_cnt; // we store count - 1.
    DVLOG(2) << "Inserting a fill word " << uint32(rep_bit_val_) << " with count " << fill_word_cnt;
    data_.push_back(fill_word_base | fill_word_cnt);
  }
  // now handle the remainder.
  bit_cnt_ = remainder;
  lit_word_ = (rep_bit_val_ == 0) ? 0 : (1U << remainder) - 1;
  rep_bit_val_ = 2;
}

void BitArray::FlushFullLiteral() {
  // VLOG(2) << "lit_word_ " << lit_word_ << ", onesmask = " << kAllOnesMask;
  if (lit_word_ == 0 || ~lit_word_ == FILL_WORD) {
    bit_cnt_ = 31;
    rep_bit_val_ = (lit_word_ & 1);
    lit_word_ = 0;
    DVLOG(2) << "Having 31 bits of " << uint32(rep_bit_val_);
    return;
  }
  bit_cnt_ = 0;
  constexpr uint32 kAllOnesMask = (1U << 31) - 1;
  uint32 word_neg = (~lit_word_ & kAllOnesMask);
  // 0 - all zeroes but 1 bit, 1 - all ones but one bit, 2 - neither of those.
  uint8 single_bit_state = is_power_2(lit_word_) ? 0 : (is_power_2(word_neg) ? 1 : 2);
  constexpr uint32 kPosListMask = 127 << 25;  // 7 MSB bits.
  const uint32 kRequiredVal = FILL_WORD | (single_bit_state << 30);
  DVLOG(2) << "single_bit_state " << uint32(single_bit_state) << ", word: " << lit_word_
           << ", neg = " << word_neg << ", size " << size_;

  if (single_bit_state < 2 && !data_.empty() &&
      ((data_.back() & kPosListMask) == kRequiredVal)) {
    // We can put the index into the previous fill word.
    uint32 one_bit_word = (single_bit_state == 0) ? lit_word_ : word_neg;
    uint32 index = Bits::FindLSBSetNonZero(one_bit_word) + 1;
    DCHECK_LT(index, 32);
    DVLOG(2) << "Adding index " << index << " to fill word " << fill_bit(data_.back())
             << " with val: " << data_.back();
    data_.back() |= (index << 25);
  } else {
    data_.push_back(lit_word_);
  }
  lit_word_ = 0;
}

bool BitArray::Get(uint32 index) const {
  DCHECK_LT(index, size_);
  for (uint32 val : data_) {
    if (val & FILL_WORD) {
      uint32 cnt = fill_word_count(val);
      if (cnt > index) {
        return fill_bit(val);
      }
      index -= cnt;
      uint8 diff = (val >> 25) & 31;
      if (diff) {
        if (index < 31) {
          return (diff == index + 1) ^ fill_bit(val);
        }
        index -= 31;
      }
    } else {
      if (index < 31) {
        return ((val >> index) & 1) == 1;
      }
      index -= 31;
    }
  }
  if (rep_bit_val_ < 2 && bit_cnt_ > index) {
    return rep_bit_val_;
  }
  if (bit_cnt_ > index) {
    return bool((lit_word_ >> index) & 1);
  }
  LOG(FATAL) << "Index out of bounds " << size_;
  return false;
}

void BitArray::Clear() {
  size_ = lit_word_ = bit_cnt_ = 0;
  data_.clear();
  rep_bit_val_ = 2;
}

void BitArray::Finalize() {
  if (rep_bit_val_ < 2) {
    FlushCount();
    if (bit_cnt_ > 0) {
      data_.push_back(lit_word_);
      lit_word_ = 0;
      bit_cnt_ = 0;
    }
    return;
  }
  if (bit_cnt_ > 0) {
    FlushFullLiteral();
  }
}

BitArray::Iterator::Iterator(const BitArray& bit_arr) : bit_array_(&bit_arr) {
  SetFromData();
}

void BitArray::Iterator::SetFromData() {
  if (data_indx_ < bit_array_->data().size()) {
    val_ = bit_array_->data()[data_indx_];
    if (val_ & FILL_WORD) {
      cnt_ = fill_word_count(val_);
      val_ = (val_ & ~MAX_COUNT) | (val_ >> 30);
    } else if (total_ + 31 <= bit_array_->size_) {
      cnt_ = 31;
    } else {
      cnt_ = bit_array_->size_ - total_;
    }
  } else if (total_ < bit_array_->size_) {
    DCHECK_EQ(bit_array_->bit_cnt_, bit_array_->size_ - total_);
    cnt_ = bit_array_->bit_cnt_;
    val_ = bit_array_->rep_bit_val_ < 2 ? (bit_array_->rep_bit_val_ | FILL_WORD) :
                                         bit_array_->lit_word_;
  } else {
    cnt_ = 0;
  }
  total_ += cnt_;
}

}  // namespace coding
}  // namespace util // no break.
