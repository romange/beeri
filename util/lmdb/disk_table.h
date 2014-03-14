// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _DISK_TABLE_H
#define _DISK_TABLE_H

#include <memory>
#include "strings/stringpiece.h"
#include "util/status.h"

struct MDB_cursor;

namespace util {

// single-thread implementation of disk based table.
class DiskTable {
  struct Rep;
  // struct TransRep;

  std::unique_ptr<Rep> rep_;
  bool fail_on_check_;
public:
  class Iterator {
    MDB_cursor* cursor_;
    friend DiskTable;

    Iterator(MDB_cursor* c) : cursor_(c) {}
  public:
    Iterator() : cursor_(nullptr) {}
    Iterator(Iterator&& other) : cursor_(other.cursor_) {
     other.cursor_ = nullptr;
    }

    ~Iterator();
    Iterator& operator=(Iterator&&);

    bool Next(strings::Slice* key, strings::Slice* val);

    bool First(strings::Slice* key, strings::Slice* val);

    // Prevent copying.
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
  };

  struct Options {
    bool fail_on_check = false;
    bool constant_length_keys = false;  // usually used when writing integer keys.

    Options() {}
  };

  explicit DiskTable(uint32 max_size_mb, bool fail_on_check = false);
  ~DiskTable();

  // dir_name - directory where the data should reside.
  Status Open(StringPiece dir_name, const Options& options = Options());

  // Begins read/write transaction. There can be only one open transaction every time.
  Status BeginTransaction(bool read_only = false);

  // if overwrite false then val will be updated with the existing value.
  // returns true if the write was taken place or false in case overwrite is false and
  // the key already existed.
  Status Put(strings::Slice key, strings::Slice val) {
    bool tmp = false;
    return PutInternal(key, true, &val, &tmp);
  }

  Status PutIfNotPresent(strings::Slice key, strings::Slice* val, bool* was_put) {
    return PutInternal(key, false, val, was_put);
  }

  bool Get(strings::Slice key, strings::Slice* val) const;

  bool Delete(strings::Slice key);

  // Silently returns if the transaction was not opened.
  Status CommitTransaction();

  // Aborts currently opened transaction if exists.
  void AbortTransaction();

  // set sync=true if you want synchronous flush of OS buffers into disk.
  Status Flush(bool sync);

  Iterator GetIterator();
private:
  Status PutInternal(strings::Slice key, bool overwrite, strings::Slice* val, bool* was_put);
};

}  // namespace util


#endif  // _DISK_TABLE_H