// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/rpc_server.h"

#include <signal.h>
#include <unordered_set>

#include "base/commandlineflags.h"
#include "base/endian.h"
#include "base/walltime.h"
#include "strings/strcat.h"
#include "util/http/varz_stats.h"
#include "util/http/http_server.h"

#include <thrift/protocol/TProtocol.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

DEFINE_int32(rpc_worker_threads, 100, "Port number.");
DEFINE_int32(rpc_recv_timeout, 60*1000*10, "Socket receive timeout in ms");

namespace util {

using namespace boost;
using namespace apache::thrift;
using std::string;
using concurrency::ThreadFactory;
using concurrency::ThreadManager;
using apache::thrift::protocol::TBinaryProtocolFactory;
using apache::thrift::protocol::TProtocolFactory;
using apache::thrift::protocol::TProtocol;
using namespace apache::thrift::transport;


namespace {
//http::VarzMapCount rpc_requests("rpc_requests");
http::VarzMapAverage rpc_latency("rpc_latency(ms)");
http::VarzMapCount rpc_latency5ms("slow_rpcs(>5ms)");
http::VarzMapCount rpc_latency15ms("slow_rpcs(>15ms)");
http::VarzQps rpc_qps("rpc_qps");

std::atomic_long opened_connections(0), closed_connections(0);

class RpcEventHandler : public TProcessorEventHandler {
  struct CallContext {
    uint64 start_time_cycles;
  };

  void* getContext(const char* fn_name, void* serverContext) override {
    CallContext* res = new CallContext;
    // We use CycleClock because GetCurrentTimeMicros is 80 times slower (800ns vs 10ns).
    res->start_time_cycles = CycleClock::Now();
    (void)fn_name;
    (void)serverContext;
    return res;
  }

  void freeContext(void* ctx, const char* fn_name) override {
    delete (CallContext*)ctx;
    (void)fn_name;
  }

  void preRead(void* ctx, const char* fn_name) override {
    // rpc_requests.Inc(StrCat(fn_name, "-received"));
    (void)ctx;
  }

  void postWrite(void* ctx, const char* fn_name, uint32_t bytes) override {
    CallContext* cctx = (CallContext*)ctx;
    uint32 ms = CycleClock::toMsec(CycleClock::Now() - cctx->start_time_cycles);
    rpc_latency.IncBy(fn_name, ms);
    if (ms > 15) {
      rpc_latency15ms.Inc(fn_name);
    } else if (ms > 5) {
      rpc_latency5ms.Inc(fn_name);
    }
    rpc_qps.Inc();
    (void) bytes;
  }

  void handlerError(void* ctx, const char* fn_name) override {
    (void) ctx;
    //rpc_requests.Inc(StrCat(fn_name, "-error"));
  }
};

class RpcServerEventHandler : public server::TServerEventHandler {
  std::mutex mu_;
  std::unordered_set<TTransport*> input_sockets_;
 public:

  /**
   * Called when a new client has connected and is about to being processing.
   */
  virtual void* createContext(boost::shared_ptr<TProtocol> input,
                              boost::shared_ptr<TProtocol> output) override {
    ++opened_connections;
    (void)output;
    std::lock_guard<std::mutex> guard(mu_);
    input_sockets_.insert(input->getTransport().get());
    return NULL;
  }

  /**
   * Called when a client has finished request-handling to delete server
   * context.
   */
  virtual void deleteContext(void* serverContext,
                             boost::shared_ptr<TProtocol>input,
                             boost::shared_ptr<TProtocol>output) override {
    ++closed_connections;
    (void)serverContext;
    (void)output;
    std::lock_guard<std::mutex> guard(mu_);
    input_sockets_.erase(input->getTransport().get());
  }

  /**
   * Called when a client is about to call the processor.
   */
  virtual void processContext(void* serverContext,
                              boost::shared_ptr<TTransport> transport) override {
    (void)serverContext;
    (void)transport;
  }

