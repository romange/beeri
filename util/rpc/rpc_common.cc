// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/rpc_common.h"

#include <memory>

#include <arpa/inet.h>  // inet_ntop
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>

#include <google/protobuf/message_lite.h>
#include "base/endian.h"
#include "base/logging.h"
#include "strings/strcat.h"

namespace util {
namespace rpc {

using std::string;

const char kMagicString[] = "RPC0.1";

void WriteRpcPacket(const ::google::protobuf::MessageLite& control,
                    const ::google::protobuf::MessageLite* payload,
                    evbuffer* output) {
  VLOG(2) << "Start WriteRpcPacket";
  static_assert(sizeof(kMagicString) == kMagicStringSize, "wrong magic stringsize");

  uint32 control_size = control.ByteSize();
  uint32 payload_size = payload ? payload->ByteSize() : 0;
  uint32 total_size = payload_size + control_size;

  DCHECK_GT(total_size, 0);
  std::unique_ptr<uint8[]> data(new uint8[total_size]);

  // TODO: wrap ev_buf as io::ZeroCopyOutputStream or to use evbuffer_reserve_space/ commit for
  uint8* next = control.SerializeWithCachedSizesToArray(data.get());
  if (payload)
    payload->SerializeWithCachedSizesToArray(next);

  uint8 tmp_buf[6];
  LittleEndian::Store16(tmp_buf, control_size);
  LittleEndian::Store32(tmp_buf + 2, payload_size);

  evbuffer_lock(output); // Must protect in order to make multiple writes atomic.
  CHECK_EQ(0, evbuffer_add(output, kMagicString, kMagicStringSize));
  CHECK_EQ(0, evbuffer_add(output, tmp_buf, sizeof tmp_buf));
  CHECK_EQ(0, evbuffer_add(output, data.get(), total_size));
  evbuffer_unlock(output);
  VLOG(2) << "WriteRpcPacket Finish";
}

std::string PrintAddrInfo(const struct sockaddr* sock_addr, size_t addr_len) {
  string result;
  result.resize(INET6_ADDRSTRLEN, 0);
  int port = -1;
  const void* addr;
  // different fields in IPv4 and IPv6:
  if (sock_addr->sa_family == AF_INET) { // IPv4
    const struct sockaddr_in* ipv4 = (const struct sockaddr_in*)sock_addr;
    port = ntohs(ipv4->sin_port);
    addr = &(ipv4->sin_addr);
  } else { // IPv6
    struct sockaddr_in6* ipv6 = (struct sockaddr_in6 *)sock_addr;
    addr = &(ipv6->sin6_addr);
    port = ntohs(ipv6->sin6_port);
  }

  // convert the IP to a string and print it:
  const char* next = inet_ntop(sock_addr->sa_family, addr, &result[0], result.size());
  if (next == nullptr) {
    LOG(FATAL) << strerror(errno);
  }
  result.resize(result.c_str() - next);
  StrAppend(&result, ":", port);
  return result;
}

ClosureRunner::~ClosureRunner() {
  if (done_) done_->Run();
}

static void free_bev(int fd, short events, void *arg) {
  bufferevent* bev = (bufferevent*)arg;
  bufferevent_free(bev);
}

void bufferevent_deleter(bufferevent* bev) {
  bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
  // There is a deadlock bug in libevent when we delete bufferevent not in event thread.
  event_base_once(bufferevent_get_base(bev), -1,  EV_TIMEOUT, free_bev, bev, NULL);
}

}  // namespace rpc
}  // namespace util
