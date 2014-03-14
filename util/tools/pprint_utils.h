// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _PPRINT_UTILS_H
#define _PPRINT_UTILS_H

#include <functional>
#include <vector>
#include <string>
#include <google/protobuf/text_format.h>

#include "base/integral_types.h"
#include "strings/stringpiece.h"

namespace util {
namespace pprint {

namespace gpb = ::google::protobuf;

class FdPath {
public:
  // msg, fd, index in repeated array.
  typedef std::function<void(const gpb::Message& msg, const gpb::FieldDescriptor*, int)> ValueCb;
  FdPath() {}
  FdPath(const gpb::Descriptor* root, StringPiece path);
  FdPath(const FdPath&) = default;
  FdPath(FdPath&&) = default;
  FdPath& operator=(const FdPath&) = default;

  void ExtractValue(const gpb::Message& msg, ValueCb cb) {
    ExtractValueRecur(msg, 0, cb);
  }

  bool IsRepeated() const;

  const std::vector<const gpb::FieldDescriptor*> path() const { return path_; }
  void push_back(const gpb::FieldDescriptor* fd) { path_.push_back(fd); }

private:
  void ExtractValueRecur(const gpb::Message& msg, uint32 index, ValueCb cb);

  std::vector<const gpb::FieldDescriptor*> path_;
  unsigned cur_repeated_depth_ = 0;
  std::vector<uint32> cur_repeated_stack_; // indices into the arrays of repeated messages.
};


// Allocated a message instance based on the descriptor.
gpb::Message* AllocateMsgFromDescr(const gpb::Descriptor* descr);

// Allocates message dynamically based on its type and serialized fs_set data.
// Both fields must be well defined and set.
gpb::Message* AllocateMsgByMeta(const string& type, const string& fd_set);

struct PathNode {
  std::vector<PathNode> children;
  const gpb::FieldDescriptor* fd;

  explicit PathNode(const gpb::FieldDescriptor* fdescr = nullptr) : fd(fdescr) {}

  // FdPath CheckForRepeated() const;

  PathNode* AddChild(const gpb::FieldDescriptor* fd);
};

class Printer {
  gpb::TextFormat::Printer printer_;
  std::string type_name_;
  std::vector<FdPath> fds_;
  PathNode root_;
  // FdPath repeated_path_;


  // void ExtractValueRecur(const gpb::Message& msg, const FdPath& fd_path, uint32 index, ValueCb cb);
  void PrintValueRecur(size_t path_index, const std::string& prefix,
                       bool has_value, const gpb::Message& msg);
public:
  explicit Printer(const gpb::Descriptor* descriptor);
  void Output(const gpb::Message& msg);
};

}  // namespace pprint
}  // namespace util


#endif  // _PPRINT_UTILS_H