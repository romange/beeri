// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/pb_writer.h"

#include "strings/strcat.h"
#include "strings/stringprintf.h"
#include "util/coding/varint.h"
#include "util/sinksource.h"

namespace util {
namespace coding {
namespace {

inline uint32 ByteSizeWithLength(uint32 v) {
  return v + Varint::Length32(v);
}

inline Status AppendUInt32(uint32 v, Sink* sink) {
  uint8 buf[Varint::kMax32];
  uint8* end = Varint::Encode32(buf, v);
  return sink->Append(strings::Slice(buf, end - buf));
}


Status SerializeEncoder(const vector<uint8>& vec, Sink* sink) {
  VLOG(1) << "Serializing uint32 array with " << vec.size() << " bytes + "
          << Varint::Length32(vec.size());
  Status status = AppendUInt32(vec.size(), sink);
  status.AddError(sink->Append(vec));
  return status;
}

inline Status SerializeBitArray(const BitArray& ba, Sink* sink) {
  Status status = AppendUInt32(ba.ByteSize() + Varint::Length32(ba.size()), sink);
  status.AddError(AppendUInt32(ba.size(), sink));
  status.AddError(sink->Append(ba.slice()));
  return status;
}

template<typename T> inline typename std::make_unsigned<T>::type
  EncodeZigZag(T t) {
   static_assert(std::is_same<T, int32>::value || std::is_same<T, int64>::value ||
                 std::is_same<T, uint64>::value || std::is_same<T, uint32>::value,
      "Must be ints");
  static constexpr int SHR = sizeof(T) * 8 - 1;
  return std::is_signed<T>::value ? (t >> SHR) ^ (t << 1) : t;
}

}  // namespace

PbFieldWriter::PbFieldWriter(const gpb::FieldDescriptor* fd) : fd_(fd) {
  if (fd_->cpp_type() == PBFD::CPPTYPE_MESSAGE) {
    msg_writer_.reset(new PbFieldWriterArray(fd_->message_type()));
  }
}

#define HANDLE_REP(T, enc) { const auto& arr = refl->GetRepeatedField<T>(msg, fd_); \
          for (T v : arr) \
            enc.push_back(EncodeZigZag<T>(v)); }

void PbFieldWriter::Add(const gpb::Message& msg) {
  const gpb::Reflection* refl = msg.GetReflection();
  string tmp;
  if (fd_->is_repeated()) {
    uint32 sz = refl->FieldSize(msg, fd_);
    arr_sizes_.push_back(sz);
    switch (fd_->cpp_type()) {
      case PBFD::CPPTYPE_INT32:
          HANDLE_REP(int32, val_uint32_);
      break;
      case PBFD::CPPTYPE_UINT32:
          HANDLE_REP(uint32, val_uint32_);
      break;
      case PBFD::CPPTYPE_INT64:
          HANDLE_REP(int64, val_uint64_);
      break;
      case PBFD::CPPTYPE_UINT64:
        /*{
          const auto& arr = refl->GetRepeatedField<uint64>(msg, fd_);
          for (auto v : arr) {
            LOG(INFO) << v;
          }
        }*/
          HANDLE_REP(uint64, val_uint64_);
      break;
      case PBFD::CPPTYPE_ENUM: {
          const auto* enum_descr = refl->GetEnum(msg, fd_);
          val_uint32_.push_back(EncodeZigZag<int>(enum_descr->number()));
      }
      break;
      case PBFD::CPPTYPE_MESSAGE: {
          const auto& arr = refl->GetRepeatedPtrField<gpb::Message>(msg, fd_);
          for (const gpb::Message& m : arr) {
            msg_writer_->Add(m);
          }
      }
      break;
      default:
        LOG(FATAL) << "Not implemented " << fd_->cpp_type_name();
    }
    return;
  }
  if (!fd_->is_required()) {
    bool exists = refl->HasField(msg, fd_);
    has_bit_.Push(exists);
    if (!exists)
      return;
  }
  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_UINT32:
      val_uint32_.push_back(refl->GetUInt32(msg, fd_));
    break;
    case PBFD::CPPTYPE_INT32:
      val_uint32_.push_back(EncodeZigZag<int32>(refl->GetInt32(msg, fd_)));
    break;
    case PBFD::CPPTYPE_UINT64:
      val_uint64_.push_back(refl->GetUInt64(msg, fd_));
    break;
    case PBFD::CPPTYPE_INT64:
      val_uint64_.push_back(EncodeZigZag<int64>(refl->GetInt64(msg, fd_)));
    break;
    case PBFD::CPPTYPE_STRING: {
      const string& str = refl->GetStringReference(msg, fd_, &tmp);
      str_encoder_.Add(str);
    }
    break;
    case PBFD::CPPTYPE_MESSAGE:
      msg_writer_->Add(refl->GetMessage(msg, fd_));
    break;
    case PBFD::CPPTYPE_DOUBLE: {
      double d = refl->GetDouble(msg, fd_);
      char* ptr = reinterpret_cast<char*>(&d);
      val_uint64_.push_back(*reinterpret_cast<uint64*>(ptr));
    }
    break;
    case PBFD::CPPTYPE_BOOL:
      val_bool_.Push(refl->GetBool(msg, fd_));
    break;
    default:
      LOG(FATAL) << "Not implemented: " << fd_->cpp_type_name();
  }
}

