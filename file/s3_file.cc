// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <libs3.h>
#include <mutex>
#include <unordered_map>
#include "file/file.h"
#include "file/s3_file.h"

namespace file {

using base::Status;
namespace {

const char kS3NamespacePrefix[] = "s3://";

typedef std::unordered_map<string, S3BucketContext> BucketMap;
struct S3GlobalData;

std::once_flag s3data_once;
S3GlobalData* global_data = nullptr;

inline BucketMap& bucket_map() {
  static BucketMap internal_map;
  return internal_map;
}

inline string safe_cstr(const char* s) {
  return s ? s : "";
}

struct CallbackData {
  S3Status status = S3StatusOK;
  size_t length = 0;
  uint8* dest_buf = nullptr;
};

class S3File : public ReadonlyFile {
  const S3BucketContext* context_ = nullptr;
  std::string key_;
  size_t file_size_ = 0;
public:
  S3File(size_t file_size, StringPiece key, const S3BucketContext* context)
      : context_(context), key_(key.as_string()), file_size_(file_size) {}

  Status Close() override { return Status::OK;}

  // Reads data and returns it in OUTPUT. Set total count of bytes read  into read_size.
  virtual Status Read(size_t offset, size_t length, strings::Slice* result,
                      uint8* buffer) override;

  size_t Size() const override { return file_size_; }

  static S3Status PropertiesCallback(const S3ResponseProperties* properties, void* callbackData) {
    VLOG(1) << "S3File " << properties->contentType << " " << properties->contentLength << " "
            << properties->metaDataCount;
    CallbackData* data = reinterpret_cast<CallbackData*>(callbackData);
    data->length = properties->contentLength;
    return S3StatusOK;
  }

  static void CompleteCallback(S3Status status, const S3ErrorDetails* errorDetails,
                               void* callbackData) {
    CallbackData* data = reinterpret_cast<CallbackData*>(callbackData);
    if (status != S3StatusOK) {
      string msg;
      if (errorDetails) {
        msg = safe_cstr(errorDetails->message) + ", more: "  +
          safe_cstr(errorDetails->furtherDetails);
      }
      LOG(WARNING) << "Status : " << S3_get_status_name(status) << ", msg: "
                   << msg;
    }
    data->status = status;
  }

  static S3Status DataCallback(int bufferSize, const char *buffer,
                               void* callbackData);
protected:
  ~S3File() {}
};

struct S3GlobalData {
  string access_key;
  string secret_key;
};

void InitS3Data() {
  if (global_data == nullptr) {
    CHECK_EQ(S3StatusOK, S3_initialize(NULL, S3_INIT_ALL, NULL));
    global_data = new S3GlobalData();
    const char* ak = CHECK_NOTNULL(getenv("AWS_ACCESS_KEY"));
    global_data->access_key.assign(ak);
    const char* sk = CHECK_NOTNULL(getenv("AWS_SECRET_KEY"));
    global_data->secret_key.assign(sk);
  }
}

inline S3BucketContext NewBucket() {
  return S3BucketContext{nullptr, nullptr, S3ProtocolHTTP, S3UriStylePath,
                         global_data->access_key.c_str(),
                         global_data->secret_key.c_str()};
}

std::pair<StringPiece, const S3BucketContext*> GetKeyAndBucket(StringPiece name) {
  DCHECK(name.starts_with(kS3NamespacePrefix)) << name;
  std::call_once(s3data_once, InitS3Data);
  StringPiece tmp = name;
  tmp.remove_prefix(sizeof(kS3NamespacePrefix) - 1);
  size_t pos = tmp.find('/');
  CHECK_NE(StringPiece::npos, pos) << "Invalid filename " << name;

  string bucket_str = tmp.substr(0, pos).as_string();
  BucketMap& bmap = bucket_map();
  auto res = bmap.emplace(bucket_str, NewBucket());
  if (res.second) {
    res.first->second.bucketName = res.first->first.c_str();
  }
  tmp.remove_prefix(pos + 1);
  return std::pair<StringPiece, const S3BucketContext*>(tmp, &res.first->second);
}

Status S3File::Read(size_t offset, size_t length, strings::Slice* result,
                    uint8* buffer) {
  if (offset + length > file_size_)
    return Status(base::StatusCode::IO_ERROR, "Invalid read range");

  S3GetObjectHandler handler{{nullptr, S3File::CompleteCallback},
                             S3File::DataCallback};
  CallbackData data;
  data.dest_buf = buffer;
  S3_get_object(context_, key_.c_str(), nullptr, offset, length, nullptr, &handler, &data);
  if (data.status != S3StatusOK) {
    return Status(base::StatusCode::IO_ERROR, S3_get_status_name(data.status));
  }
  result->set(buffer, data.length);
  return Status::OK;
}

S3Status S3File::DataCallback(int bufferSize, const char *buffer, void* callbackData) {
  VLOG(1) << "S3 Read " << bufferSize << " bytes";
  CallbackData* data = reinterpret_cast<CallbackData*>(callbackData);
  memcpy(data->dest_buf, buffer, bufferSize);
  data->dest_buf += bufferSize;
  data->length += bufferSize;
  return S3StatusOK;
}

}  // namespace

base::StatusObject<ReadonlyFile*> OpenS3File(StringPiece name) {
  auto res = GetKeyAndBucket(name);
  const S3BucketContext* context = res.second;
  CHECK(!res.first.empty()) << "Missing file name after the bucket";
  StringPiece key = res.first;

  S3ResponseHandler handler{S3File::PropertiesCallback, S3File::CompleteCallback};
  CallbackData data;
  S3_head_object(context, key.data(), nullptr, &handler, &data);

  if (data.status != S3StatusOK)
    return Status(base::StatusCode::IO_ERROR, S3_get_status_name(data.status));

  return new S3File(data.length, key, context);
}

bool IsInS3Namespace(StringPiece name) {
  return name.starts_with(kS3NamespacePrefix);
}

bool ExistsS3File(StringPiece name) {
  S3ResponseHandler handler{nullptr, S3File::CompleteCallback};
  CallbackData data;
  auto res = GetKeyAndBucket(name);
  S3_head_object(res.second, res.first.data(), nullptr, &handler, &data);
  return (data.status == S3StatusOK);
}

}  // namespace file