// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/string_coder.h"

#include <zlib.h>
#include "base/bits.h"
#include "strings/strcat.h"
#include "util/sinksource.h"

namespace util {
namespace coding {
using base::Status;

namespace {

// returns number of bytes - 1.
static inline uint8 NumFixedBytes(uint32 num) {
  return num == 0 ? 0 : Bits::FindMSBSetNonZero(num) / 8;
}

static inline uint8* StoreBigEndian(uint32 num, int8 bc, uint8* dest) {
  for (int8 i = bc; i >= 0; --i) {
    *dest ++ = (num >> i*8) & 0xFF;
  }
  return dest;
}

inline Status ParseError(string str) {
  return Status(base::StatusCode::IO_ERROR, std::move(str));
}

inline uint32 LoadBigEndian(const uint8* src, uint8 bc) {
  uint32 r = *src++;
  for (uint8 i = 0; i < bc; ++i) {
    r <<= 8;
    r |= *src++;
  }
  return r;
}

}  // namespace

StringEncoder::StringEncoder() {
  // : unique_strings_(0, VecSliceTraits(buf_), VecSliceTraits(buf_))
}

void StringEncoder::Add(strings::Slice slice) {
  // VecSlice item(buf_.size(), slice.size());
  buf_.insert(buf_.end(), slice.begin(), slice.end());
  /*auto res = unique_strings_.insert(item);
  if (!res.second) {
    buf_.resize(item.first);
  }
  offsets_.Push(res.first->first);*/
  lengths_.push_back(slice.size());
  total_size_ += slice.size();
  ++count_;
}

uint32 StringEncoder::ByteSize() const {
  return buf_.size() + buf2_.size() + header_sz_;
}

void StringEncoder::Finalize() {
  UInt32Encoder coder;
  coder.Encode(lengths_, true);
  coder.Swap(&buf2_);
  {
    uint8 bc = NumFixedBytes(buf2_.size());
    header_sz_ = bc + 2;
    header_ = RAW | (bc << 6);
  }
  if (buf_.size() > 63) {
    uLongf buf_size = compressBound(buf_.size());
    std::vector<uint8> compressed_buf(buf_size, 0);
    int res = compress(&compressed_buf.front(), &buf_size, buf_.data(), buf_.size());
    if (res != Z_OK) {
      LOG(ERROR) << "Compression error";
    } else if (buf_size + (buf_.size() / 6) <= buf_.size()) {
      VLOG(1) << "Compressing from " << buf_.size() << " to " << buf_size;
      uncompr_sz_ = buf_.size();
      uint8 ubc = NumFixedBytes(uncompr_sz_);
      header_sz_ += (ubc + 1);
      compressed_buf.resize(buf_size);
      compressed_buf.swap(buf_);
      header_ |= COMPRESSED | (ZLIB_TYPE << 2) | (ubc << 4);
    }
  }
  /*uint32 size = 0;
  for (StringPiece s : items_) {
    size += s.size();
    uint_encoder_.Push(s.size());
  }*/
}

base::Status StringEncoder::SerializeTo(Sink* sink) const {
  uint8 tmp_buf[header_sz_ + 4]; // 4 bytes padding in case of bugs :)
  uint8* next = tmp_buf;
  *next++ = header_;
  VLOG(1) << "Storing " << count_ << " strings " << " with " << buf2_.size()
          << " bytes for lengths and bufsize: " << buf_.size();
  if (uncompr_sz_) {
    next = StoreBigEndian(uncompr_sz_, (header_ >> 4) & 3, next);
  }
  next = StoreBigEndian(buf2_.size(), header_ >> 6, next);
  CHECK_EQ(header_sz_, next - tmp_buf);
  strings::Slice part(tmp_buf, header_sz_);
  RETURN_IF_ERROR(sink->Append(part));
  part.set(buf2_.data(), buf2_.size());
  RETURN_IF_ERROR(sink->Append(part));

  return sink->Append(strings::Slice(buf_.data(), buf_.size()));
}

Status StringDecoder::Init(strings::Slice slice) {
  uint32 total_sz = 0, lenc_sz;
  uint32 tmp;
  const uint8* next = slice.begin(), *dstart;
  uint8 header, enc_type;

  if (slice.size() < 2) goto err;
  header = *next++;
  enc_type = header & 3;
  if (enc_type == StringEncoder::COMPRESSED) {
    uint8 compr_type = (header >> 2) & 3;
    uint8 uncomp_sz_bc = (header >> 4) & 3;
    if (compr_type != StringEncoder::ZLIB_TYPE)
      return ParseError("Invalid compress method");
    inflated_buf_.resize(LoadBigEndian(next, uncomp_sz_bc));
    next += (uncomp_sz_bc + 1);
  }
  {
    uint8 bc = (header >> 6) & 3;
    if (next + bc > slice.end()) goto err;
    lenc_sz = LoadBigEndian(next, bc);
    next += (bc + 1);
  }
  if (lenc_sz == 0)
    return Status::OK;
  length_dec_.Init(next, lenc_sz);
  while (length_dec_.Next(&tmp)) {
    total_sz += tmp;
    ++count_;
  }
  VLOG(1) << "Loading " << count_ << " strings";
  dstart = next + lenc_sz;
  if (count_ == 0 || dstart > slice.end())
    goto err;
  if (enc_type == StringEncoder::COMPRESSED) {
    uLongf sz = inflated_buf_.size();
    VLOG(1) << "Decompressing into " << sz << " bytes from " << slice.end() - dstart << " bytes";
    int res = uncompress(&inflated_buf_.front(), &sz, dstart, slice.end() - dstart);
    if (res != Z_OK) return ParseError(StrCat("zlib error: ", zError(res)));
    if (sz != inflated_buf_.size())
      return ParseError("Inconsistent inflated size");
    raw_.set(inflated_buf_.data(), sz);
  } else {
    raw_.set(dstart, slice.end());
  }

  if (total_sz != raw_.size())
    return ParseError("Inconsistent encstring lengths");
  length_dec_.Restart();
  return Status::OK;
err:
  return ParseError("Bad encstring format");
}

bool StringDecoder::Next(strings::Slice* slice) {
  uint32 sz = 0;
  if (!length_dec_.Next(&sz)) return false;
  slice->set(raw_.begin(), sz);
  raw_.remove_prefix(sz);
  return true;
}


}  // namespace coding
}  // namespace util