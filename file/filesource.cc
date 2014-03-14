// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "file/filesource.h"

#include "base/logging.h"
#include "file/file.h"
#include "util/bzip_source.h"
#include "util/zlib_source.h"

namespace file {

using strings::Slice;
using util::Status;

Source::Source(File* file,  Ownership ownership, uint32 buffer_size)
 : BufferredSource(buffer_size), file_(file), ownership_(ownership) {
}

Source::~Source() {
  if (ownership_ == TAKE_OWNERSHIP)
    CHECK(file_->Close());
}

Status Source::SkipPos(uint64 offset) {
  return file_->Seek(offset, SEEK_CUR);
}

bool Source::RefillInternal() {
  uint32 refill = available_to_refill();
  size_t size_read = 0;
  status_ = file_->Read(refill, peek_pos_ + avail_peek_, &size_read);
  avail_peek_ += size_read;
  if (!status_.ok()) {
    return true;
  }
  return false;
}

util::Source* Source::Uncompressed(File* file) {
  Source* first = new Source(file, TAKE_OWNERSHIP);
  if (util::BzipSource::IsBzipSource(first))
    return new util::BzipSource(first, TAKE_OWNERSHIP);
  if (util::ZlibSource::IsZlibSource(first))
    return new util::ZlibSource(first, TAKE_OWNERSHIP);
  return first;
}

Sink::~Sink() {
 if (ownership_ == TAKE_OWNERSHIP)
  CHECK(file_->Close());
}

util::Status Sink::Append(strings::Slice slice) {
  uint64 bytes_written = 0;
  return file_->Write(slice.data(), slice.size(), &bytes_written);
}

Status Sink::Flush() {
  return file_->Flush();
}

LineReader::LineReader(const std::string& fl) : ownership_(TAKE_OWNERSHIP) {
  File* f = file::Open(fl, "r");
  CHECK(f) << fl;
  source_ = new file::Source(f, TAKE_OWNERSHIP);
}

LineReader::~LineReader() {
  if (ownership_ == TAKE_OWNERSHIP) {
    delete source_;
  }
}

bool LineReader::Next(std::string* result) {
  result->clear();
  bool eof = false;
  while (true) {
    Slice s = source_->Peek();
    if (s.empty()) { eof = true; break; }

    size_t eol = s.find(0xA); // Search for \n
    if (eol != Slice::npos) {
      uint32 skip = eol + 1;
      if (eol > 0 && s[eol - 1] == 0xD)
        --eol;
      result->append(reinterpret_cast<const char*>(s.data()), eol);
      source_->Skip(skip);
      ++line_num_;
      break;
    }
    result->append(reinterpret_cast<const char*>(s.data()), s.size());
    source_->Skip(s.size());
  }
  return !(eof && result->empty());
}

}  // namespace file