inline void Encode32(std::vector<uint32>* v, vector<uint8>* dest) {
  if (v->empty())
    return;
  UInt32Encoder encoder;
  encoder.Encode(*v, true);
  encoder.Swap(dest);
  std::vector<uint32>().swap(*v);
}

void PbFieldWriter::Finalize() {
  UInt32Encoder encoder;
  if (fd_->is_repeated()) {
    Encode32(&arr_sizes_, &data_buf_[0]);
  } else if (fd_->is_optional()) {
    has_bit_.Finalize();
  }

  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_STRING:
      str_encoder_.Finalize();
    break;
    case PBFD::CPPTYPE_UINT32: case PBFD::CPPTYPE_INT32: case PBFD::CPPTYPE_ENUM:
      Encode32(&val_uint32_, &data_buf_[1]);
    break;
    case PBFD::CPPTYPE_UINT64: case PBFD::CPPTYPE_INT64: case PBFD::CPPTYPE_DOUBLE:
      enc64_.Encode(val_uint64_, true);
      std::vector<uint64>().swap(val_uint64_);
    break;
    case PBFD::CPPTYPE_BOOL:
      val_bool_.Finalize();
    break;
    case PBFD::CPPTYPE_MESSAGE: // NOOP.
    break;
    default:
      LOG(FATAL) << "Not implemented" << fd_->cpp_type_name();
  }
}

uint32 PbFieldWriter::ByteSize() const {
  uint32 size = 0;
  if (fd_->is_repeated()) {
    size += ByteSizeWithLength(data_buf_[0].size());
  } else if (fd_->is_optional()) {
    size += ByteSizeWithLength(has_bit_.ByteSize() + Varint::Length32(has_bit_.size()));
  }
  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_STRING:
      size += str_encoder_.ByteSize();
    break;
    case PBFD::CPPTYPE_UINT32:
    case PBFD::CPPTYPE_INT32:
    case PBFD::CPPTYPE_ENUM:
      size += data_buf_[1].size();
    break;
    case PBFD::CPPTYPE_UINT64:
    case PBFD::CPPTYPE_INT64:
    case PBFD::CPPTYPE_DOUBLE:
      size += enc64_.ByteSize();
    break;
    case PBFD::CPPTYPE_BOOL:
      size += ByteSizeWithLength(val_bool_.ByteSize() + Varint::Length32(val_bool_.size()));
    break;
    case PBFD::CPPTYPE_MESSAGE: // NOOP.
    break;
    default:
      LOG(FATAL) << "Not implemented: " << fd_->cpp_type_name();
  }
  return size;
}

