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

// Modified by: Roman Gershman (romange@gmail.com)

#ifndef _BASE_STATUS_H
#define _BASE_STATUS_H

#include <string>
#include <vector>
#include "base/logging.h"
#include "base/port.h"
#include "base/status.pb.h"

// Status is used as a function return type to indicate success, failure or cancellation
// of the function. In case of successful completion, it only occupies sizeof(void*)
// statically allocated memory. In the error case, it records a stack of error messages.
//
// example:
// Status fnB(int x) {
//   Status status = fnA(x);
//   if (!status.ok()) {
//     status.AddErrorMsg("fnA(x) went wrong");
//     return status;
//   }
// }
//
namespace base {

class Status {
 public:
  Status(): error_detail_(NULL) {}

  static const Status OK;
  static const Status CANCELLED;

  // copy c'tor makes copy of error detail so Status can be returned by value
  Status(const Status& status) : error_detail_(
        status.error_detail_ != NULL ? new ErrorDetail(*status.error_detail_) : NULL) {
  }

  // Move c'tor.
  Status(Status&& st) : error_detail_(st.error_detail_) {
    st.error_detail_ = nullptr;
  }

  // c'tor for error case - is this useful for anything other than CANCELLED?
  Status(StatusCode::Code code)
    : error_detail_(new ErrorDetail(code)) {
  }

  // c'tor for error case
  Status(StatusCode::Code code, std::string error_msg)
    : error_detail_(new ErrorDetail(code, std::move(error_msg))) {
  }

  // c'tor for internal error
  Status(std::string error_msg) : error_detail_(
    new ErrorDetail(StatusCode::INTERNAL_ERROR, std::move(error_msg))) {}

  ~Status() {
    if (error_detail_ != NULL) delete error_detail_;
  }

  // same as copy c'tor
  Status& operator=(const Status& status) {
    delete error_detail_;
    if (PREDICT_TRUE(status.error_detail_ == NULL)) {
      error_detail_ = NULL;
    } else {
      error_detail_ = new ErrorDetail(*status.error_detail_);
    }
    return *this;
  }

  bool ok() const { return error_detail_ == NULL; }

  bool IsCancelled() const {
    return error_detail_ != NULL
        && error_detail_->error_code == StatusCode::CANCELLED;
  }

  // Does nothing if status.ok().
  // Otherwise: if 'this' is an error status, adds the error msg from 'status;
  // otherwise assigns 'status'.
  void AddError(const Status& status);
  void AddErrorMsg(StatusCode::Code code, const std::string& msg);
  void AddErrorMsg(const std::string& msg);
  void GetErrorMsgs(std::vector<std::string>* msgs) const;
  void GetErrorMsg(std::string* msg) const;

  std::string ToString() const {
    std::string msg; GetErrorMsg(&msg);
    return msg;
  }

  StatusCode::Code code() const {
    return error_detail_ == NULL ? StatusCode::OK : error_detail_->error_code;
  }

 private:
  struct ErrorDetail {
    StatusCode::Code error_code;  // anything other than OK
    std::vector<std::string> error_msgs;

    ErrorDetail(StatusCode::Code code)
      : error_code(code) {}
    ErrorDetail(StatusCode::Code code, std::string msg)
      : error_code(code) {
        error_msgs.push_back(std::move(msg));
    }
  };

  ErrorDetail* error_detail_;
};

// some generally useful macros
#define RETURN_IF_ERROR(stmt) \
  do { \
    Status __status__ = (stmt); \
    if (PREDICT_FALSE(!__status__.ok())) return __status__; \
  } while (false)

// Sometimes functions need to return both data object and status.
// It's inconvenient to set this data object by reference via argument parameter.
// StatusObject should help with this problem.
template<typename T> struct StatusObject {
  Status status;
  T obj;

  bool ok() const { return status.ok(); }

  StatusObject(const Status& s) : status(s), obj() {}
  StatusObject(Status&& s) : status(std::move(s)), obj() {}
  StatusObject(const T& t) : obj(t) {}
};

}  // namespace base

extern std::ostream& operator<<(std::ostream& o, const base::Status& status);

#endif