  void Shutdown() {
    std::lock_guard<std::mutex> guard(mu_);
    LOG(INFO) << "Stopping " << input_sockets_.size() << " open connections.";
    for (TTransport* inp : input_sockets_) {
      inp->close();
    }
  }

};

util::RpcServer* rpc_server_instance = nullptr;
volatile bool rpc_server_stopped = false;

void RpcSigHandler(int sig, siginfo_t *info, void *secret) {
  LOG(INFO) << "Catched signal " << sig << ": " << strsignal(sig);
  if (rpc_server_instance) {
    rpc_server_stopped = true;
    rpc_server_instance->Stop();
  }
}

void SetSignalHandler() {
  struct sigaction sa;
  sa.sa_sigaction = RpcSigHandler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

}  // namespace

struct RpcServer::Rep {
  string name;
  int port;
  std::unique_ptr<apache::thrift::server::TServer> server;
  shared_ptr<apache::thrift::TProcessor> processor;
  std::unique_ptr<http::Server> http_server;
  std::unique_ptr<http::VarzListNode> rpc_stats_;
  ThreadManager* thread_mgr_ = nullptr;
  shared_ptr<RpcServerEventHandler> server_event_handler;

  Rep(const string& nm, int p) : name(nm), port(p) {
    rpc_stats_.reset(new http::VarzFunction(
        "rpc_stats", std::bind(&RpcServer::Rep::PrintRpcStats, this)));
  }

  ~Rep() {
    rpc_stats_.reset();
    processor.reset();
    server.reset();
  }

  string PrintRpcStats() {
    string res;
    long opened = opened_connections, closed = closed_connections;
    long open = opened_connections - closed_connections;
    StrAppend(&res, "open_connections: ", open, " total_opened: ", opened, " total_closed: ",
              closed);
    return res;
  }
};

RpcServer::RpcServer(const std::string& name, int port,
                     apache::thrift::TProcessor* processor, http::Server* server)
  : rep_(new Rep{name, port}) {
  rep_->processor.reset(CHECK_NOTNULL(processor));
  rep_->processor->setEventHandler(shared_ptr<TProcessorEventHandler>(new RpcEventHandler));
  if (server != nullptr) {
    rep_->http_server.reset(server);
  } else {
    int http_port = port + 1;
    rep_->http_server.reset(new http::Server(http_port));
  }
  CHECK(rpc_server_instance == nullptr) << "only one rpc server is allowed";
  rpc_server_instance = this;
}

RpcServer::~RpcServer() {
  rpc_server_instance = nullptr;
}

bool RpcServer::WasServerStopped() {
  return rpc_server_stopped;
}

void RpcServer::Start() {
  // Handling threads.
  shared_ptr<ThreadFactory> thread_factory(new concurrency::PosixThreadFactory());
  shared_ptr<ThreadManager> thread_mgr(
      ThreadManager::newSimpleThreadManager(FLAGS_rpc_worker_threads));
  rep_->thread_mgr_ = thread_mgr.get();
  thread_mgr->threadFactory(thread_factory);
  thread_mgr->start();

  // Setup a protocol & transport.
  shared_ptr<TProtocolFactory> protocol_factory(new TBinaryProtocolFactory());
  shared_ptr<TTransportFactory> transport_factory(new TBufferedTransportFactory());
  TServerSocket* server_socket = new TServerSocket(rep_->port);
  server_socket->setRecvTimeout(FLAGS_rpc_recv_timeout);
  shared_ptr<TServerTransport> server_transport(server_socket);
  rep_->server_event_handler.reset(new RpcServerEventHandler);
  rep_->server.reset(new server::TThreadPoolServer(rep_->processor, server_transport,
                     transport_factory, protocol_factory, thread_mgr));
  rep_->server->setServerEventHandler(rep_->server_event_handler);

  LOG(INFO) << "Start serving " << rep_->name << " rpc server on port " << rep_->port;

  Status status = rep_->http_server->Start();
  if (!status.ok()) {
    LOG(ERROR) << "Could not start http server on port " << rep_->http_server->port();
  }
  rep_->http_server->RegisterHandler("/fuckoff", std::bind(&RpcServer::Stop, this));
  SetSignalHandler();
}

void RpcServer::Run() {
  rep_->server->serve();
  LOG(INFO) << "Exiting " << rep_->name;
}

void RpcServer::Stop() {
  LOG(INFO) << "Stopping rpc server...";
  if (enter_lameduck_handler_)
    enter_lameduck_handler_();
  rep_->server->stop();
  rep_->server_event_handler->Shutdown();
  if (on_stop_handler_)
    on_stop_handler_();
}

http::Server* RpcServer::http_server() {
  return rep_->http_server.get();
}

}  // namespace util