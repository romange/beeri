// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: tomasz.kaftal@gmail.com (Tomasz Kaftal)
//
// File wrapper implementation.
#include "file/file.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "file/s3_file.h"

using std::string;
using base::Status;
using strings::Slice;

namespace file {

namespace {

// Returns true if a uint64 actually looks like a negative int64. This checks
// if the most significant bit is one.
//
// This function exists because the file interface declares some length/size
// fields to be uint64, and we want to catch the error case where someone
// accidently passes an negative number to one of the interface routines.
inline bool IsUInt64ANegativeInt64(uint64 num) {
  return (static_cast<int64>(num) < 0);
}

Status LocalFileError() {
  char buf[1024];
#ifdef STRERROR_R_CHAR_P
  const char* s = buf;
#else
  const char* s = strerror_r(errno, buf, arraysize(buf));
#endif
  return Status(base::StatusCode::IO_ERROR, s);
}

// ----------------- LocalFileImpl --------------------------------------------
// Simple file implementation used for local-machine files (mainly temporary)
// only.
class LocalFileImpl : public File {
 public:
  LocalFileImpl(StringPiece file_name, StringPiece mode,
           const mode_t& permissions);
  LocalFileImpl(const LocalFileImpl&) = delete;

  virtual ~LocalFileImpl();

  // Return true if file exists.  Returns false if file does not exist or if an
  // error is encountered.
  // virtual bool Exists() const;

  // File handling methods.
  virtual bool Open();
  // virtual bool Delete();
  virtual bool Close();
  virtual Status Read(size_t length, uint8* OUTPUT, size_t* read_length);
  // virtual char* ReadLine(char* buffer, uint64 max_length);
  Status Write(const uint8* buffer, uint64 length, uint64* bytes_written);
  Status Seek(int64 position, int whence);
  Status Flush();
  bool eof();

 protected:
  FILE* internal_file_;

 private:
  string file_name_;
  string file_mode_;
  mode_t permissions_;

