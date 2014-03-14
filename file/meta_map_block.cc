// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _META_MAP_BLOCK_CC
#define _META_MAP_BLOCK_CC

#include "file/meta_map_block.h"
#include "util/coding/varint.h"

using base::Status;
using base::StatusCode;
using std::string;

namespace file {

void MetaMapBlock::EncodeTo(std::string* dest) const {
  // format: varint32 map size,
  // (varint string size, string data)+
  Varint::Append32(dest, meta_.size());
  for (const auto& k_v : meta_) {
    Varint::EncodeTwo32Values(dest, k_v.first.size(), k_v.second.size());
    dest->append(k_v.first).append(k_v.second);
  }
}

base::Status MetaMapBlock::DecodeFrom(strings::Slice input) {
  const uint8* ptr = input.begin(), *limit = input.end();
  uint32 sz = 0;
  if ((ptr = Varint::Parse32WithLimit(ptr, limit, &sz)) == nullptr) {
    return Status(StatusCode::IO_ERROR);
  }
  uint32 ksz = 0, vsz = 0;
  for (uint32 i = 0; i < sz; ++i) {
    if (ptr >= limit) {
      return Status(StatusCode::IO_ERROR);
    }
    ptr = Varint::DecodeTwo32Values(ptr, &ksz, &vsz);
    if (ptr + ksz + vsz > limit) {
      return Status(StatusCode::IO_ERROR);
    }
    string key;
    key.append(strings::charptr(ptr), ksz);
    ptr += ksz;
    meta_[key] = string(strings::charptr(ptr), vsz);
    ptr += vsz;
  }
  return Status::OK;
}

}  // namespace file

#endif  // _META_MAP_BLOCK_CC