// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "util/http/http_server.h"

#include <sys/stat.h>  // open
#include <fcntl.h>

#include <cstring>
#include <mutex>

extern "C" {
  #define EVHTP_DISABLE_REGEX
  #include "third_party/evhtp/evhtp.h"
}

#include "base/logging.h"
#include "strings/numbers.h"
#include "strings/hash.h"
#include "util/http/varz_stats.h"
#include "util/executor.h"

namespace http {

using util::Status;
using base::StatusCode;
using std::bind;
using namespace std::placeholders;

extern std::string BuildStatusPage();

namespace {

struct CallbackPayload {
  Server::HttpHandler handler;
};

VarzMapCount http_requests("http_requests");

int AddKVToVec(evhtp_kv_t* kv, void* arg) {
  Request::KeyValueArray* dest = reinterpret_cast<Request::KeyValueArray*>(arg);
  dest->emplace_back(StringPiece(kv->key, kv->klen), StringPiece(kv->val, kv->vlen));
  return 0;
}

}  // namespace

namespace internal {

void FilezHandler(const Request& request, Response* response);
void ProfilezHandler(const Request& request, Response* response);
void FlagzHandler(const Request& request, Response* response);

}  // namespace internal

struct Request::Rep {
  evhtp_request_t* request_info;
};

const char* Request::method() const {
  htp_method m = rep_->request_info->method;
  return htparser_get_methodstr_m(m);
}

const char* Request::uri() const {
  return rep_->request_info->uri->path->full;
}

StringPiece Request::query() const {
  return StringPiece(reinterpret_cast<char*>(rep_->request_info->uri->query_raw));
}

std::vector<std::pair<StringPiece, StringPiece>> Request::ParsedQuery() const {
  std::vector<std::pair<StringPiece, StringPiece>> res;
  evhtp_kvs_for_each(rep_->request_info->uri->query, AddKVToVec, &res);
  /*vector<StringPiece> parts = strings::Split(query(), "&", strings::SkipEmpty());
  std::vector<std::pair<StringPiece, StringPiece>> res;
  for (StringPiece p : parts) {
    size_t pos = p.find('=');
    if (pos == StringPiece::npos) {
      res.emplace_back(p, StringPiece());
    } else {
      res.emplace_back(p.substr(0, pos), p.substr(pos + 1));
    }
  }*/
  return res;
}

/**
 Reponse implementation
 **/

const char Response::kHtmlMime[] = "text/html";
const char Response::kTextMime[] = "text/plain";
const char Response::kJsonMime[] = "application/json";

struct Response::Rep {
  evhtp_request_t* request;
};

void Response::SetContentType(const char* mime_type) {
  AddHeader("Content-Type", mime_type);
}

util::Status Response::Send(HttpStatusCode code) {
  evhtp_send_reply(rep_->request, code);
  return Status::OK;
}

Response& Response::AppendContent(StringPiece str) {
  int res = evbuffer_add(rep_->request->buffer_out, str.data(), str.size());
  if (res) {
    LOG(ERROR) << "Error adding to buffer string of size " << str.size();
  }
  return *this;
}

Response& Response::AddHeader(const char* header, const char* value) {
  evhtp_headers_add_header(rep_->request->headers_out, evhtp_header_new(header, value, 0, 0));
  return *this;
}

Response& Response::AddHeaderCopy(const char* header, const char* value) {
  evhtp_headers_add_header(rep_->request->headers_out, evhtp_header_new(header, value, 0, 1));
  return *this;
}

void Response::SendFile(const char* local_file, HttpStatusCode code) {
  LOG(INFO) << "Trying to open file: " << local_file;
  int fd = open(local_file, O_RDONLY);
  struct stat st;
  int err = fstat(fd, &st);
  if (fd < 0 || err < 0) {
    AppendContent("<p>Not found</p>").Send(HTTP_NOT_FOUND);
    return;
  }

#define POSIX_CALL(x) err = x; \
    if (err < 0) break;

  evhtp_send_reply_chunk_start(rep_->request, code);
  evbuf_t* buf = evbuffer_new();
  struct evbuffer_iovec iovec;
  off_t offset = 0;
  while (offset < st.st_size) {
    size_t length = st.st_size - offset > 4096 ? 4096 : st.st_size - offset;
    POSIX_CALL(evbuffer_reserve_space(buf, length, &iovec, 1));
    POSIX_CALL(read(fd, iovec.iov_base, length));
    iovec.iov_len = length;
    POSIX_CALL(evbuffer_commit_space(buf, &iovec, 1));
    evhtp_send_reply_chunk(rep_->request, buf);
    offset += length;
    POSIX_CALL(evbuffer_drain(buf, -1));
  }
  if (err < 0) {
    LOG(ERROR) << "Error " << strerror(err);
  }
  evhtp_send_reply_chunk_end(rep_->request);
  close(fd);
  evbuffer_free(buf);
#undef POSIX_CALL
}

struct Server::Rep {
  int port;

  evhtp_t* htp;
  bool socket_bound = false;
  vector<CallbackPayload*> keeper;
  std::mutex handlers_mutex;

