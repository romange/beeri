// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef FILESOURCE_H
#define FILESOURCE_H

#include "base/integral_types.h"
#include "util/sinksource.h"
#include <memory>

namespace file {
class File;

class Source : public util::BufferredSource {
 public:
  // file must be open for reading.
  Source(File* file, Ownership ownership, uint32 buffer_size = BufferredSource::kDefaultBufferSize);
  ~Source();

  // Moves the current file position relative to the current one.
  // Does not change the contents of the buffer.
  util::Status SkipPos(uint64 offset);

  // Returns the source wrapping the file. If the file is compressed, than the stream
  // automatically inflates the compressed data. The returned source owns the file object.
  static util::Source* Uncompressed(File* file);
 private:
  bool RefillInternal();
  File* file_;
  Ownership ownership_;
};

class Sink : public util::Sink {
public:
  // file must be open for writing.
  Sink(File* file, Ownership ownership) : file_(file), ownership_(ownership) {}
  ~Sink();
  util::Status Append(strings::Slice slice);
  util::Status Flush();

private:
  File* file_;
  Ownership ownership_;
};

// Assumes that source provides stream of text characters.
// Will break the stream into lines ending with EOL (either \r\n\ or \n).
class LineReader {
public:
  LineReader(util::Source* source, Ownership ownership) : source_(source),
    ownership_(ownership) {
  }

  explicit LineReader(const std::string& filename);

  ~LineReader();

  uint64 line_num() const { return line_num_;}

  // Overrides the result with the new null-terminated line.
  // Empty lines are also returned.
  // Returns true if new line was found or false if end of stream was reached.
  bool Next(std::string* result);
private:
  util::Source* source_;
  Ownership ownership_;
  uint64 line_num_ = 0;
};

}  // namespace file

#endif  // FILESOURCE_H