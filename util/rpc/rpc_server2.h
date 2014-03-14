// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _RPC_SERVER2_H
#define _RPC_SERVER2_H

#include <memory>
#include <string>

namespace google {
namespace protobuf {
class Service;
}  // namespace protobuf
}  // namespace google

namespace http {
class Server;
}  // namespace http

namespace util {
class Executor;

namespace rpc {

class RpcServer {
  struct Rep;
  struct OpenConnection;
public:
  // Takes ownership over http server. if null is passed then the default http server is created.
  explicit RpcServer(const std::string& name, http::Server* server = nullptr);

  ~RpcServer();
  void Open(int port, Executor* executor);

  void ExportService(::google::protobuf::Service* service);

  http::Server* http_server();
private:
  std::unique_ptr<Rep> rep_;
};

}  // namespace rpc
}  // namespace util


#endif  // _RPC_SERVER2_H