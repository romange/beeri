// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "util/http/varz_stats.h"

#include "strings/strcat.h"
#include "strings/stringprintf.h"

using std::string;

namespace http {

typedef std::lock_guard<std::mutex> mguard;

static std::mutex g_varz_mutex;

static string CountToHTML(long count) {
  string res;
  StrAppend(&res, "<span class='value_text'> ", count, " </span>\n");
  return res;
}

VarzListNode::VarzListNode(const char* name)
  : name_(name), prev_(nullptr) {
  mguard guard(g_varz_mutex);
  next_ = global_list();
  if (next_) {
    next_->prev_ = this;
  }
  global_list() = this;
}

VarzListNode::~VarzListNode() {
  mguard guard(g_varz_mutex);
  if (global_list() == this) {
    global_list() = next_;
  } else {
    if (next_) {
      next_->prev_ = prev_;
    }
    if (prev_) {
      prev_->next_ = next_;
    }
  }
}

VarzListNode* & VarzListNode::global_list() {
  static VarzListNode* varz_global_list = nullptr;
  return varz_global_list;
}

void VarzListNode::IterateValues(
  std::function<void(const std::string&, const std::string&)> cb) {
  mguard guard(g_varz_mutex);
  for (VarzListNode* node = global_list(); node != nullptr; node = node->next_) {
    if (node->name_ != nullptr) {
      cb(node->name_, node->PrintHTML());
    }
  }
}

void VarzMapCount::IncBy(StringPiece key, int32 delta) {
  std::lock_guard<std::mutex> lock(mutex_);
  map_counts_[key] += delta;
}

std::string KeyValueWithStyle(StringPiece key, StringPiece val) {
  string res("<span class='key_text'>");
  StrAppend(&res, key, ":</span><span class='value_text'>", val, "</span>\n");
  return res;
}

string VarzMapCount::PrintHTML() const {
  std::lock_guard<std::mutex> lock(mutex_);
  string result;

  for (const auto& k_v : map_counts_) {
    StrAppend(&result, KeyValueWithStyle(k_v.first, SimpleItoa(k_v.second)));
  }
  return result;
}

string VarzMapAverage::PrintHTML() const {
  std::lock_guard<std::mutex> lock(mutex_);
  string result;

  for (const auto& k_v : map_) {
    string val;
    if (k_v.second.second > 0)
      val = StringPrintf("%.3f", k_v.second.first / k_v.second.second);

    StrAppend(&result, k_v.first, ": { ",
              KeyValueWithStyle("count", SimpleItoa(k_v.second.second)),
              KeyValueWithStyle("sum", StringPrintf("%.3f", k_v.second.first)),
              KeyValueWithStyle("average", val), "} ");
  }
  return result;
}

void VarzMapAverage::IncBy(const string& key, double delta) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& val = map_[key];
  val.first += delta;
  ++val.second;
}

std::string VarzCount::PrintHTML() const {
  return CountToHTML(val_.load());
}

std::string VarzQps::PrintHTML() const {
  return CountToHTML(val_.Get());
}

}  // namespace http