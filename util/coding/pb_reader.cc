// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/coding/pb_reader.h"

#include "strings/strcat.h"
#include "util/coding/int_coder.h"
#include "util/coding/varint.h"

namespace util {
namespace coding {

using base::Status;
using strings::Slice;

namespace gpb = ::google::protobuf;

namespace {
typedef gpb::FieldDescriptor PBFD;

inline Status RangeError(const char* str) {
  return Status(base::StatusCode::INTERNAL_ERROR, StrCat("Range error ", str));
}

template<typename T> inline void ResetPtr(T* & ptr, T* new_ptr) {
  if (ptr) {
    delete ptr;
  }
  ptr = new_ptr;
}

template<typename T> inline T DecodeZigZag(typename std::make_unsigned<T>::type n) {
  return std::is_signed<T>::value ? (n >> 1) ^ -static_cast<T>(n & 1) : n;
}

template<typename Coder, typename T> inline Status FillRepeatedField(
    const gpb::Reflection* refl, const gpb::FieldDescriptor* fd,
    uint32 size, Coder* source, gpb::Message* msg) {
  gpb::RepeatedField<T>* dest = refl->MutableRepeatedField<T>(msg, fd);
  dest->Clear();
  dest->Reserve(size);
  typename Coder::value_type val = 0;
  for (uint32 i = 0; i < size; ++i) {
    if (!source->Next(&val)) {
      return RangeError("Not enough repeated int");
    }
    dest->Add(DecodeZigZag<T>(val));
  }
  return Status::OK;
}

}  // namespace

PbFieldReader::PbFieldReader(const gpb::FieldDescriptor* fd) : fd_(fd) {
  if (fd_->cpp_type() == PBFD::CPPTYPE_MESSAGE) {
    submsg_reader_.reset(new PbFieldReaderArray(fd_->message_type()));
  }
  u1_.has_bit = nullptr;
  u2_.val_uint32 = nullptr;
}

PbFieldReader::~PbFieldReader() {
  if (fd_->is_repeated()) {
    delete u1_.arr_sizes;
  } else if (fd_->is_optional()) {
    delete u1_.has_bit;
  }
  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_INT32:
    case PBFD::CPPTYPE_UINT32:
      delete u2_.val_uint32;
    break;
    case PBFD::CPPTYPE_INT64:
    case PBFD::CPPTYPE_UINT64:
      delete u2_.val_uint64;
    break;
    case PBFD::CPPTYPE_STRING:
      delete u2_.str_decoder;
    default:
    break;
  }
}

void PbFieldReader::VisitPreOrder(std::function<void(PbFieldReader*)> cb) {
  cb(this);
  if (submsg_reader_)
    submsg_reader_->VisitPreOrder(cb);
}

Status PbFieldReader::Init(const uint8* ptr, uint32 size) {
  VLOG(2) << "PbFieldReader::Init " << fd_->full_name() << ", size: " << size;
  const uint8* end = ptr + size;
  if (fd_->is_repeated() || fd_->is_optional()) {
    auto res = InitMeta(ptr, end);
    if (!res.ok()) return res.status;
    ptr = res.obj;
  }
  if (fd_->cpp_type() == PBFD::CPPTYPE_MESSAGE) {
    DCHECK_EQ(0, end - ptr);
    return Status::OK;
  }
  uint32 data_sz2 = end-ptr;
  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_STRING:
      ResetPtr(u2_.str_decoder, new StringDecoder());
      RETURN_IF_ERROR(u2_.str_decoder->Init(Slice(ptr, data_sz2)));
    break;
    case PBFD::CPPTYPE_UINT32:
    case PBFD::CPPTYPE_INT32:
    case PBFD::CPPTYPE_ENUM:
      ResetPtr(u2_.val_uint32, new UInt32Decoder(ptr, data_sz2));
    break;
    case PBFD::CPPTYPE_UINT64:
    case PBFD::CPPTYPE_INT64:
      ResetPtr(u2_.val_uint64, new UInt64Decoder(ptr, data_sz2));
    break;
    default:
      LOG(FATAL) << "Not implemented " << fd_->cpp_type_name();
  }
  return Status::OK;
}

