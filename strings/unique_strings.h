// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef UNIQUE_STRINGS_H
#define UNIQUE_STRINGS_H

#include <unordered_map>
#include <unordered_set>
#include "base/arena.h"
#include "base/counting_allocator.h"
#include "strings/stringpiece.h"
#include "strings/hash.h"

class UniqueStrings {
public:
  typedef std::unordered_set<
      StringPiece, std::hash<StringPiece>,
      std::equal_to<StringPiece>, base::counting_allocator<StringPiece>> SSet;
  typedef SSet::const_iterator const_iterator;

  StringPiece Get(StringPiece source) {
    return Insert(source).first;
  }

  // returns true if insert took place or false if source was already present.
  // In any case returns the StringPiece from the set.
  std::pair<StringPiece, bool> Insert(StringPiece source);

  size_t MemoryUsage() const {
    return arena_.MemoryUsage() +  db_.get_allocator().bytes_allocated(); // db_.size()*sizeof(StringPiece);
  }

  const_iterator begin() { return db_.begin(); }
  const_iterator end() { return db_.end(); }

private:
  base::Arena arena_;
  SSet db_;
};

inline std::pair<StringPiece, bool> UniqueStrings::Insert(StringPiece source) {
  auto it = db_.find(source);
  if (it != db_.end())
    return std::make_pair(*it, false);
  if (source.empty()) {
    auto res = db_.insert(StringPiece());
    return std::make_pair(*res.first, res.second);
  }
  char* str = arena_.Allocate(source.size());
  memcpy(str, source.data(), source.size());
  StringPiece val(str, source.size());
  db_.insert(val);
  return std::make_pair(val, true);
}

template<typename T> class StringPieceMap {
  typedef std::unordered_map<StringPiece, T> SMap;
public:
  typedef typename SMap::iterator iterator;
  typedef typename SMap::const_iterator const_iterator;
  typedef typename SMap::value_type value_type;

  std::pair<iterator,bool> insert(const value_type& val) {
    auto it = map_.find(val.first);
    if (it != map_.end())
      return std::make_pair(it, false);
    if (val.first.empty()) {
      it = map_.emplace(StringPiece(), val.second).first;
    } else {
      char* str = arena_.Allocate(val.first.size());
      memcpy(str, val.first.data(), val.first.size());
      StringPiece pc(str, val.first.size());
      it = map_.emplace(pc, val.second).first;
    }
    return std::make_pair(it, true);
  }

  std::pair<iterator,bool> emplace(StringPiece key, T&& val) {
    auto it = map_.find(key);
    if (it != map_.end())
      return std::make_pair(it, false);
    if (key.empty()) {
      it = map_.emplace(StringPiece(), val).first;
    } else {
      char* str = arena_.Allocate(key.size());
      memcpy(str, key.data(), key.size());
      StringPiece pc(str, key.size());
      it = map_.emplace(pc, val).first;
    }
    return std::make_pair(it, true);
  }

  T& operator[](StringPiece val) {
    auto res = emplace(val, T());
    return res.first->second;
  }

  iterator begin() { return map_.begin(); }
  const_iterator begin() const { return map_.begin(); }

  iterator end() { return map_.end(); }
  const_iterator end() const { return map_.end(); }

  iterator find(StringPiece id) { return map_.find(id); }
  const_iterator find(StringPiece id) const { return map_.find(id); }

  void swap(StringPieceMap& other) {
    map_.swap(other.map_);
    arena_.Swap(other.arena_);
  }
  size_t MemoryUsage() const {
    return arena_.MemoryUsage() + map_.size()*(sizeof(StringPiece) + sizeof(T));
  }

  size_t size() const {
    return map_.size();
  }
private:
  base::Arena arena_;
  std::unordered_map<StringPiece, T> map_;
};

#endif  // UNIQUE_STRINGS_H