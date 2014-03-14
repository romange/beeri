// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <iostream>
#include "base/googleinit.h"
#include "file/list_file.h"
#include "file/proto_writer.h"
#include "strings/slice.h"
#include "util/map-util.h"
#include "util/tools/pprint_utils.h"

using namespace std;
using namespace util::pprint;
using strings::Slice;

DEFINE_string(input, "", "input lst file");
DEFINE_string(output, "", "output sst file");
DEFINE_string(key, "", "period delimited list of tag numbers describing tag path. "
                        "Must not be repeated path.");

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);
  CHECK(!FLAGS_input.empty());
  CHECK(!FLAGS_output.empty());
  CHECK(!FLAGS_key.empty());

  file::ListReader reader(FLAGS_input);
  std::map<std::string, std::string> meta;

  CHECK(reader.GetMetaData(&meta)) << "Failed to read meta data";
  string ptype = FindOrDie(meta, file::kProtoTypeKey);
  string fd_set = FindOrDie(meta, file::kProtoSetKey);
  std::unique_ptr<gpb::Message> tmp_msg(AllocateMsgByMeta(ptype, fd_set));
  FdPath fd_path(tmp_msg->GetDescriptor(), FLAGS_key);
  CHECK(!fd_path.IsRepeated());

  string record_buf;
  Slice record;

  gpb::TextFormat::Printer printer;
  printer.SetUseUtf8StringEscaping(true);

  file::ProtoWriter::Options options;
  options.format = file::ProtoWriter::SSTABLE;
  file::ProtoWriter writer(FLAGS_output, tmp_msg->GetDescriptor(), options);

  auto cb_fun = [&writer, &printer, &tmp_msg](
    const gpb::Message& parent, const gpb::FieldDescriptor* fd, int item_index) {
    CHECK(!fd->is_repeated());
    const gpb::Reflection* reflection = parent.GetReflection();
    CHECK(reflection->HasField(parent, fd));
    string key;
    printer.PrintFieldValueToString(parent, fd, item_index, &key);
    CHECK(writer.Add(key, *tmp_msg).ok());
  };

  while (reader.ReadRecord(&record, &record_buf)) {
    CHECK(tmp_msg->ParseFromArray(record.data(), record.size()));
    fd_path.ExtractValue(*tmp_msg, cb_fun);
  }
  CHECK(writer.Flush().ok());
  return 0;
}