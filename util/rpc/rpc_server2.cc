// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/rpc_server2.h"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <sys/socket.h>  // getaddrinfo

#include "base/logging.h"
#include "strings/strcat.h"
#include "util/http/http_server.h"
#include "util/http/varz_stats.h"
#include "util/executor.h"
#include "util/rpc/rpc.pb.h"
#include "util/rpc/rpc_common.h"
#include "util/rpc/rpc_context.h"
#include "util/rpc/server_connection.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <thread>

using namespace std::placeholders;
using std::string;

namespace util {
namespace rpc {

namespace {

void ReplyError(evbuffer* output, int64 event_id, Status::Code code,
                const string& msg = string()) {
  RpcControlResponse response;
  response.mutable_status()->set_code(code);
  if (!msg.empty())
    response.mutable_status()->set_details(msg);
  response.set_event_id(event_id);

  WriteRpcPacket(response, nullptr, output);
}

http::VarzMapCount rpc_requests("rpc_requests");
http::VarzMapAverage rpc_latency("rpc_latency(ms)");

}  // namespace


struct RpcServer::Rep {
  Rep(const std::string& n, http::Server* server) : http_server(server), name(n) {
  }

  ~Rep();

  void Open(const sockaddr_in& addr, Executor* executor);

  void IncomingRpcHandler(ServerConnection* conn, strings::Slice cntrl, strings::Slice msg);

  std::unordered_map<std::string, gpb::Service*> services_;

  bool IsOpen() const { return listener_ != nullptr; }

  std::unique_ptr<http::Server> http_server;
  string name;
private:
  static void accept_error_cb(struct evconnlistener *listener, void *ctx) {
    int err = EVUTIL_SOCKET_ERROR();
    LOG(ERROR) << "Got an error " << err << " " << evutil_socket_error_to_string(err)
               << "on the listener.";
  }

  static void accept_conn_cb(struct evconnlistener *listener,
                             int sokcet_fd, struct sockaddr *address, int socklen,
                             void *ctx);

  struct evconnlistener* listener_ = nullptr;

  Executor* executor_ = nullptr;

