// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "util/rpc/rpc_channel.h"

#include <chrono>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>

#include "base/logging.h"
#include "base/thread_annotations.h"
#include "strings/numbers.h"

#include "util/executor.h"
#include "util/rpc/rpc_common.h"
#include "util/rpc/rpc_context.h"
#include "util/rpc/rpc_message_reader.h"
#include "util/scheduler.h"

namespace util {
namespace rpc {

using namespace std::placeholders;
using namespace std::chrono;

namespace {

void resolve_host_pair(StringPiece host, int port, struct addrinfo** servinfo) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints); // make sure the struct is empty

  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
  char buf[12];

  int status = getaddrinfo(host.data(), FastIntToBuffer(port, buf), &hints, servinfo);
  CHECK_EQ(0, status) << "getaddrinfo error: " << gai_strerror(status);
}

struct PendingDeadline {
    uint64 milliseconds_since_epoch;
    int64 event_id;

    bool was_responded() const { return milliseconds_since_epoch == 0;}
    void set_responded() { milliseconds_since_epoch = 0; }
};

struct OutstandingCall {
  Context* context;
  gpb::Message* response;
  gpb::Closure* done;
};

enum ChannelState {
  DISCONNECTED,
  CONNECTED,
  SHUTTING_DOWN,
};

inline std::function<void()> ClosureFunc(gpb::Closure* cb) {
  return std::bind(&gpb::Closure::Run, cb);
}

}  // namespace

struct Channel::Rep {
  struct addrinfo* serv_addr = nullptr;

  std::atomic<ChannelState> state;

  std::shared_ptr<bufferevent> bev;
  uint32 issued_callbacks_ = 0;

  explicit Rep(Executor* exec) : state(DISCONNECTED),
     executor_(exec), // bev_(nullptr),
     alarmer_handler_id_(Scheduler::INVALID_HANDLE) {}

  ~Rep();

  void FlushOutstanding() {
    VLOG(2) << "FlushOutstanding " << outstanding_calls_.size();
    for (auto& k_v : outstanding_calls_) {
      k_v.second.context->SetError(Status::CONNECTION_REFUSED);
      executor_->Add(ClosureFunc(k_v.second.done));
    }
    outstanding_calls_.clear();
  }

  void ConnectWithAddrInfo(int cause);
  bool WaitToConnect(uint32 milliseconds);

  void AddRequest(const gpb::Message* request, const RpcControlRequest& rpc_info,
                  const OutstandingCall& call, uint32 deadline);

private:
  void ReplyHandler(strings::Slice cntrl, strings::Slice msg);

  void PullPendindDeadline(int64 event_id);
  void ReadErrorCallback();
  void DeadlineCallback(int64 id);
  void RespondWithDeadlineExceeded(int64 id);

  static void readcb(struct bufferevent *bev, void *ptr);
  static void eventcb(struct bufferevent* bev, short events, void *ptr);

  std::mutex mutex_;
  std::condition_variable connected_cv;


  Executor* executor_ = nullptr;
  Scheduler::handler_t alarmer_handler_id_;

  // TODO: consider using deque for outstanding_calls_. Since ids are monotonic,
  // it might be better for common case to actually them similarly to pending_deadlines_.
  // Moreover, those data structures could be merged.
  std::unordered_map<int64, OutstandingCall> outstanding_calls_ GUARDED_BY(mutex_);

  // this queue stores outstandine calls with monotonic times from steady clock.
  // since we only allow a constant timeout for the channel, it's possible to push those calls to
  // the back of the queue.
  std::deque<PendingDeadline> pending_deadlines_;
  std::unique_ptr<MessageReader> m_reader;
};

Channel::Rep::~Rep() {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    state = SHUTTING_DOWN;
    bev.reset();

    freeaddrinfo(serv_addr);
    if (alarmer_handler_id_ != Scheduler::INVALID_HANDLE) {
      Scheduler::Default().Remove(alarmer_handler_id_);
    }
    FlushOutstanding();
  }

  while (issued_callbacks_ > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void Channel::Rep::readcb(struct bufferevent* bev, void *ptr) {
  Channel::Rep* me = (Channel::Rep*)ptr;
  if (me->state.load() == SHUTTING_DOWN)
    return;
  struct evbuffer* input = bufferevent_get_input(bev);
  size_t len = evbuffer_get_length(input);
  VLOG(1) << "channel read_cb with " << len << " bytes";

  // Run in eventlib network thread.
  me->m_reader->ParseData(input);
}

void Channel::Rep::eventcb(struct bufferevent* bev, short events, void *ptr) {
  VLOG(1) << "eventcb:" << events;
  Channel::Rep* me = (Channel::Rep*)ptr;
  ChannelState state = me->state;
  if (state == SHUTTING_DOWN)
    return;
  bool reconnect = false;
  uint32 ms_delay = 0;
  if (events & BEV_EVENT_CONNECTED) {
    VLOG(1) << "client connected";
    me->state = CONNECTED;
    me->connected_cv.notify_all();
  } else if (events & BEV_EVENT_EOF) {
    VLOG(1) << "connection closed";
    reconnect = true;
  } else if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    VLOG(1) << "socket error " << err << " " << evutil_socket_error_to_string(err);
    reconnect = true;
    if (state == DISCONNECTED)
      ms_delay = 1000;
  }
  if (reconnect) {
    if (!me->state.compare_exchange_strong(state, DISCONNECTED)) {
      CHECK_EQ(state, SHUTTING_DOWN) << "only the d'tor expected to change the value";
      return;
    }
    VLOG(2) << "Schedule ConnectWithAddrInfo, this " << me;
    ++me->issued_callbacks_;
    auto task = std::bind(&Channel::Rep::ConnectWithAddrInfo, me, 1);
    if (ms_delay > 0) {
      VLOG(1) << "Reconnecting in 1 sec...";
      // TODO: to allow delayed non-periodic task execution in the executor.
       Scheduler::Default().Schedule(task, milliseconds(ms_delay), false);
    } else {
      me->executor_->Add(task);
    }
  }
}

