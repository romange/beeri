// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/googleinit.h"
#include "util/rpc/rpc_sample.pb.h"

#include "util/rpc/executor.h"
#include "util/rpc/rpc_channel.h"
#include "util/rpc/rpc_context.h"
#include <thread>
#include <iostream>
#include <atomic>
#include <chrono>

DEFINE_string(address, "", "Host:Port pair.");
DEFINE_int32(num_requests, 1000, "");
DEFINE_int32(num_bursts, 10, "");
//DEFINE_int32(num_threads, 10, "");
DEFINE_int32(deadline, 100, "");
DEFINE_int32(sleep, 1, "");

using namespace ::rpc::testing;
using namespace std;
using std::chrono::milliseconds;

static std::atomic_ulong total_timeouts(0);
static std::atomic_ullong total_time(0);
static std::atomic_ulong total_success(0);
static std::atomic_long pending_calls(0);

namespace util {
namespace rpc {

struct PendingCall {
  Request request;
  Response response;
  Context context;
  chrono::system_clock::time_point start;
};

void DoneCallback(PendingCall* call) {
  std::unique_ptr<PendingCall> guard(call);
  if (call->context.Failed()) {
    VLOG(1) << "Failed " << call->context.status().ShortDebugString();
    ++total_timeouts;
    return;
  }
  chrono::system_clock::time_point now = chrono::system_clock::now();

  auto duration = chrono::duration_cast<chrono::nanoseconds>(now - call->start);
  if (duration.count()/1000000 > FLAGS_deadline)
    ++total_timeouts;
  else {
    total_time += duration.count();
    ++total_success;
  }
}

void ClientFunction() {
  std::unique_ptr<Channel> channel(new Channel(&Executor::Default(), FLAGS_address));
  std::unique_ptr<TestRpcService::Stub> stub(new TestRpcService::Stub(channel.get()));

  for (int i = 0;  i < FLAGS_num_requests; ++i) {
    for (int j = 0; j < FLAGS_num_bursts; ++j) {
      PendingCall* call = new PendingCall;
      call->start = chrono::system_clock::now();
      stub->func1(&call->context, &call->request, &call->response,
                  gpb::NewCallback(DoneCallback, call));
    }
    this_thread::sleep_for(milliseconds(FLAGS_sleep));
  }
}

}  // namespace rpc
}  // namespace util

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);
  CHECK(!FLAGS_address.empty());
  auto start = chrono::system_clock::now();
  unsigned long total_requests = FLAGS_num_requests*FLAGS_num_bursts;

  util::rpc::ClientFunction();

  while (total_success + total_timeouts < total_requests) {
     this_thread::sleep_for(milliseconds(20));
  }
  LOG(INFO) << "Timeout ratio: " << double(total_timeouts) / total_requests;
  LOG(INFO) << "Average latency " << double(total_time)/(1000000.0*total_success);
  auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start);
  LOG(INFO) << "QPS: " <<  double(total_requests)/ duration.count()*1000.0;
  return 0;
}