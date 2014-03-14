#ifndef _BASE_LOGGING_
#define _BASE_LOGGING_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <string>
#include <glog/logging.h>

#pragma GCC diagnostic pop

namespace base {
std::string ProgramAbsoluteFileName();

std::string ProgramBaseName();

// i.e. myprog.runtime_dir/
std::string ProgramRunfilesPath();

// i.e. myprog.runtime_dir/some/relative/path
std::string ProgramRunfile(const std::string& relative_path);  // relative to runtime dir.

std::string MyUserName();

extern const char kVersionString[];
extern const char kBuildTimeString[];

class ConsoleLogSink : public google::LogSink {
public:
  virtual void send(google::LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) override;

  static ConsoleLogSink* instance();
};

}  // namespace base

#define CONSOLE_INFO LOG_TO_SINK(base::ConsoleLogSink::instance(), INFO)

// taken from chromium but it does not have implementation.
// #define COMPACT_GOOGLE_LOG_QFATAL LogMessageQuietlyFatal(__FILE__, __LINE__)
/* #define QCHECK(condition)  \
//       LOG_IF(QFATAL, PREDICT_FALSE(!(condition))) \
//              << "Check failed: " #condition " "

// #define QCHECK_OP(name, op, val1, val2) \
//   CHECK_OP_LOG(name, op, val1, val2, LogMessageQuietlyFatal)
*/
// class LogMessageQuietlyFatal : public google::LogMessage {
//  public:
//   LogMessageQuietlyFatal(const char* file, int line);
//   LogMessageQuietlyFatal(const char* file, int line,
//                          const google::CheckOpString& result);
//   ~LogMessageQuietlyFatal();
// };

#endif  // _BASE_LOGGING_