void Channel::Rep::ConnectWithAddrInfo(int cause) {
  VLOG(2) << "ConnectWithAddrInfo " << cause << " this " << this;
  {
    std::lock_guard<std::mutex> lk(mutex_);
    --issued_callbacks_;
    if (state == SHUTTING_DOWN)
      return;
    FlushOutstanding();
    this->bev.reset(bufferevent_socket_new(executor_->ebase(), -1,
                                           BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE),
                    bufferevent_deleter);
    m_reader.reset(new MessageReader(std::bind(&Channel::Rep::ReplyHandler, this, _1, _2),
                                     std::bind(&Channel::Rep::ReadErrorCallback, this)));
    bufferevent_setcb(bev.get(), readcb, NULL, eventcb, this);
    int status = bufferevent_socket_connect(bev.get(), serv_addr->ai_addr, serv_addr->ai_addrlen);
    CHECK_EQ(status, 0) << evutil_socket_error_to_string(status);
    bufferevent_setwatermark(bev.get(), EV_READ, MessageReader::min_packet_size(), 0);
    bufferevent_enable(bev.get(), EV_READ);
  }
  /*if (tmp != nullptr) {
    bufferevent_free(tmp);
  }*/
}

bool Channel::Rep::WaitToConnect(uint32 milliseconds) {
  auto next = system_clock::now() + std::chrono::milliseconds(milliseconds);
  std::unique_lock<std::mutex> lk(mutex_);
  while (state.load() != CONNECTED) {
    std::cv_status status = connected_cv.wait_until(lk, next);
    if (status == std::cv_status::timeout)
      return false;
  }
  return true;
}

void Channel::Rep::AddRequest(
    const gpb::Message* request, const RpcControlRequest& rpc_info,
    const OutstandingCall& call, uint32 deadline) {
  int64 id = rpc_info.event_id();
  mutex_.lock();
  CHECK(bev) << "bev should be initialized when calling to AddRequest";
  std::shared_ptr<bufferevent> tmp(bev);
  if (deadline > 0) {
    auto now = steady_clock::now();
    uint64 msec = deadline + duration_cast<milliseconds>(now.time_since_epoch()).count();
    pending_deadlines_.push_back(PendingDeadline{msec, id});
    if (alarmer_handler_id_ == Scheduler::INVALID_HANDLE) {
      alarmer_handler_id_ = Scheduler::Default().Schedule(
          std::bind(&Channel::Rep::DeadlineCallback, this, id),
          milliseconds(deadline), false);
    }
  }
  outstanding_calls_.emplace(id, call);
  mutex_.unlock();
  WriteRpcPacket(rpc_info, request, bufferevent_get_output(tmp.get()));
}

void Channel::Rep::ReplyHandler(strings::Slice cntrl, strings::Slice msg) {
  if (state.load() == SHUTTING_DOWN)
    return;

  RpcControlResponse control_response;
  CHECK(control_response.ParseFromArray(cntrl.data(), cntrl.size()));
  VLOG(1) << control_response.ShortDebugString() << ",   payload size " << msg.size();

  int64 event_id = control_response.event_id();

  mutex_.lock();
  auto it = outstanding_calls_.find(event_id);
  if (it == outstanding_calls_.end()) {
    VLOG(1) << "Could not find outstanding call with id " << event_id;
    mutex_.unlock();
    return;
  }
  auto task = ClosureFunc(it->second.done);
  Context* context = it->second.context;
  gpb::Message* msg_response = it->second.response;
  outstanding_calls_.erase(it);
  PullPendindDeadline(event_id);
  mutex_.unlock();

  if (control_response.status().code() != Status::OK) {
    context->mutable_status()->Swap(control_response.mutable_status());
    executor_->Add(task);
    return;
  }
  CHECK(msg_response->ParseFromArray(msg.data(), msg.size()));
  VLOG(2) << "Received rpc " << msg_response->ShortDebugString();
  executor_->Add(task);
}

