// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/server_connection.h"

#include <event2/bufferevent.h>
#include <event2/event.h>

#include "util/rpc/rpc_common.h"
#include "util/rpc/rpc_message_reader.h"

using namespace std::placeholders;
using std::string;

namespace util {
namespace rpc {

ServerConnection::ServerConnection(
    bufferevent* buff_ev,
    std::function<void(ServerConnection*, strings::Slice, strings::Slice)> cb)
    : bev_(buff_ev, bufferevent_deleter) {
  auto err_cb = std::bind(&ServerConnection::ReadErrorCallback, this);
  reader_.reset(new MessageReader(std::bind(cb, this, _1, _2), err_cb));

  bufferevent_setcb(buff_ev, ServerConnection::readcb, NULL, connection_event_cb, this);
  bufferevent_setwatermark(buff_ev, EV_READ, MessageReader::min_packet_size(), 0);
  bufferevent_enable(buff_ev, EV_READ);
}

ServerConnection::~ServerConnection() {
}

void ServerConnection::readcb(struct bufferevent* bev, void *ptr) {
  ServerConnection* me = (ServerConnection*)ptr;
  std::shared_ptr<bufferevent> tmp(me->bev_);  // shared_ptrs are thread safe (and lock free).
  if (!tmp)
    return;
  evbuffer* input = bufferevent_get_input(tmp.get());
  if (me->type_ == UNDEFINED) {
    if (me->reader_->ShouldProcessRpc(input)) {
      me->type_ = RPC;
    }
  }
  if (me->type_ == RPC) {
    me->reader_->ParseData(input);
  }
}

void ServerConnection::connection_event_cb(struct bufferevent *bev, short events, void *ctx) {
  ServerConnection* conn = (ServerConnection*)ctx;
  bool close_conn = false;
  if (events & BEV_EVENT_EOF) {
    VLOG(1) << "Closed connection";
    close_conn = true;
  } else if (events & BEV_EVENT_ERROR) {
    LOG(WARNING) << "Bufevent error: " << evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
    close_conn = true;
  }
  if (close_conn) {
    conn->ScheduleClose();
  }
}

void ServerConnection::ScheduleClose() {
  std::shared_ptr<bufferevent> tmp;
  tmp.swap(bev_);
}

void ServerConnection::ReadErrorCallback() {
  int sfd = bufferevent_getfd(bev_.get());
  if (sfd != -1) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof addr;
    string peer;
    if (getpeername(sfd, &addr, &addrlen) == 0) {
      VLOG(1) << "addrlen: " << addrlen << ", sfd " << sfd << ", family " << addr.sa_family ;
      peer = PrintAddrInfo(&addr, addrlen);
    }
    LOG(ERROR) << "Error reading data from connection " << peer << ", closing the connection.";
  }
  ScheduleClose();
}

void ServerConnection::ReplierCb(Call* call) {
  VLOG(1) << "ReplierCb start " << this;
  std::shared_ptr<bufferevent> tmp(bev_);
  if (tmp) {
    evbuffer* output = bufferevent_get_output(tmp.get());
    RpcControlResponse control_response;
    control_response.set_event_id(call->event_id);
    if (call->context.Failed()) {
      VLOG(1) << "Replying with error status " << call->context.status().ShortDebugString();
      control_response.mutable_status()->Swap(call->context.mutable_status());
      WriteRpcPacket(control_response, nullptr, output);
    } else {
      VLOG(2) << "Replying with response: " << call->msg_response->ShortDebugString();
      WriteRpcPacket(control_response, call->msg_response.get(), output);
    }
  }
  auto r = DecRef();
  delete call;
  VLOG(1) << "ReplierCb end " << tmp.get() << " " << r;
}

}  // namespace rpc
}  // namespace util
