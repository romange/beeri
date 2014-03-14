// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <bzlib.h>

#include "bzip_source.h"
#include "strings/strcat.h"

namespace util {

struct BzipSource::Rep {
  bz_stream stream;

  Rep() { memset(&stream, 0, sizeof(stream)); }
};

BzipSource::BzipSource(Source* sub_source, Ownership ownership)
    : sub_stream_(sub_source), ownership_(ownership), rep_(new Rep) {
  CHECK_EQ(BZ_OK, BZ2_bzDecompressInit(&rep_->stream, 0, 1));
}

BzipSource::~BzipSource() {
  BZ2_bzDecompressEnd(&rep_->stream);
  if (ownership_ == TAKE_OWNERSHIP) delete sub_stream_;
}

bool BzipSource::RefillInternal() {
  DCHECK_GT(available_to_refill(), 0);
  int32 produced = 0;
  while (produced == 0) {
    strings::Slice input_buf = sub_stream_->Peek(buf_size_ / 16);
    if (input_buf.empty()) return true;

    rep_->stream.next_in = const_cast<char*>(input_buf.charptr());
    rep_->stream.avail_in = input_buf.size();
    rep_->stream.next_out = reinterpret_cast<char*>(peek_pos_) + avail_peek_;
    rep_->stream.avail_out = available_to_refill();

    char* prev = rep_->stream.next_out;
    VLOG(2) << "before BZ2_bzDecompress - avail_out:" << rep_->stream.avail_out << ", avail_in: "
            << rep_->stream.avail_in;
    int res = BZ2_bzDecompress(&rep_->stream);
    DCHECK_LE(rep_->stream.avail_in, input_buf.size());
    int32 consumed = input_buf.size() - rep_->stream.avail_in;
    produced = rep_->stream.next_out - prev;
    VLOG(2) << "After BZ2_bzDecompress, res: " << res << ", avail_out:" << rep_->stream.avail_out
            << ", consumed: " << consumed << ", produced:" << produced;
    sub_stream_->Skip(consumed);

    avail_peek_ += produced;
    if (res != BZ_OK) {
      if (res != BZ_STREAM_END) {
        status_.AddErrorMsg(base::StatusCode::IO_ERROR, StrCat("BZip error ", res));
      } else {
        VLOG(2) << "Reached bzip-eof";
      }
      return true;
    }
  }
  return false;
}

bool BzipSource::IsBzipSource(Source* source) {
  strings::Slice header(source->Peek(3));
  return header.size() >= 3 && (header[0] == 'B') && (header[1] == 'Z') && (header[2] == 'h');
}
}  // namespace util