  // modified only by the accept_conn_cb() or by destructor.
  std::unordered_set<ServerConnection*> connections_;
};

RpcServer::Rep::~Rep() {
  // TODO: to free all resources, including the open connections and
  // buffevents.
  // I assume that accept_conn_cb can not be called after this line. Since only accept_conn_cb
  // modifies connections_, we feel safe access it.
  if (listener_)
    evconnlistener_free(listener_);
  for (ServerConnection* conn : connections_) {
    conn->ScheduleClose();
    conn->DecRef();
  }

  // It is possible that eventhread is still processing incoming callbacks including readcb
  // we can not know if it's the case, and it's very hard to wait for them to finish.
  // for now we just sleep and hope for better future.
  // TODO: to think about sync logic in ServerConnection::readcb and here which checks
  // if there are penging incoming events on this connection.
  std::this_thread::sleep_for(std::chrono::milliseconds(140));
}

// The following function runs in base_event_loop thread.
// TODO: should not call any user code directly.
void RpcServer::Rep::IncomingRpcHandler(ServerConnection* conn, strings::Slice cntrl,
                                        strings::Slice msg) {
  RpcControlRequest request;
  CHECK(request.ParseFromArray(cntrl.data(), cntrl.size()));
  VLOG(2) << request.ShortDebugString() << ",   payload size " << msg.size();
  size_t pos = request.method_full_name().rfind('.');
  CHECK_NE(pos, string::npos);
  string service_name = request.method_full_name().substr(0, pos);
  std::shared_ptr<bufferevent> tmp(conn->bev());
  if (!tmp)
    return;
  evbuffer* output = bufferevent_get_output(tmp.get());

  // services_ is immutable when rpc server is running.
  auto it = services_.find(service_name);
  if (it == services_.end()) {
    ReplyError(output, request.event_id(), Status::INVALID_SERVICE, service_name);
    return;
  }
  gpb::Service* service = it->second;
  const gpb::ServiceDescriptor* sdescr = CHECK_NOTNULL(service->GetDescriptor());

  string method_name = request.method_full_name().substr(pos + 1);
  const gpb::MethodDescriptor* mdescr = sdescr->FindMethodByName(method_name);
  if (mdescr == nullptr) {
    ReplyError(output, request.event_id(), Status::INVALID_METHOD, request.method_full_name());
    return;
  }
  rpc_requests.Inc(StrCat(mdescr->name(), "-received"));

  ServerConnection::Call* call =
      conn->AllocateCall(request.event_id(), service->GetRequestPrototype(mdescr).New(),
                         service->GetResponsePrototype(mdescr).New());
  CHECK(call->msg_request->ParseFromArray(msg.data(), msg.size()));

  executor_->Add(std::bind(&gpb::Service::CallMethod, service,
                           mdescr, &call->context, call->msg_request.get(),
                           call->msg_response.get(),
                           gpb::NewCallback(conn, &ServerConnection::ReplierCb, call)));
}

void RpcServer::Rep::Open(const sockaddr_in& addr, Executor* executor) {
  CHECK(executor_ == nullptr) << "Open() can be called only once.";

  executor_ = executor;
  const sockaddr* saddr = reinterpret_cast<const sockaddr*>(&addr);
  int rpc_port = ntohs(addr.sin_port);
  listener_ = evconnlistener_new_bind(
    executor->ebase(), accept_conn_cb, this,
    LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE|LEV_OPT_THREADSAFE, -1,
    saddr, sizeof addr);
  if (listener_ == nullptr) {
    LOG(FATAL) << "Could not open listen connection on port " << rpc_port;
  }
  evconnlistener_set_error_cb(listener_, accept_error_cb);
  if (!http_server) {
    int port = ntohs(addr.sin_port) + 1;
    http_server.reset(new http::Server(port));
  }

  LOG(INFO) << "Start serving " << name << " rpc server on port " << rpc_port;
  ::util::Status status = http_server->Start();
  if (!status.ok()) {
    LOG(ERROR) << "Could not start http server on port " << http_server->port();
  }
}

// Runs in event thread.
void RpcServer::Rep::accept_conn_cb(struct evconnlistener *listener,
                                    int socket_fd, struct sockaddr *address, int socklen,
                                    void *ctx) {
  struct event_base* base = evconnlistener_get_base(listener);
  struct bufferevent* bev = bufferevent_socket_new(base, socket_fd,
     BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);

  RpcServer::Rep* me = reinterpret_cast<RpcServer::Rep*>(ctx);
  ServerConnection* conn =
      new ServerConnection(bev, std::bind(&Rep::IncomingRpcHandler, me, _1, _2, _3));

  VLOG(1) << "Accepting connection from " << PrintAddrInfo(address, socklen)
          << ", socket_fd = " << bufferevent_getfd(bev);

  // GC unused connections.
  // we can either add a pointer to Rep in ServerConnection or do it in O(n) every time.
  auto it = me->connections_.begin();
  while (it != me->connections_.end()) {
    if (!(*it)->bev()) {
      (*it)->DecRef();
      it = me->connections_.erase(it);
    } else {
      ++it;
    }
  }
  me->connections_.insert(conn);
  VLOG(2) << "accept_conn_cb Exit\n\n";
}

RpcServer::RpcServer(const std::string& name, http::Server* server)
    : rep_(new Rep(name, server)) {
}

RpcServer::~RpcServer() {}

void RpcServer::ExportService(gpb::Service* service) {
  CHECK_NOTNULL(service);
  CHECK(!rep_->IsOpen()) << "ExportService should be called before RpcServer::Open";

  const gpb::ServiceDescriptor* s_descr = CHECK_NOTNULL(service->GetDescriptor());
  auto result = rep_->services_.emplace(s_descr->full_name(), service);
  CHECK(result.second) << "Can not register twice a service with the same name "
                       << s_descr->full_name();
}

void RpcServer::Open(int port, Executor* executor) {
  struct sockaddr_in my_addr;
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = INADDR_ANY;
  my_addr.sin_port = htons(port);

  rep_->Open(my_addr, executor);
}

}  // namespace rpc
}  // namespace util
