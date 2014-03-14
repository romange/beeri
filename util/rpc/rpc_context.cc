// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/rpc/rpc_context.h"

namespace util {
namespace rpc {

void Context::SetFailed(const std::string& reason) {
  status_.set_details(reason);
  status_.set_code(Status::UNKNOWN_ERROR);
}

bool Context::Failed() const {
  return status_.code() != Status::OK;
}

Context::~Context() {
}

}  // namespace util
}  // namespace util
