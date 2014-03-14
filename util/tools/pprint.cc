// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <sys/stat.h>
#include <leveldb/db.h>
#include "base/googleinit.h"
#include "file/list_file.h"
#include "file/sstable/sstable.h"
#include "file/proto_writer.h"
#include "strings/escaping.h"
#include "util/lmdb/disk_table.h"
#include "util/plang/plang_parser.h"
#include "util/tools/pprint_utils.h"
#include "util/map-util.h"

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>

DEFINE_string(protofiles, "", "");
DEFINE_string(type, "", "");
DEFINE_bool(ldb, false, "");
DEFINE_string(where, "", "boolean constraint in plang language");

using namespace util::pprint;
using strings::Slice;
namespace gpc = gpb::compiler;

class ErrorCollector : public gpc::MultiFileErrorCollector {
  void AddError(const string& filenname, int line,int column, const string& message) {
    std::cerr << "Error File : " << filenname << " : " << message << std::endl;
  }
};

static const gpb::Descriptor* FindDescriptor() {
  CHECK(!FLAGS_type.empty()) << "type must be filled. For example: --type=foursquare.Category";
  const gpb::DescriptorPool* gen_pool = gpb::DescriptorPool::generated_pool();
  const gpb::Descriptor* descriptor = gen_pool->FindMessageTypeByName(FLAGS_type);
  if (descriptor)
    return descriptor;

  gpc::DiskSourceTree tree;
  tree.MapPath("START_FILE", FLAGS_protofiles);
  ErrorCollector collector;
  gpc::Importer importer(&tree, &collector);
  if (!FLAGS_protofiles.empty()) {
    // TODO: to support multiple files some day.
    CHECK(importer.Import("START_FILE"));
  }
  descriptor = importer.pool()->FindMessageTypeByName(FLAGS_type);
  return descriptor;
}

using namespace file;
int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);

  std::unique_ptr<plang::Expr> test_expr;
  if (!FLAGS_where.empty()) {
    std::istringstream istr(FLAGS_where);
    std::ostringstream ostr;
    plang::Parser parser(istr, ostr);
    CHECK_EQ(0, parser.parse()) << "Could not parse " << FLAGS_where;
    test_expr = std::move(parser.res_val);
  }

  // const Reflection* reflection = msg->GetReflection();
  for (int i = 1; i < argc; ++i) {
    StringPiece path(argv[i]);
    VLOG(1) << "Opening " << path;

    string ptype;
    string fd_set;
    std::unique_ptr<gpb::Message> tmp_msg;
    if (FLAGS_ldb) {
      leveldb::DB* db = nullptr;
      leveldb::Options options;
      auto status = leveldb::DB::Open(options, path.as_string(), &db);
      CHECK(status.ok()) << status.ToString();
      leveldb::ReadOptions read_opts;
      read_opts.fill_cache = false;
      std::unique_ptr<leveldb::Iterator> iter(db->NewIterator(read_opts));
      iter->SeekToFirst();
      tmp_msg.reset(AllocateMsgFromDescr(FindDescriptor()));
      Printer printer(tmp_msg->GetDescriptor());

      while(iter->Valid()) {
        auto val = iter->value();
        CHECK(tmp_msg->ParseFromArray(val.data(), val.size()))
          << "string size: " << val.size() << ", key: " << iter->key().ToString();
        if (test_expr && !plang::EvaluateBoolExpr(*test_expr, *tmp_msg))
          continue;
        printer.Output(*tmp_msg);
        iter->Next();
      }
      iter.reset();
      delete db;
      VLOG(1) << "Finished reading";
      return 0;
    }
    bool is_mdb = path.ends_with(".mdb");
    if (is_mdb) {
      struct stat stats;
      is_mdb = (stat(path.data(), &stats)) == 0 && S_ISREG(stats.st_mode);
    }
    if (is_mdb) {
      size_t pos = path.rfind('/');
      StringPiece folder = path.substr(0, pos);
      std::cout << folder << "\n";
      util::DiskTable table(20, true);
      CHECK(table.Open(folder).ok());
      CHECK(table.BeginTransaction(true).ok());
      Slice key, val;
      if (table.Get(Slice(file::kProtoTypeKey), &val)) {
        ptype = val.as_string();
      }
      if (table.Get(Slice(file::kProtoSetKey), &val)) {
        fd_set = val.as_string();
      }
      if (!ptype.empty() && !fd_set.empty()) {
        tmp_msg.reset(AllocateMsgByMeta(ptype, fd_set));
      } else {
        tmp_msg.reset(AllocateMsgFromDescr(FindDescriptor()));
      }
      Printer printer(tmp_msg->GetDescriptor());
      auto it = table.GetIterator();

      while (it.Next(&key, &val)) {
        CHECK(tmp_msg->ParseFromArray(val.data(), val.size()))
          << "string size: " << val.size() << ", string: " << strings::CHexEscape(val);
        if (test_expr && !plang::EvaluateBoolExpr(*test_expr, *tmp_msg))
          continue;
        // printer.Output(*tmp_msg);
      }
      table.AbortTransaction();
    } else if (path.ends_with(".sst")) {
      auto res = file::ReadonlyFile::Open(path);
      CHECK(res.status.ok()) << res.status;
      std::unique_ptr<file::ReadonlyFile> file(res.obj);
      auto res2 = sstable::Table::Open(sstable::ReadOptions(), file.get());
      CHECK(res2.status.ok()) << res2.status.ToString();
      std::unique_ptr<sstable::Table> table(res2.obj);
      std::unique_ptr<sstable::Iterator> it(table->NewIterator());
      const auto& meta = table->GetMeta();
      ptype = FindWithDefault(meta, file::kProtoTypeKey, "");
      fd_set = FindWithDefault(meta, file::kProtoSetKey, "");
      if (!ptype.empty() && !fd_set.empty()) {
        tmp_msg.reset(AllocateMsgByMeta(ptype, fd_set));
      } else {
        tmp_msg.reset(AllocateMsgFromDescr(FindDescriptor()));
      }
      Printer printer(tmp_msg->GetDescriptor());
      for (it->SeekToFirst(); it->Valid(); it->Next()) {
        CHECK(tmp_msg->ParseFromArray(it->value().data(), it->value().size()));
        if (test_expr && !plang::EvaluateBoolExpr(*test_expr, *tmp_msg))
          continue;
        std::cout << strings::CHexEscape(it->key()) << " : ";
        printer.Output(*tmp_msg);
        // std::cout << it->key().data() << ": " << it->value().size() << std::endl;
      }
      CHECK(file->Close().ok());
    } else {
      file::ListReader reader(argv[i]);
      string record_buf;
      Slice record;
      std::map<std::string, std::string> meta;
      if (!reader.GetMetaData(&meta)) {
        LOG(ERROR) << "Error reading " << argv[i];
        return 1;
      }
      ptype = FindWithDefault(meta, file::kProtoTypeKey, "");
      fd_set = FindWithDefault(meta, file::kProtoSetKey, "");
      if (!ptype.empty() && !fd_set.empty())
        tmp_msg.reset(AllocateMsgByMeta(ptype, fd_set));
      else
        tmp_msg.reset(AllocateMsgFromDescr(FindDescriptor()));
      Printer printer(tmp_msg->GetDescriptor());
      while (reader.ReadRecord(&record, &record_buf)) {
        CHECK(tmp_msg->ParseFromArray(record.data(), record.size()));
        if (test_expr && !plang::EvaluateBoolExpr(*test_expr, *tmp_msg))
          continue;
        printer.Output(*tmp_msg);
      }
    }
  }

  return 0;
}