// Copyright 2013, Beeri 15.  All rights reserved.
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
using std::string;

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
  } else {
    LOG(FATAL) << "Invalid format " << opts.format;
  }
}

ProtoWriter::~ProtoWriter() {
  if (writer_) writer_->Flush();
}

util::Status ProtoWriter::Add(const ::google::protobuf::MessageLite& msg) {
  CHECK(writer_);
  CHECK_EQ(dscr_->full_name(), msg.GetTypeName());
  if (!was_init_) {
    RETURN_IF_ERROR(writer_->Init());
    was_init_ = true;
  }

  return writer_->AddRecord(msg.SerializeAsString());
}

util::Status ProtoWriter::Add(strings::Slice key, const ::google::protobuf::MessageLite& msg) {
  CHECK_EQ(dscr_->full_name(), msg.GetTypeName());
   if (options_.format == SSTABLE) {
    if (!key.empty()) {
      char* ptr = arena_.Allocate(key.size());
      memcpy(ptr, key.data(), key.size());
      key = Slice(ptr, key.size());
    }
    int msg_size = msg.ByteSize();
    strings::Slice value;
    if (msg_size > 0) {
      char* ptr = arena_.Allocate(msg_size);
      msg.SerializeWithCachedSizesToArray(reinterpret_cast<uint8*>(ptr));
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
    if (!was_init_) {
      RETURN_IF_ERROR(writer_->Init());
      was_init_ = true;
    }
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
  return Status::OK;
}

}  // namespace file