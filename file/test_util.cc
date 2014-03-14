// #include <unistd.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "file/test_util.h"
#include "file/file_util.h"

namespace {

using std::string;

string GetTemporaryDirectoryName() {
  // tmpnam() is generally not considered safe but we're only using it for
  // testing.  We cannot use tmpfile() or mkstemp() since we're creating a
  // directory.
  char b[L_tmpnam + 1];     // HPUX multithread return 0 if s is 0
  string result = tmpnam(b);
  return result;
}

// Creates a temporary directory on demand and deletes it when the process
// quits.
class TempDirDeleter {
 public:
  TempDirDeleter() {}
  ~TempDirDeleter() {
    if (!name_.empty()) {
      file_util::DeleteRecursively(name_);
    }
  }

  string GetTempDir() {
    if (name_.empty()) {
      name_ = GetTemporaryDirectoryName();
      CHECK(mkdir(name_.c_str(), 0777) == 0) << strerror(errno);

      // Stick a file in the directory that tells people what this is, in case
      // we abort and don't get a chance to delete it.
      file_util::WriteStringToFileOrDie("", name_ + "/TEMP_DIR_FILE");
    }
    return name_;
  }

 private:
  string name_;
};

TempDirDeleter temp_dir_deleter_;

}  // namespace

string TestTempDir() {
  return temp_dir_deleter_.GetTempDir();
}

namespace file {

base::Status ReadonlyStringFile::Read(size_t offset, size_t length, strings::Slice* result,
                                      uint8* buffer) {
  if (contents_.size() < offset + length)
    return base::Status(base::StatusCode::INTERNAL_ERROR);
  result->set(reinterpret_cast<const uint8*>(contents_.data() + offset), length);
  return base::Status::OK;
}

}  // namespace file