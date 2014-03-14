// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "rpc_client.h"

#include "strings/numbers.h"
#include "strings/stringpiece.h"
#include "strings/split.h"
#include "strings/strcat.h"

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>

using util::Status;
using namespace apache::thrift::transport;

namespace util {

bool HostPort::Parse(const std::string& hostport, HostPort* result) {
  using strings::SkipEmpty;
  std::vector<StringPiece> v = strings::Split(hostport, ":", SkipEmpty());
  if (v.size() != 2) return false;
  int32 p = 0;
  if (!safe_strto32(v[1], &p)) return false;
  result->port = p;
  result->host = v[0].ToString();
  return true;
}

HostPort::HostPort(const string& hostport) {
  CHECK(HostPort::Parse(hostport, this)) << "Could not parse " << hostport;
}

RpcClientBase::RpcClientBase(const HostPort& host_port) {
  socket_.reset(new TSocket(host_port.host, host_port.port));
  //socket_->setConnTimeout(1000000);
  transport_.reset(new TBufferedTransport(socket_));
  // transport_.reset(new TFramedTransport(socket_));
  protocol_.reset(new apache::thrift::protocol::TBinaryProtocol(transport_));
}

std::string RpcClientBase::host() {
  return socket_->getHost();
}

int RpcClientBase::port() {
  return socket_->getPort();
}

Status RpcClientBase::Open() {
  try {
    if (!transport_->isOpen()) {
      transport_->open();
    }
  } catch (TTransportException& e) {
    return Status(StrCat("Couldn't open transport for ", host(), ":", port(),
                               "(", e.what(), ")"));
  }
  return Status::OK;
}

Status RpcClientBase::OpenWithRetry(uint32 num_tries, uint32 wait_ms) {
  DCHECK_GE(wait_ms, 10);
  Status status;
  uint32 try_count = 0L;
  if (num_tries == 0) num_tries = kuint32max;
  while (try_count < num_tries) {
    ++try_count;
    status = Open();
    if (status.ok()) return status;
    LOG(INFO) << "Unable to connect to " << host() << ":" << port()
              << "  (Attempt " << try_count << " of "
              << (num_tries > 0 ? SimpleItoa(num_tries) : string("inf")) << ")";
    usleep(wait_ms * 1000L);
  }

  return status;
}

Status RpcClientBase::Close() {
  transport_->close();
  return Status::OK;
}

void RpcClientBase::SetReceiveDeadline(uint32 ms) {
  socket_->setRecvTimeout(ms);
}

Status RpcClientBase::Call(std::function<void()> f) {
  if (!transport_->isOpen()) {
    try {
      transport_->open();
    } catch (TTransportException& e) {
      LOG(ERROR) <<  "Can not reopen transport: " << e.what();
      return Status(base::StatusCode::IO_ERROR);
    }
  }
  if (transport_->isOpen()) {
    try {
      f();
    } catch (const apache::thrift::TException& e) {
      return Status(base::StatusCode::IO_ERROR, e.what());
    }
    return Status::OK;
  }
  return Status(base::StatusCode::RUNTIME_ERROR);
}

}  // namespace util