  Rep(int p) : port(p) {
    htp  = evhtp_new(util::Executor::Default().ebase(), NULL);
  }

  ~Rep() {
    evhtp_free(htp);
    for (CallbackPayload* v : keeper)
      delete v;
  }

  static void main_callback(evhtp_request_t* req, void* arg);

  static void DefaultRootHandler(const Request& request, Response* response) {
    StringPiece uri(request.uri());
    if (uri == "/favicon.ico") {
      // taken from http://www.favicon.cc/?action=icon&file_id=627311
      response->AppendContent("<head>\n"
          "<link href='data:image/x-icon;base64,AAABAAEAEBAAAAAAAABoBQAAFgAAACgAAAAQAAAAIAAAAAEACA"
          "AAAAAAAAEAAAAAAAAAAAAAAAEAAAAAAAAAAAAA////ACEhIQAMDAwAkpKSAOjo6AC5ubkACgoKAMbGxgArKysAb"
          "m5uAAICAgAqKioAUlJSACMjIwDExMQAMDAwAN7e3gCUlJQ");
      response->AppendContent(string(1326, 'A'));
      response->AppendContent("QEBAQEBAQEBAQUBAQEBAQEBAQEBAQEBAAAAAAcBAQEBAQAAAAABCwAAAAAAAAEBAQAAA"
          "AAADAAAAAAAAAAAAQIAAAAAAAEAAAAAAAAAAAEBAwAACAEBAAAAAAAAAAAKAQEBAAAAAQAAAAAAAAAADQERAAAA"
          "AAYAAAAAAAAAAAEBAAAAAAABAAAAAAAAAAEBAQAAAAABDgAQAQESAQEBAQEECQEBAAAAAAEAAAABAQEBAQEBAAA"
          "AAAMAAAAAAQEBAQEBAQAAAAABAAAAAAEBAQEBAQEAAAABAQAAAAEBAQEBAQEBAQEBAQEAAA8BAQEBAQEBAQEBAQ"
          "EBAQEBAQEBAQAAAAAAAAAAAAAAAAAAAAAAAAA=' rel='icon' type='image/x-icon' />");
      response->AppendContent("</head>\n");
      response->Send(HTTP_OK);
      return;
    }
    if (uri != "/") {
      response->Send(HTTP_NOT_FOUND);
      return;
    }
    response->SetContentType(Response::kHtmlMime);
    response->AppendContent(BuildStatusPage());
    response->Send(HTTP_OK);
  }
};

void Server::Rep::main_callback(evhtp_request_t* req, void* arg) {
  CallbackPayload* payload = reinterpret_cast<CallbackPayload*>(arg);
  if (payload == nullptr) {
    evhtp_send_reply(req, EVHTP_RES_SERVERR);
    return;
  }

  Request::Rep req_rep{req};
  Request request(&req_rep);
  VLOG(2) << "Getting http requests " << request.uri();
  http_requests.Inc("received");

  Response::Rep response_rep{req};
  Response response(&response_rep);
  response.SetContentType(Response::kTextMime);

  http_requests.Inc("handled");
  payload->handler(request, &response);
}

Server::Server(int port, string directory_root) : rep_(new Rep{port}),
  directory_root_(directory_root) {
}

Server::~Server() {
  Shutdown();
}

void Server::RegisterHandler(StringPiece url, HttpHandler handler) {
  std::lock_guard<std::mutex> guard(rep_->handlers_mutex);
  CallbackPayload* payload = new CallbackPayload{handler};
  rep_->keeper.push_back(payload);
  evhtp_set_cb(rep_->htp, url.data(), Rep::main_callback, payload);
}

util::Status Server::Start() {
  evhtp_bind_socket(rep_->htp, "0.0.0.0", rep_->port, 10);
  rep_->socket_bound = true;

  LOG(INFO) << "Starting http server on port " << rep_->port;

  RegisterHandler("/", std::bind<void>(&Rep::DefaultRootHandler, _1, _2));

  RegisterHandler("/profilez", std::bind(internal::ProfilezHandler, _1, _2));
  RegisterHandler("/filez", std::bind(internal::FilezHandler, _1, _2));
  RegisterHandler("/flagz", std::bind(internal::FlagzHandler, _1, _2));
  return Status::OK;
}

void Server::Shutdown() {
  if (rep_->socket_bound) {
    evhtp_unbind_socket(rep_->htp);
    rep_->socket_bound = false;
  }
}

void Server::Wait() {
  // Wait for signal indicating time to shut down.
  /*sigset_t wait_mask;
  sigemptyset(&wait_mask);
  sigaddset(&wait_mask, SIGINT);
  sigaddset(&wait_mask, SIGQUIT);
  sigaddset(&wait_mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &wait_mask, 0);
  int sig = 0;
  sigwait(&wait_mask, &sig);
*/
  util::Executor::Default().StopOnTermSignal();
  util::Executor::Default().WaitForLoopToExit();
  Shutdown();
}

int Server::port() const {
  return rep_->port;
}

}  // namespace http