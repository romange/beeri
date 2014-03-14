// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/rpc_channel.h"

#include <array>
#include <condition_variable>
#include <future>
#include <memory>
#include <thread>

#include <event2/event.h>
#include <event2/thread.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "util/rpc/executor.h"
#include "util/rpc/rpc_sample.pb.h"
#include "util/rpc/rpc_context.h"
#include "util/rpc/rpc_server2.h"
#include "util/scheduler.h"

namespace util {
namespace rpc {

using namespace ::rpc::testing;
using namespace std::chrono;

static const char kTestString[] = "Hello World, ";

class TestRpcServiceImpl : public TestRpcService {
 public:
  uint32 msec_delay = 0;
  virtual void func1(gpb::RpcController* controller,
                     const ::rpc::testing::Request* request,
                     ::rpc::testing::Response* response,
                     gpb::Closure* done) {
    string str(kTestString);
    response->set_result(str + request->name());
    /*std::this_thread::sleep_for(milliseconds(msec_delay));
    done->Run();*/

    Scheduler::Default().Schedule([done]() {done->Run();},
        milliseconds(msec_delay > 0 ? msec_delay : 1), false);
  }
};


class RpcTest : public testing::Test {
protected:
  static void SetUpTestCase() {
  }

  void SetUp() override {
    executor_.reset(new Executor);
    SetupRpcServer();
    channel_.reset(new Channel(executor_.get(), "localhost:45000"));
    stub_.reset(new TestRpcService::Stub(channel_.get()));
    request_.set_name("Roman");
  }

  void TearDown() override {
    rpc_server_.reset(nullptr);
    channel_.reset(nullptr);
    VLOG(1) << "Closing executor";
    executor_.reset(nullptr);
  }

  void SetupRpcServer() {
    rpc_server_.reset(new RpcServer("TestRpcServer"));
    rpc_server_->ExportService(&service_impl_);
    rpc_server_->Open(45000, executor_.get());
  }

  std::unique_ptr<Executor> executor_;
  std::unique_ptr<TestRpcService::Stub> stub_;
  std::unique_ptr<Channel> channel_;
  TestRpcServiceImpl service_impl_;
  std::unique_ptr<RpcServer> rpc_server_;
  Context rpc_context_;
  DoneBarrier barrier_;
  Request request_;
  Response response_;
};

TEST_F(RpcTest, Basic) {
  ASSERT_TRUE(channel_->WaitToConnect(1000));

  stub_->func1(&rpc_context_, &request_, &response_, &barrier_);
  ASSERT_TRUE(barrier_.Wait(1000));
  EXPECT_EQ(Status::OK, rpc_context_.status().code());
  string expected = kTestString + request_.name();
  EXPECT_EQ(expected, response_.result());
}

TEST_F(RpcTest, DeadlineExceededSingle) {
  ASSERT_TRUE(channel_->WaitToConnect(100));

  channel_->set_rpc_deadline(10);
  service_impl_.msec_delay = 20;

  auto start = steady_clock::now();
  stub_->func1(&rpc_context_, &request_, &response_, &barrier_);
  ASSERT_TRUE(barrier_.Wait(1000));  // called even though server did not send the reply yet
  EXPECT_EQ(Status::DEADLINE_EXCEEDED, rpc_context_.status().code());
  int64 duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
  EXPECT_LT(duration, 12);
  EXPECT_GE(duration, 10);

  // Make sure that server callbacks are run.
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
}

TEST_F(RpcTest, DeadlineExceededMany) {
  ASSERT_TRUE(channel_->WaitToConnect(1000));

  channel_->set_rpc_deadline(10);
  service_impl_.msec_delay = 20;

  constexpr int kNumCalls = 10;
  std::array<Response, kNumCalls> responses;
  std::array<Context, kNumCalls> contexts;
  std::array<DoneBarrier, kNumCalls> done_barriers;
  auto start = steady_clock::now();
  LOG(INFO) << "Start sending many rpcs";
  for (int i = 0; i < responses.size(); ++i) {
     stub_->func1(&contexts[i], &request_, &responses[i], &done_barriers[i]);
  }
  for (int i = 0; i < done_barriers.size(); ++i) {
    ASSERT_TRUE(done_barriers[i].Wait(1000));
  }
  LOG(INFO) << "Finished sending many rpcs";
  int64 duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
  for (int i = 0; i < contexts.size(); ++i) {
    EXPECT_EQ(Status::DEADLINE_EXCEEDED, contexts[i].status().code());
  }
  EXPECT_GE(duration, 10);
  for (int i = 0; i < contexts.size(); ++i) {
    ASSERT_TRUE(done_barriers[i].Wait(1000));
  }
}

TEST_F(RpcTest, ConnectionStopped) {
  ASSERT_TRUE(channel_->WaitToConnect(100));
  stub_->func1(&rpc_context_, &request_, &response_, &barrier_);
  ASSERT_TRUE(barrier_.Wait(100));  // called even though server did not send the reply yet
  EXPECT_EQ(Status::OK, rpc_context_.status().code());

  DoneBarrier done2;
  Context rpc2;
  rpc_server_.reset(nullptr);

  stub_->func1(&rpc2, &request_, &response_, &done2);
  ASSERT_TRUE(done2.Wait(100));  // called even though server did not send the reply yet
  EXPECT_EQ(Status::CONNECTION_REFUSED, rpc2.status().code());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  SetupRpcServer();
  channel_->WaitToConnect(3000);

  DoneBarrier done3;
  Context rpc3;
  stub_->func1(&rpc3, &request_, &response_, &done3);
  ASSERT_TRUE(done3.Wait(100));
  EXPECT_EQ(Status::OK, rpc3.status().code());

  DoneBarrier done4;
  Context rpc4;
  stub_->func1(&rpc4, &request_, &response_, &done4);
  channel_.reset(nullptr);
  ASSERT_TRUE(done4.Wait(100));
  EXPECT_EQ(Status::CONNECTION_REFUSED, rpc4.status().code());
}

}  // namespace rpc
}  // namespace util
