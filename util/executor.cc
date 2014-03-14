// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/executor.h"

#include <atomic>
#include <event2/event.h>
#include <event2/thread.h>
#include <pthread.h>
#include <signal.h>

#include "base/logging.h"
#include "base/sync_queue.h"
#include "util/proc_stats.h"

#define PTHREAD_CALL(x) \
  do { \
    int my_err = pthread_ ## x; \
    CHECK_EQ(0, my_err) << strerror(my_err); \
  } while(false)

static constexpr int kThreadStackSize = 65536;

namespace util {

static pthread_once_t eventlib_init_once = PTHREAD_ONCE_INIT;
static Executor* signal_executor_instance = nullptr;

static void InitExecutorModule() {
  CHECK_EQ(0, evthread_use_pthreads());
}

static void ExecutorSigHandler(int sig, siginfo_t *info, void *secret) {
  LOG(INFO) << "Catched signal " << sig << ": " << strsignal(sig);
  if (signal_executor_instance) {
    signal_executor_instance->Shutdown();
  }
}

class Executor::Rep {
  event_base* base_ = nullptr;

  base::sync_queue<std::function<void()>> tasks_queue_;
  std::vector<pthread_t> pool_threads_;
  pthread_t event_loop_thread_;
  pthread_cond_t shut_down_cond_ = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  bool shut_down_;

  std::atomic_bool start_cancel_;  // signals worker threads that they should stop running.
  uint32 poolthreads_finished_count_;  // number of worker threads that finished their run.

  // signals each time a worker thread finished.
  pthread_cond_t finished_pool_threads_ = PTHREAD_COND_INITIALIZER;

  static void* RunEventBase(void* me);
  static void* RunPoolThread(void* me);
public:
  Rep() {
    shut_down_ = false;
    start_cancel_ = false;
    poolthreads_finished_count_ = 0;

    base_ = CHECK_NOTNULL(event_base_new());
    pthread_attr_t attrs;
    PTHREAD_CALL(attr_init(&attrs));
    PTHREAD_CALL(attr_setstacksize(&attrs, kThreadStackSize));
    PTHREAD_CALL(create(&event_loop_thread_, &attrs,  Executor::Rep::RunEventBase, this));
    PTHREAD_CALL(setname_np(event_loop_thread_, "EventBaseThd"));
    PTHREAD_CALL(attr_destroy(&attrs));
  }

  ~Rep() {
    StartCancel();
    WaitShutdown();
    event_base_free(base_);
  }

  event_base* base() { return base_; }

  void StartCancel() {
    start_cancel_ = true;
    event_base_loopexit(base_, NULL); // signal to exit.
  }

  bool was_cancelled() const { return start_cancel_; }

  void SetupThreadPool(unsigned num_threads) {
    CHECK(pool_threads_.empty());
    CHECK_GT(num_threads, 0);
    pool_threads_.resize(num_threads);

    char buf[30] = {0};
    pthread_attr_t attrs;
    PTHREAD_CALL(attr_init(&attrs));
    PTHREAD_CALL(attr_setstacksize(&attrs, kThreadStackSize));

    for (unsigned i = 0; i < num_threads; ++i) {
      PTHREAD_CALL(create(&pool_threads_[i],  &attrs,  Executor::Rep::RunPoolThread, this));
      sprintf(buf, "ExecPool_%d", i);
      PTHREAD_CALL(setname_np(pool_threads_[i], buf));
    }
    PTHREAD_CALL(attr_destroy(&attrs));
  }

  void WaitShutdown() {
    PTHREAD_CALL(mutex_lock(&mutex_));
    // We do not use pthread_join because it can not be used from multiple threads.
    // Here we allow the flexibility for several threads to wait for the loop to exit.
    while (!shut_down_) {
      PTHREAD_CALL(cond_wait(&shut_down_cond_, &mutex_));
    }

    while (poolthreads_finished_count_ < pool_threads_.size()) {
      PTHREAD_CALL(cond_wait(&finished_pool_threads_, &mutex_));
    }
    PTHREAD_CALL(mutex_unlock(&mutex_));
  }

  void Add(std::function<void()> f) {
    if (was_cancelled())
      return;
    tasks_queue_.push(f);
  }
};

void* Executor::Rep::RunEventBase(void* arg) {
  Executor::Rep* me = (Executor::Rep*)arg;

  int res;
  while ((res = event_base_dispatch(me->base_)) == 1) {
    pthread_yield();
  }

  VLOG(1) << "Finished running event_base_dispatch with res: " << res;
  PTHREAD_CALL(mutex_lock(&me->mutex_));
  me->shut_down_ = true;
  PTHREAD_CALL(cond_broadcast(&me->shut_down_cond_));
  PTHREAD_CALL(mutex_unlock(&me->mutex_));

  return NULL;
}

void* Executor::Rep::RunPoolThread(void* arg) {
  Executor::Rep* me = (Executor::Rep*)arg;
  while (!me->start_cancel_) {
    std::function<void()> val;
    bool res = me->tasks_queue_.pop(5, &val);
    if (res) val();
  }
  char buf[30] = {0};
  pthread_getname_np(pthread_self(), buf, sizeof buf);
  VLOG(1) << "Finished running ThreadPool thread " << buf << " with " << me->tasks_queue_.size();
  PTHREAD_CALL(mutex_lock(&me->mutex_));
  ++me->poolthreads_finished_count_;
  PTHREAD_CALL(cond_broadcast(&me->finished_pool_threads_));
  PTHREAD_CALL(mutex_unlock(&me->mutex_));

  return NULL;
}


Executor::Executor(unsigned int num_threads) {
  pthread_once(&eventlib_init_once, InitExecutorModule);
  rep_.reset(new Rep());

  if (num_threads == 0) {
    uint32 num_cpus = sys::NumCPUs();
    if (num_cpus == 0)
      num_threads = 2;
    else
      num_threads = num_cpus * 2;
  }
  rep_->SetupThreadPool(num_threads);
}

Executor::~Executor() {

}

event_base* Executor::ebase() {
  return rep_->base();
}


void Executor::Add(std::function<void()> f) {
  rep_->Add(f);
}

void Executor::Shutdown() {
  rep_->StartCancel();
}

void Executor::WaitForLoopToExit() {
  rep_->WaitShutdown();
}

void Executor::StopOnTermSignal() {
  signal_executor_instance = this;

  struct sigaction sa;
  sa.sa_sigaction = ExecutorSigHandler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

Executor& Executor::Default() {
  static Executor executor;
  return executor;
}

}  // namespace util