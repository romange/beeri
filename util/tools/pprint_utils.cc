// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/tools/pprint_utils.h"

#include <iostream>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>

#include "base/logging.h"
#include "strings/numbers.h"
#include "strings/split.h"
#include "strings/strcat.h"

DEFINE_bool(short, false, "");
DEFINE_string(csv, "", "comma delimited list of tag numbers. For repeated fields, it's possible "
                       "to add :[delimiting char] after a tag number.");
DEFINE_bool(use_csv_null, true, "When printing csv format use \\N for outputing undefined "
                                "optional fields.");

using std::cout;

namespace util {
namespace pprint {

FdPath::FdPath(const gpb::Descriptor* root, StringPiece path) {
  std::vector<StringPiece> parts = strings::Split(path, ".");
  CHECK(!parts.empty()) << path;
  const gpb::Descriptor* cur_descr = root;
  for (size_t j = 0; j < parts.size(); ++j) {
    uint32 tag_id = ParseLeadingUInt32Value(parts[j], 0);
    const gpb::FieldDescriptor* field = cur_descr->FindFieldByNumber(tag_id);
    CHECK(field) << "Can not find tag id " << parts[j];
    if (j + 1 < parts.size()) {
      CHECK_EQ(field->cpp_type(), gpb::FieldDescriptor::CPPTYPE_MESSAGE);
      cur_descr = field->message_type();
    } else {
      CHECK_NE(field->cpp_type(), gpb::FieldDescriptor::CPPTYPE_MESSAGE)
        << "Invalid leaf tag" << path;
    }
    path_.push_back(field);
  }
}

bool FdPath::IsRepeated() const {
  for (auto v : path_) {
    if (v->is_repeated())
      return true;
  }
  return false;
}


void FdPath::ExtractValueRecur(const gpb::Message& msg, uint32 index, ValueCb cb) {
  CHECK_LT(index, path_.size());
  auto fd = path_[index];
  const gpb::Reflection* reflection = msg.GetReflection();
  uint32 cur_repeated_depth = 0;
  for (uint32 i = 0; i < index; ++i) {
    if (path_[i]->is_repeated()) ++cur_repeated_depth;
  }
  if (fd->is_repeated()) {
    int sz = reflection->FieldSize(msg, fd);
    if (sz > 0) {
      if (index + 1 < path_.size()) {
        // Non leaves, repeated messages.
        if (cur_repeated_depth < cur_repeated_stack_.size()) {
          const gpb::Message& new_msg =
              reflection->GetRepeatedMessage(msg, fd, cur_repeated_stack_[cur_repeated_depth]);
          ExtractValueRecur(new_msg, index + 1, cb);
        } else {
          for (int i = 0; i < sz; ++i) {
            cur_repeated_stack_.push_back(i);
            const gpb::Message& new_msg = reflection->GetRepeatedMessage(msg, fd, i);
            ExtractValueRecur(new_msg, index + 1, cb);
            cur_repeated_stack_.pop_back();
          }
        }

      } else {
        // Repeated leaves.
        string val;
        for (int i = 0; i < sz; ++i) {
          cb(msg, fd, i);
        }
      }
    }
    return;
  }

  if (index + 1 < path_.size()) {
    const gpb::Message& new_msg = reflection->GetMessage(msg, fd);
    ExtractValueRecur(new_msg, index + 1, cb);
    return;
  }
  /*if (FLAGS_use_csv_null && !reflection->HasField(msg, fd)) {
    cb("\\N");
    return;
  }
  string res;
  printer_.PrintFieldValueToString(msg, fd, -1, &res);*/
  cb(msg, fd, -1);
}

gpb::Message* AllocateMsgByMeta(const string& type, const string& fd_set) {
  CHECK(!type.empty());
  CHECK(!fd_set.empty());

  static gpb::SimpleDescriptorDatabase proto_db;
  static gpb::DescriptorPool proto_db_pool(&proto_db);
  const gpb::Descriptor* descriptor = proto_db_pool.FindMessageTypeByName(type);
  if (!descriptor) {
    gpb::FileDescriptorSet fd_set_proto;
    CHECK(fd_set_proto.ParseFromString(fd_set));
    for (int i = 0; i < fd_set_proto.file_size(); ++i) {
      proto_db.Add(fd_set_proto.file(i));
    }
    descriptor = proto_db_pool.FindMessageTypeByName(type);
  }

  CHECK(descriptor) << "Can not find " << type << " in the proto pool.";
  return AllocateMsgFromDescr(descriptor);
}

gpb::Message* AllocateMsgFromDescr(const gpb::Descriptor* descr) {
  static gpb::DynamicMessageFactory message_factory;

  const gpb::Message* msg_proto = message_factory.GetPrototype(descr);
  CHECK_NOTNULL(msg_proto);
  return msg_proto->New();
}

PathNode* PathNode::AddChild(const gpb::FieldDescriptor* fd) {
  for (PathNode& n : children) {
    if (n.fd == fd) return &n;
  }
  children.push_back(PathNode(fd));
  return &children.back();
}

Printer::Printer(const gpb::Descriptor* descriptor): type_name_(descriptor->full_name()) {
  printer_.SetUseShortRepeatedPrimitives(true);
  printer_.SetUseUtf8StringEscaping(true);
  std::vector<StringPiece> tags = strings::Split(FLAGS_csv, ",", strings::SkipWhitespace());
  if (tags.empty()) {
    printer_.SetInitialIndentLevel(1);
    printer_.SetSingleLineMode(FLAGS_short);
  } else {
    for (StringPiece tag_path : tags) {
      FdPath fd_path(descriptor, tag_path);
      PathNode* cur_node = &root_;
      for (const gpb::FieldDescriptor* fd: fd_path.path()) {
        cur_node = cur_node->AddChild(fd);
      }
      fds_.push_back(std::move(fd_path));
    }
  }
}

void Printer::Output(const gpb::Message& msg) {
  string text_output;
  if (fds_.empty()) {
    CHECK(printer_.PrintToString(msg, &text_output));
    std::cout << type_name_ << " {" << (FLAGS_short ? " " : "\n")
              << text_output << "}\n";
  } else {
    PrintValueRecur(0, "", false, msg);
  }
}

void Printer::PrintValueRecur(size_t path_index, const string& prefix,
                              bool has_value, const gpb::Message& msg) {
  CHECK_LT(path_index, fds_.size());
  auto cb_fun = [path_index, this, has_value, &prefix, &msg](
    const gpb::Message& parent, const gpb::FieldDescriptor* fd, int item_index) {
    string val;
    printer_.PrintFieldValueToString(parent, fd, item_index, &val);
    if (item_index == -1) {
      const gpb::Reflection* reflection = parent.GetReflection();
      if (FLAGS_use_csv_null && !reflection->HasField(parent, fd)) {
        val = "\\N";
      }
    }

    string next_val = (path_index == 0) ? val : StrCat(prefix, ",", val);
    bool next_has_value = has_value | !val.empty();
    if (path_index + 1 == fds_.size()) {
      if (next_has_value)
        cout << next_val << std::endl;
    } else {
      PrintValueRecur(path_index + 1, next_val, next_has_value, msg);
    }
  };
  fds_[path_index].ExtractValue(msg, cb_fun);
}

}  // namespace pprint
}  // namespace util
