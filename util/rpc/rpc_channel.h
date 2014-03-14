// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _RPC_CHANNEL_H
#define _RPC_CHANNEL_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <google/protobuf/service.h>
#include "strings/stringpiece.h"

struct bufferevent;

namespace util {
class Executor;
namespace rpc {

namespace gpb = ::google::protobuf;
class Context;
class MessageReader;

class Channel : public gpb::RpcChannel {
  std::string host_;
  int32 port_;
 public:
  Channel(Executor* executor, StringPiece host_port);
  virtual ~Channel();

  // Call the given method of the remote service.  The signature of this
  // procedure looks the same as Service::CallMethod(), but the requirements
  // are less strict in one important way:  the request and response objects
  // need not be of any specific class as long as their descriptors are
  // method->input_type() and method->output_type().
  virtual void CallMethod(const gpb::MethodDescriptor* method,
                          gpb::RpcController* controller,
                          const gpb::Message* request,
                          gpb::Message* response,
                          gpb::Closure* done);

  // Waits for the channel to connect.
  bool WaitToConnect(uint32 milliseconds = kuint32max);

  // Can be called at most once during the lifetime of the channel.
  // If the deadline is set and the response was not received during the specified timeout,
  // then the done callback will be called and rpc::Context will return Status::DEADLINE_EXCEEDED.
  // The deadline is relevent only when sending requests and has no affect during
  // WaitToConnect call.
  void set_rpc_deadline(uint32 milliseconds);
private:

  class Rep;

  std::unique_ptr<Rep> rep_;

  std::atomic_llong next_id_;  // monotonically increasing id
  std::atomic_ulong deadline_;  // deadline in milliseconds.
};

}  // namespace rpc
}  // namespace util

#endif  // _RPC_CHANNEL_H