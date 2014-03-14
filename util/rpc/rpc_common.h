// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _RPC_COMMON_H
#define _RPC_COMMON_H

#include <string>
#include "base/integral_types.h"

struct evbuffer;
struct bufferevent;
struct sockaddr;

namespace google {
namespace protobuf {
class Closure;
class MessageLite;

}  // namespace protobuf
}  // namespace google

namespace util {
namespace rpc {

namespace gpb = ::google::protobuf;

void WriteRpcPacket(const gpb::MessageLite& control, const gpb::MessageLite* payload,
                    evbuffer* output);

std::string PrintAddrInfo(const struct sockaddr* sock_addr, size_t addr_len);

class ClosureRunner {
 gpb::Closure* done_;

 public:
  explicit ClosureRunner(gpb::Closure* cl) : done_(cl) {}
  ~ClosureRunner();
};

extern const char kMagicString[];
constexpr uint8 kMagicStringSize = 7;

void bufferevent_deleter(bufferevent* bev);

}  // namespace rpc
}  // namespace util

#endif  // _RPC_COMMON_H