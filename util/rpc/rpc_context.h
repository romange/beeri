// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _RPC_CONTEXT_H
#define _RPC_CONTEXT_H

#include <condition_variable>
#include <google/protobuf/service.h>

#include "base/logging.h"
#include "util/rpc/rpc.pb.h"

namespace util {
namespace rpc {

class Context : public ::google::protobuf::RpcController {
 public:
  inline Context() {}
  virtual ~Context();

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  // Client-side methods ---------------------------------------------
  // These calls may be made from the client side only.  Their results
  // are undefined on the server side (may crash).

  // Resets the RpcController to its initial state so that it may be reused in
  // a new call.  Must not be called while an RPC is in progress.
  virtual void Reset() {
    LOG(FATAL) << "Not implemented";
  }

  // After a call has finished, returns true if the call failed.  The possible
  // reasons for failure depend on the RPC implementation.  Failed() must not
  // be called before a call has finished.  If Failed() returns true, the
  // contents of the response message are undefined.
  virtual bool Failed() const;

  // If Failed() is true, returns a human-readable description of the error.
  virtual std::string ErrorText() const {
    LOG(FATAL) << "Not implemented";
    return "";
  }

  // Advises the RPC system that the caller desires that the RPC call be
  // canceled.  The RPC system may cancel it immediately, may wait awhile and
  // then cancel it, or may not even cancel the call at all.  If the call is
  // canceled, the "done" callback will still be called and the RpcController
  // will indicate that the call failed at that time.
  virtual void StartCancel() {
    LOG(FATAL) << "Not implemented";
  }

  // Server-side methods ---------------------------------------------
  // These calls may be made from the server side only.  Their results
  // are undefined on the client side (may crash).

  // Causes Failed() to return true on the client side.  "reason" will be
  // incorporated into the message returned by ErrorText().  If you find
  // you need to return machine-readable information about failures, you
  // should incorporate it into your response protocol buffer and should
  // NOT call SetFailed().
  virtual void SetFailed(const std::string& reason);

  // If true, indicates that the client canceled the RPC, so the server may
  // as well give up on replying to it.  The server should still call the
  // final "done" callback.
  virtual bool IsCanceled() const {
    return false;
  }

  // Asks that the given callback be called when the RPC is canceled.  The
  // callback will always be called exactly once.  If the RPC completes without
  // being canceled, the callback will be called after completion.  If the RPC
  // has already been canceled when NotifyOnCancel() is called, the callback
  // will be called immediately.
  //
  // NotifyOnCancel() must be called no more than once per request.
  virtual void NotifyOnCancel(::google::protobuf::Closure* callback) {
    LOG(FATAL) << "Not implemented";
  }

  const Status& status() const { return status_; }

  void set_status(const rpc::Status& status) {
    status_ = status;
  }

  rpc::Status* mutable_status() { return &status_; }

  void SetError(rpc::Status::Code code, const std::string& details = std::string()) {
    status_.set_code(code);
    if (!details.empty())
      status_.set_details(details);
  }
 private:
  Status status_;
};

class DoneBarrier : public ::google::protobuf::Closure {
  std::condition_variable cv_;
  std::mutex mu_;
  bool done_ = false;
public:
  void Run() {
    std::unique_lock<std::mutex> lk(mu_);
    done_ = true;
    cv_.notify_all();
  }

  // Returns true if run was called, false if the deadline was expired.
  bool Wait(uint64_t msec = -1) {
    std::unique_lock<std::mutex> lk(mu_);
    while (!done_) {
      std::cv_status status = cv_.wait_for(lk, std::chrono::milliseconds(msec));
      if (status == std::cv_status::timeout)
        return false;
    }
    return true;
  }
};

}  // namespace rpc
}  // namespace util

#endif  // _RPC_CONTEXT_H