Status PbFieldReader::Read(gpb::Message* msg) {
  uint32 vals_count = 1;
  const gpb::Reflection* refl = msg->GetReflection();
  Status st;
  if (fd_->is_repeated()) {
    if (!u1_.arr_sizes->Next(&vals_count)) {
      return RangeError("r5");
    }
    switch (fd_->cpp_type()) {
      case PBFD::CPPTYPE_INT32:
        st = FillRepeatedField<UInt32Decoder, int32>(refl, fd_, vals_count, u2_.val_uint32, msg);
      break;
      case PBFD::CPPTYPE_UINT32:
        st = FillRepeatedField<UInt32Decoder, uint32>(refl, fd_, vals_count, u2_.val_uint32, msg);
      break;
      case PBFD::CPPTYPE_INT64:
        st = FillRepeatedField<UInt64Decoder, int64>(refl, fd_, vals_count, u2_.val_uint64, msg);
      break;
      case PBFD::CPPTYPE_UINT64:
        st = FillRepeatedField<UInt64Decoder, uint64>(refl, fd_, vals_count, u2_.val_uint64, msg);
      break;
      case PBFD::CPPTYPE_MESSAGE:{
        auto* arr = refl->MutableRepeatedPtrField<gpb::Message>(msg, fd_);
        arr->Clear();
        arr->Reserve(vals_count);
        for (uint32 i = 0; i < vals_count; ++i) {
          gpb::Message* item = refl->AddMessage(msg, fd_);
          st = submsg_reader_->Read(item);
          if (!st.ok()) return st;
        }
      }
      break;
      default:
        LOG(FATAL) << "Not implemented" << fd_->cpp_type_name();
    }
    return st;
  }  // repeated
  if (fd_->is_optional()) {
    if (has_iter_.Done()) {
      return RangeError("r6");
    }

    bool has = *has_iter_;
    ++has_iter_;
    if (!has)
      return Status::OK;
  }

#define HANDLE_VAL(T, dec, func) {           \
      std::remove_reference<decltype(*dec)>::type::value_type val = 0;  \
      if (!dec->Next(&val)) return RangeError(#T " finito");  \
      refl->func(msg, fd_, DecodeZigZag<T>(val)); }

  switch (fd_->cpp_type()) {
    case PBFD::CPPTYPE_STRING: {
      Slice sl;
      if (!u2_.str_decoder->Next(&sl)) {
        return RangeError("Corrupt string");
      }
      refl->SetString(msg, fd_, sl.as_string());
    }
    break;
    case PBFD::CPPTYPE_UINT32:
        HANDLE_VAL(uint32, u2_.val_uint32, SetUInt32);
    break;
    case PBFD::CPPTYPE_INT32:
        HANDLE_VAL(int32, u2_.val_uint32, SetInt32);
    break;
    case PBFD::CPPTYPE_UINT64:
        HANDLE_VAL(uint64, u2_.val_uint64, SetUInt64);
    break;
    case PBFD::CPPTYPE_INT64:
        HANDLE_VAL(int64, u2_.val_uint64, SetInt64);
    break;
    case PBFD::CPPTYPE_MESSAGE: {
      Status st = submsg_reader_->Read(refl->MutableMessage(msg, fd_));
      if (!st.ok()) return st;
    }
    break;
    default:
      LOG(FATAL) << "Not implemented" << fd_->cpp_type_name();
  }
  return Status::OK;
}

base::StatusObject<const uint8*> PbFieldReader::InitMeta(
      const uint8* start, const uint8* end) {
  uint32 data_sz = 0;
  const uint8* next = Varint::Parse32WithLimit(start, end, &data_sz);
  if (next == nullptr) {
    return RangeError("r1");
  }
  start = next + data_sz;
  if (fd_->is_repeated()) {
    ResetPtr(u1_.arr_sizes, new UInt32Decoder(next, data_sz));
    return start;
  }
  uint32 bit_count = 0;
  next = Varint::Parse32WithLimit(next, start, &bit_count);
  if (next == nullptr) {
    return RangeError("r2");
  }
  strings::Slice bit_slice(next, start - next);
  if (bit_slice.size() % sizeof(uint32) != 0) {
    return Status(base::StatusCode::INTERNAL_ERROR, "Invalid bit array size");
  }
  ResetPtr(u1_.has_bit, new BitArray(bit_count, bit_slice));
  has_iter_ = u1_.has_bit->begin();
  return start;
}

PbFieldReaderArray::PbFieldReaderArray(const gpb::Descriptor* descr) {
  fields_.resize(descr->field_count());
  for (int i = 0; i < descr->field_count(); ++i) {
    const gpb::FieldDescriptor* fd = descr->field(i);
    fields_[i] = new PbFieldReader(fd);
  }
}

PbFieldReaderArray::~PbFieldReaderArray() {
  for (PbFieldReader* v : fields_)
    delete v;
}

Status PbFieldReaderArray::Read(gpb::Message* msg) {
  for (PbFieldReader* field : fields_) {
    RETURN_IF_ERROR(field->Read(msg));
  }
  return Status::OK;
}

PbBlockDeserializer::PbBlockDeserializer(const gpb::Descriptor* desc) : root_(desc) {
  root_.VisitPreOrder([this](PbFieldReader* r) { readers_.push_back(r); });
}

PbBlockDeserializer::~PbBlockDeserializer() {

}

Status PbBlockDeserializer::Init(strings::Slice block, uint32* num_msgs) {
  DCHECK(!block.empty());
  uint32 field_sizes_arr_sz = 0;
  const uint8* next = Varint::Parse32(block.begin(), &field_sizes_arr_sz);
  if (next + field_sizes_arr_sz > block.end())
    return RangeError("block size");

  UInt32Decoder decoder(next, field_sizes_arr_sz);
  if (!decoder.Next(num_msgs)) {
    return RangeError("num msgs");
  }
  next += field_sizes_arr_sz;
  uint32 size = 0;
  for (PbFieldReader* v : readers_) {
    if (!decoder.Next(&size)) {
      return RangeError("field sizes");
    }
    RETURN_IF_ERROR(v->Init(next, size));
    next += size;
  }
  return Status::OK;
}

}  // namespace coding
}  // namespace util