// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _UTIL_CODING_PB_READER_H
#define _UTIL_CODING_PB_READER_H

#include <memory>
#include "strings/slice.h"
#include "util/coding/int_coder.h"
#include "util/coding/string_coder.h"
#include "base/status.h"

namespace google {
namespace protobuf {
class Descriptor;
class Message;
}  // namespace protobuf
}  // namespace google

namespace util {
namespace coding {
namespace gpb = ::google::protobuf;

class PbFieldReaderArray;

class PbFieldReader {
public:
  PbFieldReader(const gpb::FieldDescriptor* fd);
  ~PbFieldReader();

  void VisitPreOrder(std::function<void(PbFieldReader*)> cb);
  base::Status Init(const uint8* ptr, uint32 size);

  base::Status Read(gpb::Message* msg);

private:
  base::StatusObject<const uint8*> InitMeta(const uint8* start, const uint8* end);

  const gpb::FieldDescriptor* fd_;
  union {
    UInt32Decoder* arr_sizes;
    BitArray* has_bit;
  } u1_;

  union {
    UInt64Decoder* val_uint64;
    UInt32Decoder* val_uint32;
    StringDecoder* str_decoder;
  } u2_;
  BitArray::Iterator has_iter_;
  std::unique_ptr<PbFieldReaderArray> submsg_reader_;
};

class PbFieldReaderArray {
  std::vector<PbFieldReader*> fields_;
public:
  PbFieldReaderArray(const gpb::Descriptor* desc);
  ~PbFieldReaderArray();

  base::Status Read(gpb::Message* msg);

  void VisitPreOrder(std::function<void(PbFieldReader*)> cb) {
    for (PbFieldReader* v : fields_)
      v->VisitPreOrder(cb);
  }
};

class PbBlockDeserializer {
public:
  explicit PbBlockDeserializer(const gpb::Descriptor* desc);
  ~PbBlockDeserializer();

  base::Status Init(strings::Slice block, uint32* num_msgs);
  base::Status Read(gpb::Message* msg) {
    return root_.Read(msg);
  }
private:
  PbFieldReaderArray root_;
  std::vector<PbFieldReader*> readers_;
};

}  // namespace coding
}  // namespace util

#endif  // _UTIL_CODING_PB_READER_H