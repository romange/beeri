// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <iostream>
#include <memory>

#include "base/googleinit.h"
#include "file/sstable/sstable.h"
#include "file/file.h"

using namespace std;
using namespace file;

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);
  for (int i = 1; i < argc; ++i) {
    auto res = file::ReadonlyFile::Open(argv[i]);
    CHECK(res.status.ok()) << res.status;
    std::unique_ptr<file::ReadonlyFile> file(res.obj);
    auto res2 = sstable::Table::Open(sstable::ReadOptions(), file.get());
    CHECK(res2.status.ok()) << res2.status.ToString();
    std::unique_ptr<sstable::Table> table(res2.obj);

    sstable::Iterator* it = table->NewIterator();
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      it->key();
      it->value();
      // cout << it->key().data() << ": " << it->value().size() << endl;
    }
    CHECK(file->Close().ok());
  }
  return 0;
}