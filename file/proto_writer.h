// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _PROTO_WRITER_H
#define _PROTO_WRITER_H

#include <memory>

#include "strings/stringpiece.h"
#include "base/status.h"
#include "base/arena.h"

namespace google {
namespace protobuf {
class Descriptor;
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace util {
class DiskTable;
class Sink;
}  // namespace util

namespace file {
class ListWriter;

extern const char kProtoSetKey[];
extern const char kProtoTypeKey[];

namespace sstable {
class TableBuilder;
}  // namespace sstable

class ProtoWriter {
  std::unique_ptr<ListWriter> writer_;
  // std::unique_ptr<util::DiskTable> table_;
  std::unique_ptr<util::Sink> sink_;
  std::unique_ptr<sstable::TableBuilder> table_builder_;

  typedef std::pair<strings::Slice, strings::Slice> KVSlice;
  std::vector<KVSlice> k_v_vec_;
  const ::google::protobuf::Descriptor* dscr_;
  base::Arena arena_;

  bool was_init_ = false;
  uint32 write_count_in_transaction_ = 0;
public:
  enum Format {LIST_FILE, KEY_TABLE, SSTABLE};

  struct Options {
    Format format;

    // relevant for disk table, which must know in advance how big the table is.
    uint32 max_size_mb = 100;
    uint32 transaction_size = 1000; // number of writes per transaction.

    Options() : format(LIST_FILE) {}
  };

  ProtoWriter(StringPiece filename, const ::google::protobuf::Descriptor* dscr,
              Options opts = Options());

  ~ProtoWriter();

  base::Status Add(const ::google::protobuf::MessageLite& msg);

  // Used for key-value tables.
  base::Status Add(strings::Slice key, const ::google::protobuf::MessageLite& msg);

  base::Status Flush();

  const ListWriter* writer() const { return writer_.get();}
private:
  Options options_;
};

}  // namespace file

#endif  // _PROTO_WRITER_H