Status PbFieldWriter::SerializeTo(Sink* sink) const {
  VLOG(1) << "Serializing field " << fd_->full_name();
  if (fd_->is_repeated()) {
    RETURN_IF_ERROR(SerializeEncoder(data_buf_[0], sink));
  } else if (fd_->is_optional()) {
    RETURN_IF_ERROR(SerializeBitArray(has_bit_, sink));
  }
  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_STRING:
      VLOG(1) << "Serializing string with " << str_encoder_.ByteSize() << " total bytes.";
      RETURN_IF_ERROR(str_encoder_.SerializeTo(sink));
    break;
    case PBFD::CPPTYPE_UINT32:
    case PBFD::CPPTYPE_INT32:
    case PBFD::CPPTYPE_ENUM:
      RETURN_IF_ERROR(sink->Append(data_buf_[1]));
    break;
    case PBFD::CPPTYPE_UINT64:
    case PBFD::CPPTYPE_INT64:
    case PBFD::CPPTYPE_DOUBLE:
      RETURN_IF_ERROR(enc64_.SerializeTo(sink));
    break;
    case PBFD::CPPTYPE_BOOL:
      RETURN_IF_ERROR(SerializeBitArray(val_bool_, sink));
    break;
    case PBFD::CPPTYPE_MESSAGE: // NOOP.
    break;
    default:
      LOG(FATAL) << "Not implemented: " << fd_->cpp_type_name();
  }
  return Status::OK;
}

void PbFieldWriter::VisitPreOrder(std::function<void(PbFieldWriter*)> cb) {
  cb(this);
  if (msg_writer_)
    msg_writer_->VisitPreOrder(cb);
}

std::string PbFieldWriter::FieldName() const {
  return fd_->full_name();
}

PbFieldWriterArray::PbFieldWriterArray(const gpb::Descriptor* descr) {
  field_writer_.resize(descr->field_count());
  for (int i = 0; i < descr->field_count(); ++i) {
    const gpb::FieldDescriptor* fd = descr->field(i);
    field_writer_[i] = new PbFieldWriter(fd);
  }
}

PbFieldWriterArray::~PbFieldWriterArray() {
  for (PbFieldWriter* v : field_writer_)
    delete v;
}

PbBlockSerializer::PbBlockSerializer(const gpb::Descriptor* descr)
    : desc_(descr), root_fields_(descr) {
  root_fields_.VisitPreOrder([this](PbFieldWriter* writer) {
      all_fields_.push_back(writer);
    });
}

PbBlockSerializer::~PbBlockSerializer() {
}

void PbBlockSerializer::Add(const gpb::Message& msg) {
  CHECK(!was_finalized_);
  ++size_;
  root_fields_.Add(msg);
}

Status PbBlockSerializer::SerializeTo(Sink* sink) {
  if (!was_finalized_) {
    was_finalized_ = true;
    for (PbFieldWriter* fw : all_fields_) {
      fw->Finalize();
    }
  }

  uint32 total_size = 0;
  std::vector<uint32> field_sizes;
  // msg count followed by all the fields' byte sizes.
  field_sizes.push_back(size_);
  for (PbFieldWriter* fw : all_fields_) {
    VLOG(2) << "Field " << fw->FieldName() << ", size " << fw->ByteSize();
    total_size += fw->ByteSize();
    field_sizes.push_back(fw->ByteSize());
  }
  std::vector<uint8> fs_buf;
  Encode32(&field_sizes, &fs_buf);
  total_size += ByteSizeWithLength(fs_buf.size());

  VLOG(1) << "Serialize all fields column sizes. Block size is " << total_size;
  RETURN_IF_ERROR(SerializeEncoder(fs_buf, sink));
  for (PbFieldWriter* fw : all_fields_) {
    RETURN_IF_ERROR(fw->SerializeTo(sink));
  }
  return Status::OK;
}

std::string PbBlockSerializer::DebugStats() const {
  string res;
  for (const PbFieldWriter* fw : all_fields_) {
    StrAppend(&res, fw->FieldName(), " size ", fw->ByteSize(), ", ");
  }
  return res;
}
}  // namespace coding
}  // namespace util