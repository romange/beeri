// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _SERVER_CONNECTION_H
#define _SERVER_CONNECTION_H

#include <memory>
#include "base/refcount.h"

#include "strings/slice.h"
#include "util/rpc/rpc_context.h"

struct bufferevent;

namespace util {
namespace rpc {

class MessageReader;

namespace gpb = ::google::protobuf;

class ServerConnection : public base::RefCount<ServerConnection> {
  std::unique_ptr<MessageReader> reader_;
  std::shared_ptr<bufferevent> bev_;
  enum Type {UNDEFINED, RPC, HTTP } type_ = UNDEFINED;
public:
  struct Call {
    Context context;

    int64 event_id;
    std::unique_ptr<gpb::Message> msg_request;
    std::unique_ptr<gpb::Message> msg_response;

    Call(int64 id, gpb::Message* req, gpb::Message* resp)
        : event_id(id), msg_request(req), msg_response(resp) {}
  };

  explicit ServerConnection(bufferevent* buff_ev,
        std::function<void(ServerConnection*, strings::Slice, strings::Slice)> cb);

  static void readcb(struct bufferevent* bev, void *ptr);
  static void connection_event_cb(struct bufferevent *bev, short events, void *ctx);

  Call* AllocateCall(int64 id, gpb::Message* req, gpb::Message* resp) {
    AddRef();
    return new Call{id, req, resp};
  }

  void ReadErrorCallback();

  // writes the answer back the client and deletes the call.
  // Note that this function receives output pointer that can be invalid by the time
  // it's called.
  // TODO: We should keep all the incoming connections in set and be able to check
  // if the connection is still valid before writing into output.
  void ReplierCb(Call* call);

  void ScheduleClose();

  ~ServerConnection();

  std::shared_ptr<bufferevent> bev() const {
    return bev_;
  }

};

}  // namespace rpc
}  // namespace util

#endif  // _SERVER_CONNECTION_H