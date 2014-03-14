// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef UTIL_RPC_CLIENT_H
#define UTIL_RPC_CLIENT_H

#include <functional>
#include <string>
#include "util/status.h"
#include <thrift/protocol/TBinaryProtocol.h>

namespace apache {
namespace thrift {
namespace transport {
class TSocket;
class TTransport;
}  // namespace transport
}  // namespace thrift
}  // namespace apache

namespace util {

struct HostPort {
public:
  HostPort(const std::string& hostport);

  static bool Parse(const std::string& hostport, HostPort* result);

  std::string host;
  int port = -1;
};

class RpcClientBase {
 public:
  ~RpcClientBase() {
    Close();
  }

  std::string host();
  int port();

  // Open the connection to the remote server. May be called
  // repeatedly, is idempotent unless there is a failure to connect.
  util::Status Open();

  // Retry the Open num_retries time waiting wait_ms milliseconds between retries.
  util::Status OpenWithRetry(uint32 num_retries, uint32 wait_ms);

  // Close the connection with the remote server. May be called
  // repeatedly.
  util::Status Close();

  void SetReceiveDeadline(uint32 ms);

  //apache::thrift::transport::TSocket* socket() {return socket_.get();}
  apache::thrift::transport::TTransport* transport() {return transport_.get();}

  // Call the function, tries to overcome some of the connection problem that might raise.
  Status Call(std::function<void()> f);

 protected:
  RpcClientBase(const HostPort& host_port);

  boost::shared_ptr<apache::thrift::protocol::TBinaryProtocol> protocol_;
private:
  boost::shared_ptr<apache::thrift::transport::TSocket> socket_;
  boost::shared_ptr<apache::thrift::transport::TTransport> transport_;
};

template<typename T> class RpcClient : public RpcClientBase {
public:
  typedef T Interface;

  explicit RpcClient(const HostPort& host_port) : RpcClientBase(host_port) {
    interface_.reset(new Interface(protocol_));
  }

  Interface* operator->() { return iface(); }
  Interface* iface() { return interface_.get(); }
private:
  std::unique_ptr<Interface> interface_;
};


}  // namespace util

#endif  // UTIL_RPC_CLIENT_H