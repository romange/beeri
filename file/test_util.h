#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <string>
#include "file/file.h"

// When running unittests, get the directory containing the source code.
std::string TestSourceDir();

// When running unittests, get a directory where temporary files may be
// placed.
std::string TestTempDir();

namespace file {

class NullFile : public File {
 public:
  NullFile() : File("NullFile") {}
  virtual bool Close() override { return true; }
  virtual bool Open() override { return true; }
  virtual base::Status Read(size_t , uint8* , size_t* ) {
    return base::Status::OK;
  }
  base::Status Write(const uint8* ,uint64,  uint64* ) override { return base::Status::OK; }
  base::Status Seek(int64 , int ) override { return base::Status::OK; }
  bool eof() override  {return true;}
  base::Status Flush() override {return base::Status::OK; }
};

class ReadonlyStringFile : public ReadonlyFile {
  string contents_;
public:
  ReadonlyStringFile(const string& str) : contents_(str) {}

  // Reads upto length bytes and updates the result to point to the data.
  // May use buffer for storing data.
  base::Status Read(size_t offset, size_t length, strings::Slice* result,
                            uint8* buffer) override;

  // releases the system handle for this file.
  base::Status Close() override { return base::Status::OK; }

  size_t Size() const override { return contents_.size(); }
};

}  // namespace file

#endif  // TEST_UTIL_H
