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
// The file provides simple file functionalities.
// TODO(tkaftal): Tests needed.
#ifndef SUPERSONIC_OPENSOURCE_FILE_FILE_H_
#define SUPERSONIC_OPENSOURCE_FILE_FILE_H_

#include <string>

#include "base/integral_types.h"
#include "strings/stringpiece.h"
#include "base/status.h"

namespace file {

// Use this file mode value, if you want the file system default behaviour when
// creating a file. The exact behaviour depends on the file system.
const mode_t DEFAULT_FILE_MODE = static_cast<mode_t>(0x7FFFFFFFU);

class ReadonlyFile {
protected:
  ReadonlyFile() {}
public:

  struct Options {
    bool use_mmap;
    Options() : use_mmap(true) {}
  };

  virtual ~ReadonlyFile();

  // Reads upto length bytes and updates the result to point to the data.
  // May use buffer for storing data. In case, EOF reached sets result.size() < length but still
  // returns Status::OK.
  virtual base::Status Read(size_t offset, size_t length, strings::Slice* result,
                            uint8* buffer) = 0;

  // releases the system handle for this file.
  virtual base::Status Close() = 0;


  virtual size_t Size() const = 0;

  // Factory function that creates the ReadonlyFile object.
  // The ownership is passed to the caller.
  static base::StatusObject<ReadonlyFile*> Open(StringPiece name, const Options& opts = Options());
};

// Wrapper class for system functions which handle basic file operations.
// The operations are virtual to enable subclassing, if there is a need for
// different filesystem/file-abstraction support.
class File {
 public:
  // Flush and Close access to a file handle and delete this File
  // object. Returns true on success.
  virtual bool Close() = 0;

  // Open a file that has already been created. Should not be called directly.
  virtual bool Open() = 0;

  // Try to write 'length' bytes from 'buffer', returning
  // the number of bytes that were actually written.
  // Return <= 0 on error.
  virtual base::Status Write(const uint8* buffer, uint64 length, uint64* bytes_written) = 0;

  base::Status Write(strings::Slice slice, uint64* bytes_written) {
    return Write(slice.ubuf(), slice.size(), bytes_written);
  }

  virtual base::Status Flush() = 0;

  // If we're currently at eof.
  virtual bool eof() = 0;

  // Returns the file name given during Create(...) call.
  const std::string& create_file_name() const { return create_file_name_; }

 protected:
  explicit File(const StringPiece create_file_name);

  // Do *not* call the destructor directly (with the "delete" keyword)
  // nor use scoped_ptr; instead use Close().
  virtual ~File();

  // Name of the created file.
  const std::string create_file_name_;
};


// Factory method to create a new file object. Calls Open on the
// resulting object to open the file.  Using the appropriate flags
// (+) will result in the file being created if it does not already
// exist
// TODO: to wrap it with FileSystem class.
File* Open(StringPiece file_name, StringPiece mode);

// Deletes the file returning true iff successful.
bool Delete(StringPiece name);

bool Exists(StringPiece name);

class FileCloser {
 public:
  FileCloser() : fp_(nullptr) {}
  // Takes ownership of 'fp' and deletes it upon going out of
  // scope.
  explicit FileCloser(File* fp) : fp_(fp) { }
  File* get() const { return fp_; }
  File& operator*() const { return *fp_; }
  File* operator->() const { return fp_; }
  File* release() {
    File* fp = fp_;
    fp_ = nullptr;
    return fp;
  }
  void reset(File* new_fp) {
    if (fp_) {
      fp_->Close();
    }
    fp_ = new_fp;
  }
  bool Close() {
    return fp_ ? release()->Close() : true;
  }
  // Delete (unlink, remove) the underlying file.
  ~FileCloser() { reset(nullptr); }

 private:
  File* fp_;
  FileCloser(const FileCloser&) = delete;
};

} // namespace file

#endif  // SUPERSONIC_OPENSOURCE_FILE_FILE_H_
