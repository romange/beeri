// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include <event2/thread.h>
#include <event2/event.h>

#include "base/googleinit.h"

#include "util/executor.h"
#include "util/rpc/rpc_server2.h"
#include "util/rpc/rpc_sample.pb.h"

DEFINE_int32(port, 45000, "server port");

namespace gpb = ::google::protobuf;

class TestRpcServiceImpl : public ::rpc::testing::TestRpcService {
 public:
  virtual void func1(gpb::RpcController* controller,
                     const ::rpc::testing::Request* request,
                     ::rpc::testing::Response* response,
                     gpb::Closure* done) {
    response->set_result("Hello World!");
    done->Run();
  }
};


int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);
  CHECK_EQ(0, evthread_use_pthreads());

  TestRpcServiceImpl test_service;

  util::rpc::RpcServer server("SampleRpcServer");
  server.ExportService(&test_service);
  util::Executor* executor = &util::Executor::Default();
  server.Open(FLAGS_port, executor);
  executor->WaitForLoopToExit();

  return 0;
}
