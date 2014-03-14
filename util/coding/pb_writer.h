// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _UTIL_CODING_PB_WRITER_H
#define _UTIL_CODING_PB_WRITER_H

#include "base/arena.h"
#include "util/coding/int_coder.h"
#include "util/coding/string_coder.h"
#include "util/status.h"
#include <memory>
#include <vector>

namespace google {
namespace protobuf {
class FieldDescriptor;
class Message;
}  // namespace protobuf
}  // namespace google

namespace util {

class Sink;

namespace coding {

class PbFieldWriterArray;
namespace gpb = ::google::protobuf;

class PbFieldWriter {
public:
  PbFieldWriter(const gpb::FieldDescriptor* fd);

  void Add(const gpb::Message& msg);
  void Finalize();
  uint32 ByteSize() const;

  void VisitPreOrder(std::function<void(PbFieldWriter*)> cb);

  Status SerializeTo(Sink* sink) const;
  std::string FieldName() const;
private:
  typedef gpb::FieldDescriptor PBFD;
  const gpb::FieldDescriptor* fd_;

  std::vector<uint32> arr_sizes_, val_uint32_;
  std::vector<uint64> val_uint64_;
  std::vector<uint8> data_buf_[2];
  UInt64Encoder enc64_;

  util::coding::BitArray has_bit_, val_bool_;
  StringEncoder str_encoder_;
  std::unique_ptr<PbFieldWriterArray> msg_writer_;

  PbFieldWriter(const PbFieldWriter&) = delete;
  void operator=(const PbFieldWriter&) = delete;
};

class PbFieldWriterArray {
  std::vector<PbFieldWriter*> field_writer_;
  PbFieldWriterArray(const PbFieldWriterArray&) = delete;
  void operator=(const PbFieldWriterArray&) = delete;
public:
  explicit PbFieldWriterArray(const gpb::Descriptor* desc);
  ~PbFieldWriterArray();

  void Add(const gpb::Message& msg) {
    for (PbFieldWriter* v : field_writer_)
      v->Add(msg);
  }

  void VisitPreOrder(std::function<void(PbFieldWriter*)> cb) {
    for (PbFieldWriter* v : field_writer_)
      v->VisitPreOrder(cb);
  }
};

/*
 Serializes data from multiple messages into a dense memory block.
 Each field (and subfield) is stored in column oriented format.
 Note that schema for the message is not stored here.
 Usage:
 PbBlockSerializer block_serializer(descriptor);
 for (; many times; ) {
    msg_serializer.Add(msg);  // packs them in memory
 }
 msg_serializer.SerializeToSink(&sink);  // writes all the data into sink.
*/

class PbBlockSerializer {
  const gpb::Descriptor* desc_;
  PbFieldWriterArray root_fields_;
  std::vector<PbFieldWriter*> all_fields_;
  uint32 size_ = 0;
  bool was_finalized_ = false;
public:
  explicit PbBlockSerializer(const gpb::Descriptor* desc);
  ~PbBlockSerializer();

  void Add(const gpb::Message& msg);
  Status SerializeTo(Sink* sink);

  // Number of messages addes so far.
  uint32 NumEntries() const { return size_; }

  std::string DebugStats() const;
};

}  // namespace coding
}  // namespace util

#endif  // _UTIL_CODING_PB_WRITER_H