#include "base/logging.h"

#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

namespace base {

using std::string;

static const char kProcSelf[] = "/proc/self/exe";
static const char kDeletedSuffix[] =  " (deleted)";
constexpr size_t kDeletedSuffixLen = strlen(kDeletedSuffix);

string ProgramAbsoluteFileName() {
  string res(2048, '\0');
  size_t sz = readlink(kProcSelf, &res.front(), res.size());
  CHECK_GT(sz, 0);
  if (sz > kDeletedSuffixLen) {
    // When binary was deleted, linux link contains kDeletedSuffix at the end.
    // Lets strip it.
    if (res.compare(sz - kDeletedSuffixLen, kDeletedSuffixLen, kDeletedSuffix) == 0) {
      sz -= kDeletedSuffixLen;
      res[sz] = '\0';
    }
  }
  res.resize(sz);
  return res;
}

string ProgramBaseName() {
  string res = ProgramAbsoluteFileName();
  size_t pos = res.rfind("/");
  if (pos == string::npos)
    return res;
  return res.substr(pos + 1);
}

string ProgramRunfilesPath() {
  return ProgramAbsoluteFileName().append(".runfiles/");
}

string ProgramRunfile(const string& relative_path) {
  return ProgramRunfilesPath().append(relative_path);
}

string MyUserName() {
  const char* str = std::getenv("USER");
  return str ? str : string("unknown-user");
}

void ConsoleLogSink::send(google::LogSeverity severity, const char* full_filename,
                          const char* base_filename, int line,
                          const struct ::tm* tm_time,
                          const char* message, size_t message_len) {
  std::cout.write(message, message_len);
  std::cout << std::endl;
}

ConsoleLogSink* ConsoleLogSink::instance() {
  static ConsoleLogSink sink;
  return &sink;
}

}  // namespace base