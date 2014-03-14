// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/googleinit.h"
#include "experimental/ExampleService.h"
#include "util/rpc/rpc_client.h"

DEFINE_string(address, "", "Host:Port pair.");
DEFINE_int32(count, 1, "");

using namespace std;

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);
  CHECK(!FLAGS_address.empty());
  util::RpcClient<bla::ExampleServiceClient> client(FLAGS_address);
  client.OpenWithRetry(0, 2000);
  LOG(INFO) << "After Open";
  int32 res = 0;
  for (int i = 0; i < FLAGS_count; ++i) {
     res = client->bar(10);
  }
  LOG(INFO) << "Exiting... with result " << res;
  return 0;
}