  bool IsOpenedWritable() const;
};

LocalFileImpl::LocalFileImpl(StringPiece file_name,
                             StringPiece mode,
                             const mode_t& permissions)
  : File(file_name),
    internal_file_(NULL),
    file_name_(file_name.ToString()),
    file_mode_(mode.ToString()),
    permissions_(permissions) { }

LocalFileImpl::~LocalFileImpl() { }

bool LocalFileImpl::Open() {
  if (internal_file_ != NULL) {
    LOG(ERROR) << "File already open: " << internal_file_;
    return false;
  }

  {
    // Using sys/stat.h to check if file can be opened.
    struct stat tmp;
    if (stat(create_file_name_.c_str(), &tmp) != 0) {
      if (errno != ENOENT) {
        // In case of an error (ENOENT only means a directory on
        // create_file_name_'s has not been found - it will be created).
        LOG(WARNING) << "Can't open " << create_file_name_
                     << " because stat() failed "
                     << "(errno = " << strerror(errno) << ").";
        return false;
      }
    } else if (S_ISDIR(tmp.st_mode)) {
      LOG(ERROR) << "Can't open " << create_file_name_
                 << " because it's a directory.";
      return false;
    }
  }

  mode_t permissions = permissions_;
  if (permissions == DEFAULT_FILE_MODE)
    permissions = 0666;

  // Get mode flags
  bool must_use_fopen = false;
  int mode_flags = 0;
  if (file_mode_ == "r") {
    mode_flags = O_RDONLY;
  } else if (file_mode_ == "r+") {
    mode_flags = O_RDWR;
  } else if (file_mode_ == "w") {
    mode_flags = O_CREAT | O_WRONLY | O_TRUNC;
  } else if (file_mode_ == "w+") {
    mode_flags = O_CREAT | O_RDWR | O_TRUNC;
  } else if (file_mode_ == "a") {
    mode_flags = O_CREAT | O_WRONLY | O_APPEND;
  } else if (file_mode_ == "a+") {
    mode_flags = O_CREAT | O_RDWR | O_APPEND;
  } else {
    must_use_fopen = true;
  }
  if (must_use_fopen) {
    // We don't understand the file mode; see if we can let fopen handle it.
    if (permissions_ == DEFAULT_FILE_MODE) {
      internal_file_ = fopen(file_name_.c_str(), file_mode_.c_str());
    }
  } else {
    int fd = open(file_name_.c_str(), mode_flags, permissions);
    if (fd >= 0) {
      internal_file_ = fdopen(fd, file_mode_.c_str());
      if (internal_file_ == NULL) {
        LOG(ERROR) << "fdopen failed: " << strerror(errno);
        close(fd);
      }
    }
  }
  return (internal_file_ != NULL);
}

bool LocalFileImpl::Close() {
  bool result = false;
  if (internal_file_ != NULL) {
    bool error = false;
    int rc;
    error |= (IsOpenedWritable() && ferror(internal_file_));
    rc = fclose(internal_file_);
    error |= (rc != 0);
    result = !error;
  }
  delete this;
  return result;
}

Status LocalFileImpl::Read(size_t length, uint8* buffer, size_t* read_length) {
  CHECK_NOTNULL(buffer);
  CHECK_NOTNULL(internal_file_);
  CHECK_NOTNULL(read_length);
  uint64 bytes_to_read = 0;
  uint64 bytes_read = 0;
  *read_length = 0;
  do {
    bytes_to_read = std::min(length - *read_length, size_t(kuint64max));
    bytes_read = fread(buffer + *read_length, 1, bytes_to_read, internal_file_);
    if (bytes_read < bytes_to_read && ferror(internal_file_)) {
      LOG(ERROR) << "Error on read, " << bytes_read << " out of "
                 << bytes_to_read << " bytes read; file: "
                 << create_file_name_;
      return LocalFileError();
    }
    *read_length += bytes_read;
  } while (*read_length != length && bytes_read == bytes_to_read);
  /*if (feof(internal_file_))
    return Status(base::StatusCode::END_OF_STREAM);*/
  return Status::OK;
}

/*char* LocalFileImpl::ReadLine(char* buffer, uint64 max_length) {
  if (internal_file_ == NULL) return NULL;
  return (fgets(buffer, static_cast<int>(max_length), internal_file_));
}*/

Status LocalFileImpl::Write(const uint8* buffer, uint64 length, uint64* bytes_written) {
  CHECK_NOTNULL(buffer);
  CHECK_NOTNULL(internal_file_);
  CHECK(!IsUInt64ANegativeInt64(length));

  *bytes_written = fwrite(buffer, 1, length, internal_file_);
  // Checking ferror() here flags errors sooner, though we could skip it
  // since caller should not assume that a "successful" write makes it to
  // disk before Flush() or Close(), which already check ferror().
  return ferror(internal_file_) ? LocalFileError() : Status::OK;
}

// The following require a bunch of assertions to make sure
// the 32 to 64 bit conversions are ok.
Status LocalFileImpl::Seek(int64 position, int whence) {
  CHECK_NOTNULL(internal_file_);
  /*if (whence SEEK_SET position < 0) {
    LOG(ERROR) << "Invalid seek position parameter: " << position
               << " on file " << create_file_name_;
    return false;
  }*/
  if (FSEEKO(internal_file_, static_cast<off_t>(position), whence) != 0) {
    return LocalFileError();
  }
  return Status::OK;
}

Status LocalFileImpl::Flush() {
  if (fflush(internal_file_)) return LocalFileError();
  return Status::OK;
}

bool LocalFileImpl::eof() {
  if (internal_file_ == NULL) return true;
  return static_cast<bool>(feof(internal_file_));
}

// Is the file opened writable?
inline bool LocalFileImpl::IsOpenedWritable() const {
  return ((file_mode_[0] == 'w') ||
          (file_mode_[0] == 'a') ||
          ((file_mode_[0] != '\0') && (file_mode_[1] == '+')));
}

}  // namespace

File::File(StringPiece name)
    : create_file_name_(name.ToString()) { }

File::~File() { }

File* Open(StringPiece file_name, StringPiece mode) {
  File* ptr = nullptr;
  ptr = new LocalFileImpl(file_name, mode, DEFAULT_FILE_MODE);
  if (ptr->Open())
    return ptr;
  ptr->Close();
  return nullptr;
}

bool Exists(StringPiece fname) {
  if (IsInS3Namespace(fname)) {
    return ExistsS3File(fname);
  }
  return access(fname.data(), F_OK) == 0;
}

bool Delete(StringPiece name) {
  int err;
  if ((err = unlink(name.data())) == 0) {
    return true;
  } else {
    return false;
  }
}

ReadonlyFile::~ReadonlyFile() {
}

class PosixMmapReadonlyFile : public ReadonlyFile {
  void* base_;
  size_t sz_;
public:
  PosixMmapReadonlyFile(void* base, size_t sz) : base_(base), sz_(sz) {}

  ~PosixMmapReadonlyFile() {
    if (base_) {
      LOG(WARNING) << " ReadonlyFile::Close was not called";
      Close();
    }
  }

  Status Read(size_t offset, size_t length, Slice* result, uint8* buffer) override;

  Status Close() override;

  size_t Size() const override {
    return sz_;
  }
};

Status PosixMmapReadonlyFile::Read(
    size_t offset, size_t length, Slice* result, uint8* buffer) {
  Status s;
  if (offset + length > sz_) {
    *result = Slice();
    return Status(base::StatusCode::INTERNAL_ERROR, "Invalid read range");
  }
  *result = Slice(reinterpret_cast<char*>(base_) + offset, length);

  return s;
}

Status PosixMmapReadonlyFile::Close() {
  if (munmap(base_, sz_) < 0) {
    return LocalFileError();
  }
  base_ = nullptr;
  return Status::OK;
}

base::StatusObject<ReadonlyFile*> ReadonlyFile::Open(StringPiece name) {
  if (IsInS3Namespace(name)) {
    return OpenS3File(name);
  }
  int fd = open(name.data(), O_RDONLY);
  if (fd < 0) {
    return LocalFileError();
  }
  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    close(fd);
    return LocalFileError();
  }
  void* base = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (base == MAP_FAILED) {
    return LocalFileError();
  }
  return new PosixMmapReadonlyFile(base, sb.st_size);
}


} // namespace file