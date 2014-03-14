// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef VARZ_STATS_H
#define VARZ_STATS_H

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <string>
#include "base/integral_types.h"
#include "strings/stringpiece.h"
#include "strings/unique_strings.h"
#include "util/stats/sliding_counter.h"

namespace http {

class VarzListNode {
public:
  explicit VarzListNode(const char* name);
  virtual ~VarzListNode();

  // Appends string representations of each active node in the list to res.
  // Used for outputting the current state.
  static void IterateValues(std::function<void(const std::string&, const std::string&)> cb);
protected:
  virtual std::string PrintHTML() const = 0;
private:
  // Returns the head to varz linked list. Note that the list becomes invalid after at least one
  // linked list node was destroyed.
  static VarzListNode* & global_list();


  const char* name_;
  VarzListNode* next_;
  VarzListNode* prev_;
};

/**
  Represents a family (map) of counters. Each counter has its own key name.
**/
class VarzMapCount : public VarzListNode {
public:
  explicit VarzMapCount(const char* varname) : VarzListNode(varname) {}

  // Increments key by delta.
  void IncBy(StringPiece key, int32 delta);

  void Inc(StringPiece key) { IncBy(key, 1); }

private:
  std::string PrintHTML() const override;

  mutable std::mutex mutex_;
  StringPieceMap<long> map_counts_;
};

// represents a family of averages.
class VarzMapAverage : public VarzListNode {
public:
  explicit VarzMapAverage(const char* varname) : VarzListNode(varname) {}

  void IncBy(const string& key, double delta);

private:
  string PrintHTML() const override;

  mutable std::mutex mutex_;

  std::unordered_map<string, std::pair<double, unsigned long>> map_;
};

class VarzCount : public VarzListNode {
public:
  explicit VarzCount(const char* varname) : VarzListNode(varname) {}

  void IncBy(int32 delta) { val_ += delta; }
  void Inc() { IncBy(1); }

private:
  string PrintHTML() const override;
  std::atomic_long val_;
};

class VarzQps : public VarzListNode {
public:
  explicit VarzQps(const char* varname) : VarzListNode(varname) {}

  void Inc() { val_.Inc(); }

private:
  string PrintHTML() const override;

  mutable util::QPSCount val_;
};

class VarzFunction : public VarzListNode {
public:
  explicit VarzFunction(const char* varname, std::function<string()> cb)
    : VarzListNode(varname), cb_(cb) {}

private:
  string PrintHTML() const override { return cb_(); }

  std::function<string()> cb_;
};

}  // namespace http

#endif  // VARZ_STATS_H