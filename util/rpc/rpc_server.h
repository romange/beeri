// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <functional>
#include <memory>
#include <string>
#include <boost/shared_ptr.hpp>

namespace apache {
namespace thrift {
class TProcessor;
class TDispatchProcessor;

namespace protocol {
class TProtocol;
}  // namespace protocol
}  // namespace thrift
}  // namespace apache

namespace http {
class Server;
}  // namespace http

namespace util {

class RpcServer {
public:
  // Takes ownership over processor.
  RpcServer(const std::string& name, int port,
            apache::thrift::TProcessor* processor, http::Server* server = nullptr);
  ~RpcServer();

  // Configures an RPC server (and its debug http server) but does not run the
  // main loop.
  void Start();

  // Runs the main loop and blocks indefinitely.
  void Run();

  void Stop();

  http::Server* http_server();

  // Registers cb to be called when rpc server is stopped.
  // enter_lameduck_cb is called right before we stop the rpc server.
  // rpc_stopped_cb is called right after.
  void RegisterLameduckHandlers(std::function<void()> enter_lameduck_cb,
                            std::function<void()> rpc_stopped_cb) {
    enter_lameduck_handler_ = enter_lameduck_cb;
    on_stop_handler_ = rpc_stopped_cb;
  }

  static bool WasServerStopped();
private:
  struct Rep;
  std::unique_ptr<Rep> rep_;
  std::function<void()> enter_lameduck_handler_;
  std::function<void()> on_stop_handler_;
};

template <typename Processor, typename ServiceIf> class RpcStoppableProcessor : public Processor {
public:
  RpcStoppableProcessor(ServiceIf* ptr) : Processor(boost::shared_ptr<ServiceIf>(ptr)) {}

  bool dispatchCall(::apache::thrift::protocol::TProtocol* iprot,
                    ::apache::thrift::protocol::TProtocol* oprot, const std::string& fname,
                    int32_t seqid, void* callContext) override {
    if (RpcServer::WasServerStopped())
      return false;
    return Processor::dispatchCall(iprot, oprot, fname, seqid, callContext);
  }
};

}  // namespace util

#endif  // RPC_SERVER_H