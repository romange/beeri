// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _FILE_SSTABLE_SSTABLE_UTIL_H
#define _FILE_SSTABLE_SSTABLE_UTIL_H

#include <memory>
#include "file/file.h"
#include "file/sstable/sstable.h"
#include "strings/stringpiece.h"

namespace file {
namespace sstable {

template<typename T> base::Status ReadProtoRecords(StringPiece name,
                                                   std::function<void(StringPiece, T&&)> cb) {
  auto res = ReadonlyFile::Open(name);
  if (!res.ok()) return res.status;
  std::unique_ptr<ReadonlyFile> fl(res.obj);
  auto res2 = Table::Open(sstable::ReadOptions(), res.obj);
  if (!res2.ok()) return res2.status;
  std::unique_ptr<Table> tbl(res2.obj);
  std::unique_ptr<sstable::Iterator> it(tbl->NewIterator());
  T msg;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    CHECK(msg.ParseFromArray(it->value().data(), it->value().size()));
    cb(it->key(), std::move(msg));
  }
  CHECK(fl->Close().ok());
  return base::Status::OK;
}

}  // namespace sstable
}  // namespace file


#endif  // _FILE_SSTABLE_SSTABLE_UTIL_H
