// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <iostream>
#include "base/googleinit.h"
#include "file/list_file.h"
#include "strings/stringpiece.h"
#include <google/protobuf/compiler/importer.h>

DEFINE_string(root_dir, "./", "");
DEFINE_string(db_file, "./proto_db.lst", "Protos db file");

namespace gpb = ::google::protobuf;
namespace gpc = gpb::compiler;

class ErrorCollector : public gpc::MultiFileErrorCollector {
  void AddError(const string& filenname, int line,int column, const string& message) {
    std::cerr << "Error File : " << filenname << " : " << message << std::endl;
  }
};

using std::cout;
using std::endl;
int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);

  if (argc < 2)
    return 0;
  char* root_dir = realpath(FLAGS_root_dir.c_str(), NULL);
  gpc::DiskSourceTree tree;
  tree.MapPath("", root_dir);
  ErrorCollector collector;
  gpc::Importer importer(&tree, &collector);

  std::vector<gpb::FileDescriptorProto> protos;
  StringPiece root(root_dir);
  cout << "Root dir is " << root_dir << endl;
  for (int i = 1; i < argc; ++i) {
    char* file_name = realpath(argv[i], NULL);
    StringPiece fname(file_name);
    if (fname.starts_with(root)) {
      fname.remove_prefix(root.size() + 1);
      cout << "Importing " << fname << endl;
      const gpb::FileDescriptor* fd = importer.Import(fname.as_string());
      CHECK(fd != NULL) << fname;
      gpb::FileDescriptorProto p;
      fd->CopyTo(&p);
      protos.push_back(std::move(p));
    } else {
      cout << "Skipping " << fname << " because it's not under " << root_dir << endl;
    }
    free(file_name);
  }
  free(root_dir);
  file::ListWriter writer(FLAGS_db_file);
  CHECK(writer.Init().ok());
  for (const auto& p : protos) {
    CHECK(writer.AddRecord(p.SerializeAsString()).ok());
  }
  std::cout << "Saved " << protos.size() << " proto files.\n";
  CHECK(writer.Flush().ok());
  return 0;
}