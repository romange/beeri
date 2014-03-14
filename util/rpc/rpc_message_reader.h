// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _RPC_MESSAGE_READER_H
#define _RPC_MESSAGE_READER_H

#include <string>
#include "strings/slice.h"

struct evbuffer;

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace util {
namespace rpc {

class MessageReader {
  enum State {
    IDLE,
    READ_CNTRL,
    READ_PAYLOAD,
    FATAL_ERROR,
  };

  State state_;

  void ReadData(struct evbuffer* input);

  bool ParseStartRpcHeader(struct evbuffer* input, uint32* ctrl_size, uint32* payload_size);

public:
  typedef std::function<void(strings::Slice control, strings::Slice payload)> MessageCallback;
  typedef std::function<void()> ErrorCallback;

  MessageReader(MessageCallback rpc_cb, ErrorCallback err_cb);

  bool IsValid() const { return state_ != FATAL_ERROR; }

  void ParseData(struct evbuffer* input);

  // Returns true if the data that is contained in input should be parsed by (rpc) MessageReader.
  bool ShouldProcessRpc(struct evbuffer* input);

  static size_t min_packet_size();

private:
  std::string msg_control_;  // buffer for storing the packet.
  std::string msg_payload_;

  char* next_ptr_ = nullptr;
  char* end_ptr_ = nullptr;
  MessageCallback rpc_cb_;
  ErrorCallback err_cb_;
};

}  // namespace rpc
}  // namespace util


#endif  // _RPC_MESSAGE_READER_H