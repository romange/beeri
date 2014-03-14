// Copyright 2012 Cloudera Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified by: Roman Gershman (romange@gmail.com)
#include "base/status.h"

using std::string;

namespace base {

// NOTE: this is statically initialized and we must be very careful what
// functions these constructors call.  In particular, we cannot call
// glog functions which also rely on static initializations.
// TODO: is there a more controlled way to do this.
const Status Status::OK;
const Status Status::CANCELLED(StatusCode::CANCELLED, "Cancelled");

void Status::AddErrorMsg(StatusCode::Code code, const std::string& msg) {
  if (error_detail_ == NULL) {
    error_detail_ = new ErrorDetail(code, msg);
  } else {
    error_detail_->error_msgs.push_back(msg);
  }
  VLOG(2) << msg;
}

void Status::AddErrorMsg(const std::string& msg) {
  AddErrorMsg(StatusCode::INTERNAL_ERROR, msg);
}

void Status::AddError(const Status& status) {
  if (status.ok()) return;
  AddErrorMsg(status.code(), status.ToString());
}

void Status::GetErrorMsgs(std::vector<string>* msgs) const {
  msgs->clear();
  if (error_detail_ != NULL) {
    *msgs = error_detail_->error_msgs;
  }
}

void Status::GetErrorMsg(string* msg) const {
  msg->clear();
  if (error_detail_ != NULL) {
    if (StatusCode::Code_IsValid(error_detail_->error_code)) {
      const string& str = StatusCode::Code_Name(error_detail_->error_code);
      msg->append(str).append(" ");
    }
    for (const string& e : error_detail_->error_msgs) {
      msg->append(e).append("\n");
    }
    if (!error_detail_->error_msgs.empty()) {
      msg->pop_back();
    }
  } else {
    msg->assign("OK");
  }
}

}  // namespace util

std::ostream& operator<<(std::ostream& o, const base::Status& status) {
  o << status.ToString();
  return o;
}