void Channel::Rep::PullPendindDeadline(int64 event_id) {
  if (pending_deadlines_.empty())
    return;
  int64 start_id = pending_deadlines_.front().event_id;
  if (start_id == event_id) {
    pending_deadlines_.front().set_responded();
    // Lets remove the first item as well as next ones that were responed before.
    // This could happen because responses can come out of order.
    while (!pending_deadlines_.empty() && pending_deadlines_.front().was_responded()) {
      pending_deadlines_.pop_front();
    }
  } else {
    int64 index = event_id - start_id;  // ids are monotonic and without holes.
    CHECK_GT(index, 0);
    CHECK_LT(index, pending_deadlines_.size());
    CHECK_EQ(pending_deadlines_[index].event_id, event_id);
    pending_deadlines_[index].set_responded();
  }
}

void Channel::Rep::ReadErrorCallback() {
  LOG(WARNING) << "Error reading data. Reestablishing the connection";
  ++issued_callbacks_;
  executor_->Add(std::bind(&Channel::Rep::ConnectWithAddrInfo, this, 2));
}

void Channel::Rep::DeadlineCallback(int64 id) {
  std::lock_guard<std::mutex> lk(mutex_);
  alarmer_handler_id_ = Scheduler::INVALID_HANDLE;
  if (pending_deadlines_.empty()) {  // no pending deadlines.
    DCHECK(outstanding_calls_.empty());
    return;
  }
  int64 start_id = pending_deadlines_.front().event_id;
  int64 index = id - start_id;
  if (index >= 0 && !pending_deadlines_[index].was_responded()) {
    pending_deadlines_[index].set_responded();
    RespondWithDeadlineExceeded(id);
  }
  auto now = steady_clock::now();
  uint64 msec_now = duration_cast<milliseconds>(now.time_since_epoch()).count();
  while (!pending_deadlines_.empty()) {
    const PendingDeadline& front = pending_deadlines_.front();
    if (front.was_responded()) {
      pending_deadlines_.pop_front();
      continue;
    }
    if (front.milliseconds_since_epoch <= msec_now) {
      RespondWithDeadlineExceeded(front.event_id);
      pending_deadlines_.pop_front();
      continue;
    }
    int64 next_deadline = front.milliseconds_since_epoch - msec_now;
    alarmer_handler_id_ = Scheduler::Default().Schedule(
        std::bind(&Channel::Rep::DeadlineCallback, this, front.event_id),
        milliseconds(next_deadline), false);
    break;
  }
}

void Channel::Rep::RespondWithDeadlineExceeded(int64 id) {
  VLOG(2) << "Deadline exceeded for event " << id;
  auto it = outstanding_calls_.find(id);
  CHECK(it != outstanding_calls_.end()) << "Id: " << id;
  it->second.context->SetError(Status::DEADLINE_EXCEEDED);
  auto done = it->second.done;
  outstanding_calls_.erase(it);
  executor_->Add(ClosureFunc(done));
}

// Channel implementation.
// Wraps Channel::Rep.
//
Channel::Channel(Executor* executor, StringPiece host_port)
    : rep_(new Rep(executor)), next_id_(10), deadline_(0) {
  size_t pos = host_port.find(':');
  CHECK_NE(pos, StringPiece::npos) << "Invalid host port " << host_port;
  host_ = StringPiece(host_port, 0, pos).as_string();
  host_port.remove_prefix(pos + 1);

  CHECK(safe_strto32(host_port, &port_)) << host_port;
  CHECK_GT(port_, 0) << host_port;
  resolve_host_pair(host_, port_, &rep_->serv_addr);

  rep_->issued_callbacks_ = 1;
  rep_->ConnectWithAddrInfo(3);
}

Channel::~Channel() {}

bool Channel::WaitToConnect(uint32 milliseconds) {
  return rep_->WaitToConnect(milliseconds);
}

void Channel::set_rpc_deadline(uint32 milliseconds) {
  CHECK_GT(milliseconds, 0);

  CHECK_EQ(deadline_.exchange(milliseconds), 0) << "set_rpc_deadline can be called at most once";
}


void Channel::CallMethod(const gpb::MethodDescriptor* method,
                         gpb::RpcController* controller,
                         const gpb::Message* request,
                         gpb::Message* response,
                         gpb::Closure* done) {
  ChannelState st = rep_->state.load();
  Context* context = reinterpret_cast<Context*>(CHECK_NOTNULL(controller));
  VLOG(2) << "Channel::CallMethod " << st;
  if (st == DISCONNECTED) {
    context->SetError(Status::CONNECTION_REFUSED);
    done->Run();
    return;
  }

  int64 id = next_id_.fetch_add(1);
  RpcControlRequest rpc_info;
  rpc_info.set_event_id(id);
  rpc_info.set_method_full_name(method->full_name());
  rep_->AddRequest(request, rpc_info, OutstandingCall{context, response, CHECK_NOTNULL(done)},
                   deadline_.load());
}

}  // namespace rpc
}  // namespace util
