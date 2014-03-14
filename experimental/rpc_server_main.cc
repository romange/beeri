// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/googleinit.h"
#include "base/logging.h"
#include "file/file_util.h"
#include "util/http/http_server.h"
#include "util/rpc/rpc_server.h"
#include "experimental/ExampleService.h"

#include <vector>

using namespace std::placeholders;

DEFINE_int32(port, 8081, "Port number.");

class ExampleServiceImpl : public bla::ExampleServiceIf {
 public:
  ExampleServiceImpl() {}

  virtual void foo(bla::Response& return_arg, const bla::Request& req) {
    return_arg.code = res++;
  };

  virtual int32_t bar(const int32_t arg) {
    return res++;
  }

  int res = 10;
};

class HttpServer : public http::Server {
public:
  explicit HttpServer(int port) : http::Server(port, "rpc_server_main.runfiles/") {
    //CHECK(file_util::ReadFileToString(, &page_));
    //LOG(INFO) << page_;
    /*HttpHandler handler = std::bind(&HttpServer::ServeMap, this, _1, _2);
    RegisterHandler("/", handler);*/
  }
private:
  void ServeMap(const http::Request& /*request*/, http::Response* response) {
    response->SetContentType(http::Response::kHtmlMime);
    response->AppendContent(page_);
    response->Send(http::HTTP_OK);
  }

  string page_;
};

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);

  boost::shared_ptr<bla::ExampleServiceIf> handler(new ExampleServiceImpl());
  util::RpcServer server("ExampleServer", FLAGS_port,
      new bla::ExampleServiceProcessor(handler), new HttpServer(FLAGS_port + 1));
  server.Start();
  server.Run();
  LOG(INFO) << "Exiting server...";

  return 0;
}

