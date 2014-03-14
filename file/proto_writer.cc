// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <unordered_set>
#include <vector>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include "file/proto_writer.h"

#include "file/list_file.h"
#include "file/filesource.h"
#include "file/sstable/sstable_builder.h"
#include "util/lmdb/disk_table.h"

using base::Status;
using strings::Slice;
namespace file {

const char kProtoSetKey[] = "__proto_set__";
const char kProtoTypeKey[] = "__proto_type__";

namespace gpb = ::google::protobuf;

ProtoWriter::ProtoWriter(StringPiece filename, const gpb::Descriptor* dscr, Options opts)
    : dscr_(dscr), options_(opts) {
  const gpb::FileDescriptor* fd = dscr->file();
  gpb::FileDescriptorSet fd_set;
  std::unordered_set<const gpb::FileDescriptor*> unique_set({fd});
  std::vector<const gpb::FileDescriptor*> stack({fd});
  while (!stack.empty()) {
    fd = stack.back();
    stack.pop_back();
    fd->CopyTo(fd_set.add_file());

    for (int i = 0; i < fd->dependency_count(); ++i) {
      const gpb::FileDescriptor* child = fd->dependency(i);
      if (unique_set.insert(child).second) {
        stack.push_back(child);
      }
    }
  }
  string fd_set_str = fd_set.SerializeAsString();
  if (opts.format == LIST_FILE) {
    writer_.reset(new ListWriter(filename));
    writer_->AddMeta(kProtoSetKey, fd_set_str);
    writer_->AddMeta(kProtoTypeKey, dscr->full_name());
  } else if (opts.format == SSTABLE) {
    File* fl = CHECK_NOTNULL(Open(filename, "w"));
    sink_.reset(new Sink(fl, TAKE_OWNERSHIP));
    table_builder_.reset(new sstable::TableBuilder(sstable::Options(), sink_.get()));
    table_builder_->AddMeta(kProtoSetKey, fd_set_str);
    table_builder_->AddMeta(kProtoTypeKey, dscr->full_name());
  }
#if 0
  else if (opts.format == KEY_TABLE) {
    table_.reset(new util::DiskTable(opts.max_size_mb));
    Status status = table_->Open(filename);
    CHECK(status.ok()) << status;
    status = table_->BeginTransaction();
    status.AddError(table_->Put(strings::Slice(kProtoSetKey), strings::Slice(fd_set_str)));
    status.AddError(table_->Put(strings::Slice(kProtoTypeKey), strings::Slice(dscr->full_name())));
    status.AddError(table_->CommitTransaction());
    CHECK(status.ok()) << status;
  }
#endif
  else {
    LOG(FATAL) << "Invalid format " << opts.format;
  }
}

ProtoWriter::~ProtoWriter() {
  if (writer_) writer_->Flush();
}

util::Status ProtoWriter::Add(const ::google::protobuf::MessageLite& msg) {
  CHECK(writer_);
  CHECK_EQ(dscr_->full_name(), msg.GetTypeName());
  util::Status status;
  if (!was_init_) {
    status =  writer_->Init();
    if (!status.ok())
       return status;
    was_init_ = true;
  }

  return writer_->AddRecord(msg.SerializeAsString());
}

util::Status ProtoWriter::Add(strings::Slice key, const ::google::protobuf::MessageLite& msg) {
  CHECK_EQ(dscr_->full_name(), msg.GetTypeName());
#if 0
  if (options_.format == KEY_TABLE) {
    CHECK(table_);
    if (write_count_in_transaction_ == 0) {
      RETURN_IF_ERROR(table_->BeginTransaction());
    }
    util::Status status = table_->Put(key, msg.SerializeAsString());
    if (status.ok()) {
      if (++write_count_in_transaction_ >= options_.transaction_size) {
        write_count_in_transaction_ = 0;
        return table_->CommitTransaction();
      }
      return Status::OK;
    }
    table_->AbortTransaction();
    write_count_in_transaction_ = 0;
    return status;
  } else
#endif
   if (options_.format == SSTABLE) {
    if (!key.empty()) {
      char* ptr = arena_.Allocate(key.size());
      memcpy(ptr, key.data(), key.size());
      key = Slice(ptr, key.size());
    }
    int msg_size = msg.ByteSize();
    strings::Slice value;
    if (msg_size > 0) {
      uint8* ptr = reinterpret_cast<uint8*>(arena_.Allocate(msg_size));
      msg.SerializeWithCachedSizesToArray(ptr);
      value.set(ptr, msg_size);
    }
    k_v_vec_.emplace_back(key, value);
  } else {
    LOG(FATAL) << "Incorrect call";
  }
  return Status::OK;
}

util::Status ProtoWriter::Flush() {
  if (writer_) {
    return writer_->Flush();
  }
  if (options_.format == SSTABLE) {
    auto cmp = [](const KVSlice& a, const KVSlice& b) { return a.first.compare(b.first) < 0;};
    std::sort(k_v_vec_.begin(), k_v_vec_.end(), cmp);
    for (const auto& k_v : k_v_vec_) {
      table_builder_->Add(k_v.first, k_v.second);
    }
    RETURN_IF_ERROR(table_builder_->Finish());
    return sink_->Flush();
  }
#if 0
  if (write_count_in_transaction_ > 0) {
    RETURN_IF_ERROR(table_->CommitTransaction());
    write_count_in_transaction_ = 0;
  }
  return table_->Flush(true);
#endif
  return Status::OK;
}

}  // namespace file