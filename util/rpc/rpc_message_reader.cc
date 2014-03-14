// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/rpc_message_reader.h"
#include "base/integral_types.h"
#include "base/endian.h"
#include "base/logging.h"

// #include "util/coding/fixed.h"
// #include "util/rpc/rpc.pb.h"

#include <event2/buffer.h>
#include <event2/bufferevent.h>

namespace util {
namespace rpc {

static const char kMagicPrefix[] = "RPC0.1";
constexpr uint8 kMagicPrefixLen = sizeof kMagicPrefix;
constexpr uint8 kHeaderSize = kMagicPrefixLen + 6;
constexpr uint32 kMaxPacketSize = 1024*1024*10;  // 10MB


// The frame format is:
// 1. kMagicPrefix
// 2. 2 bytes describing header size (ctrl packet + varint)
// 3. 4 bytes describing payload size.
// 4. control packet.
// 5. payload packet.

 MessageReader::MessageReader(std::function<void(strings::Slice control,
                                                 strings::Slice payload)> rpc_cb,
                              ErrorCallback err_cb)
    : state_(IDLE), rpc_cb_(rpc_cb), err_cb_(err_cb) {
}

void MessageReader::ParseData(struct evbuffer* input) {
  size_t len = evbuffer_get_length(input);
  VLOG(1) << "MessageReader::ParseData with " << len << " bytes" << ", input " << input;

  while ((len = evbuffer_get_length(input)) > 0) {
    if (state_ == IDLE ) {
      if (len < kHeaderSize) {
        // This could happen when we already parsed some packets and there are more bytes in
        // the buffer but not enough in order to start deserializing.
        break;
      }
      uint32 control_size = 0, payload_size = 0;
      if (!ParseStartRpcHeader(input, &control_size, &payload_size)) {
        // Could not parse the start of the message, i.e. it was corrupted. There is no syncing
        // mechanism so we must declare this connection as broken.
        state_ = FATAL_ERROR;
        return;
      }
      VLOG(2) << "Contrl size " << control_size << ", payload size: " << payload_size
              << ", len: " << evbuffer_get_length(input) << ", contigious len: "
              << evbuffer_get_contiguous_space(input);

      // TODO: to use evbuffer_get_contiguous_space to save allocation.
      msg_control_.resize(control_size);
      msg_payload_.resize(payload_size);
      next_ptr_ = &msg_control_.front();
      end_ptr_ = next_ptr_ + control_size;
      state_ = READ_CNTRL;
    } else if (!IsValid()) {
      evbuffer_drain(input, len);
      err_cb_();
      return;
    }
    ReadData(input);
  }
}

bool MessageReader::ShouldProcessRpc(struct evbuffer* input) {
  if (state_ != IDLE) return true;
  if (evbuffer_get_length(input) < kMagicPrefixLen)
    return false;  // it should really be not sure.

  unsigned char* buf = evbuffer_pullup(input, kMagicPrefixLen);
  return memcmp(buf, kMagicPrefix, kMagicPrefixLen) == 0;
}

void MessageReader::ReadData(struct evbuffer* input) {
  DCHECK_NE(state_, IDLE);
  uint32 left_bytes = end_ptr_ - next_ptr_;

  int read_bytes = evbuffer_remove(input, next_ptr_, left_bytes);
  CHECK_GE(read_bytes, 0) << "evbuffer_remove failed";
  next_ptr_ += read_bytes;
  VLOG(1) << "ReadData: read " << read_bytes << ", left " << left_bytes;
  if (uint32(read_bytes) < left_bytes)
    return;
  if (state_ == READ_CNTRL && !msg_payload_.empty()) {
    next_ptr_ = &msg_payload_.front();
    end_ptr_ = next_ptr_ + msg_payload_.size();
    state_ = READ_PAYLOAD;
    return;
  }
  rpc_cb_(msg_control_, msg_payload_);
  state_ = IDLE;
  next_ptr_ = nullptr;
}

bool MessageReader::ParseStartRpcHeader(struct evbuffer* input,
                                        uint32* ctrl_size, uint32* payload_size) {
  DCHECK_GE(evbuffer_get_length(input), kHeaderSize);

  // Peek into the header.
  unsigned char* buf = evbuffer_pullup(input, kHeaderSize);
  if (memcmp(buf, kMagicPrefix, kMagicPrefixLen) != 0) {
    LOG(ERROR) << "Invalid magic string";
    return false;
  }
  buf += kMagicPrefixLen;
  *ctrl_size = LittleEndian::Load16(buf);
  if (*ctrl_size == 0) {
    // TODO: to fix this. This error is not fatal for the connection.
    // ctrl_size is not valid but it's possible to keep the channel and
    // just skip the invalid packet.
    LOG(ERROR) << "Invalid 0 control msg size " << " bytes";
    return false;
  }
  buf += 2;
  *payload_size = LittleEndian::Load32(buf);
  evbuffer_drain(input, kHeaderSize);
  return true;
}

size_t MessageReader::min_packet_size() {
  return kHeaderSize + 1;
}

}  // namespace util
}  // namespace util
