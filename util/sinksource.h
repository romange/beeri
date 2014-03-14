// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#ifndef UTIL_SINKSOURCE_H
#define UTIL_SINKSOURCE_H

#include <memory>
#include <string>
#include "base/integral_types.h"
#include "base/macros.h"
#include "strings/slice.h"
#include "util/status.h"

// We prefer Sink and Source (like in snappy and icu) over ZeroCopy streams like in protobuf.
// The reason for this is not convenient corner cases where you have few bytes left in the buffers
// returned by ZeroCopy streams and you need to write a special code in order to serialize your
// primitives whcih require more space.
// Sinks solve this problem by allowing scratch buffers.

namespace util {

class Sink {
 public:
  struct WritableBuffer {
   public:
     WritableBuffer() {}
     WritableBuffer(uint8* f, size_t s): ptr(f), capacity(s) {}
     operator strings::Slice() const { return strings::Slice(ptr, capacity); }
     strings::Slice Prefix(size_t size) const { return strings::Slice(ptr, size); }

     uint8* ptr = nullptr;
     size_t capacity = 0;
  };

  Sink() {}
  virtual ~Sink() {}

  // Appends slice to sink.
  virtual Status Append(strings::Slice slice) = 0;

  // Returns a writable buffer for appending .
  // Guarantees that result.capacity >=min_capacity.
  // May return a pointer to the caller-owned scratch buffer which must have capacity >=min_capacity.
  // The returned buffer is only valid until the next operation on this Sink.
  // After writing at most result.capacity bytes, call Append() with the pointer returned from this
  // function (result.first) and the number of bytes written. Many Append() implementations will
  // avoid copying bytes if this function returned an internal buffer (by just returning
  // WritableBuffer object or calling its Prefix function.)
  // Partial usage example:
  // WritableBuffer buf = sink->GetAppendBuffer(min_capacity, scracth_buffer);
  // ... Write n bytes into buf, with n <= buf.capacity.
  // sink->Append(buf.Prefix(n));
  // In many implementations, that call to Append will avoid copying bytes.
  // If the Sink allocates or reallocates an internal buffer, it should use the desired_capacity_hint
  // if appropriate.
  // If a non-scratch buffer is returned, the caller may only pass its prefix to Append().
  // That is, it is not correct to pass an interior pointer to Append().
  // The default implementation always returns the scratch buffer.
  virtual WritableBuffer GetAppendBuffer(
    size_t min_capacity,
    WritableBuffer scratch,
    size_t desired_capacity_hint = 0);

  Status Append(const std::vector<uint8>& vec) {
    return Append(strings::Slice(vec.data(), vec.size()));
  }

  Status Append(const uint8* src, size_t len) {
    return Append(strings::Slice(src, len));
  }

  // Flushes internal buffers. The default implemenation does nothing. Sink
  // subclasses may use internal buffers that require calling Flush() at the end
  // of the stream.
  virtual Status Flush();

 private:
  DISALLOW_COPY_AND_ASSIGN(Sink);
};

class StringSink : public Sink {
  std::string contents_;
public:
  Status Append(strings::Slice slice) {
    contents_.append(slice.charptr(), slice.length());
    return Status::OK;
  }
  std::string& contents() { return contents_; }
  const std::string& contents() const { return contents_; }
};

// An abstract interface for an object that produces a sequence of bytes.
//
// Example:
//
//   Source* source = ...
//   while (source->Available() > 0) {
//     Slice data = source->Peek();
//     ... do something with "data" ...
//     source->Skip(data.length());
//   }
//
class Source {
 public:
  Source() {}
  virtual ~Source() {}

  // Peek at the next flat region of the source.  Does not reposition
  // the source.  The returned region is empty iff source reached end of stream.
  //
  // Returns Slice to the beginning of the region.
  // The returned region is valid until the next call to Skip() or
  // until this object is destroyed, whichever occurs first.
  //
  // minimal_size - defines the minimal threshold with which a caller
  // wants to read the buffer. It should be relatively small because large numbers may
  // increase memory copies. It is allowed for Source to return less than minimal_size in case
  // end of stream was reached.
  virtual strings::Slice Peek(uint32 minimal_size = 0) = 0;

  // Skips the next n bytes. Invalidates any StringPiece returned by a previous
  // call to Peek().
  //
  // REQUIRES: Peek().size() >= n
  virtual void Skip(size_t n) = 0;

  virtual util::Status status() const = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(Source);
};

class StringSource : public Source {
public:
  // block_size is used to simulate paging reads, usually in tests.
  // input must exists all the time StringSource is used. It should not be
  // changed either since StringSource wraps its internal buffer during the construction.
  explicit StringSource(const std::string& input, uint32 block_size = kuint32max)
    : input_(input), block_size_(block_size) {}

  size_t Available() const {return input_.size();};

  void Skip(size_t n) { input_.remove_prefix(n); }
  strings::Slice Peek(uint32 = 0) { return strings::Slice(input_, 0, block_size_); }
  virtual Status status() const { return Status::OK; }
private:
  strings::Slice input_;
  uint32 block_size_ = kuint32max;
};


// A class that requires an intermediate memory buffer in order to read from somewhere.
// Usually Sink-Source requires at least one memory buffer in order to exchange contents
// between end-points.
// The convention here is that Source manages it's internal buffer, and allows callers to access it
// directly, while Sink copies the data (using the Append) but optionally can provide direct buffer
// as well.
// BufferredSource follows this convention by taking buffer management on itself.
class BufferredSource : public Source {
public:
  static const int kDefaultBufferSize = 65536;
  explicit BufferredSource(uint32 bufsize = kDefaultBufferSize);

  void Skip(size_t n);
  strings::Slice Peek(uint32 minimal_size = 0);
  util::Status status() const { return status_; }

private:
  void Refill(uint32 minimal_size);
protected:
  // Fills the buffer at peek_pos_ + avail_peek_.
  // Can fill available_to_refill() bytes.
  // returns true if eof at source is reached.
  // It still may be that peek buffer has be filled with the remainder of the data.
  // Must update status_ upon exit.
  // All derived clients must implement this function.
  virtual bool RefillInternal() = 0;

  bool IsPeekable(uint32 minimal_size) const {
    return eof_ || (avail_peek_ >= minimal_size && avail_peek_ != 0);
  };

  size_t available_to_refill() const {
    return (buffer_.get() + buf_size_) - (peek_pos_ + avail_peek_);
  }

  // The buffer looks like:
  // buffer_.get() ______ peek_pos_ _____ peek_pos_+ avail_peek________
  // ____peek_pos_+ avail_peek + avail_to_refill = buffer_.get() + buf_size_.
  uint8* peek_pos_ = nullptr; // Running pointer.
  std::unique_ptr<uint8[]> buffer_;
  uint32 buf_size_;

  // Control the peek and refill buffers.
  uint32 avail_peek_ = 0;
  bool eof_ = false;
  util::Status status_;

  DISALLOW_COPY_AND_ASSIGN(BufferredSource);
};

}  // namespace util

#endif  // UTIL_SINKSOURCE_H
