// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/lmdb/disk_table.h"

 #include <sys/stat.h>
#include "base/logging.h"
#include "strings/strcat.h"
#include "util/lmdb/lmdb.h"


#define MDB_ERR_STATUS(code) PREDICT_TRUE((code) == MDB_SUCCESS) ? \
    Status::OK : Status(StatusCode::IO_ERROR, StrCat(__FILE__, ":", __LINE__, " ", \
                                               mdb_strerror(code)))
#define RETURN_IF_MDB_ERR(code) \
  do { \
    if (PREDICT_FALSE(code != MDB_SUCCESS)) { \
      if (fail_on_check_) LOG(FATAL) << mdb_strerror(code); \
      return MDB_ERR_STATUS(code); \
    } \
  } while (false)


using std::pair;
using strings::Slice;
using base::Status;
using base::StatusCode;

namespace util {

static Status LocalFileError() {
  char buf[1024];
  strerror_r(errno, buf, arraysize(buf));
  return Status(StatusCode::IO_ERROR, buf);
}

struct DiskTable::Rep {
  MDB_env* env = nullptr;
  MDB_dbi dbi;
  MDB_txn* txn = nullptr;
};

/*struct DiskTable::TransRep {
  MDB_dbi dbi;
  MDB_txn* txn = nullptr;
  MDB_env* env = nullptr;
};

DiskTable::Transaction::Transaction() : t_rep_(new TransRep) {
}

DiskTable::Transaction::~Transaction() {
  CHECK(t_rep_->txn == nullptr);
}

Status DiskTable::Transaction::Init(DiskTable::Rep* r) {

}*/

/*Status DiskTable::Transaction::Put(StringPiece key, StringPiece val) {

}

Status DiskTable::Transaction::Commit() {
  int rc = mdb_txn_commit(t_rep_->txn);
  t_rep_->txn = nullptr;
  return rc ? Status(StatusCode::IO_ERROR, StrCat("mdb_txn_commit: ", rc)) : Status::OK;
}*/

DiskTable::DiskTable(uint32 size_mb, bool fail_on_check) : rep_(new Rep),
    fail_on_check_(fail_on_check) {
  CHECK_EQ(0, mdb_env_create(&rep_->env));
  CHECK_EQ(0, mdb_env_set_mapsize(rep_->env, uint64(size_mb) * 1024ULL * 1024ULL));
}

DiskTable::~DiskTable() {
  mdb_env_close(rep_->env);
}

Status DiskTable::Open(StringPiece path, const Options& options) {
  CHECK(!path.empty());
  struct stat stats;
  string str(path.as_string());
  if (stat(str.c_str(), &stats) != 0) {
    if (errno != ENOENT)
      return LocalFileError();
    if (mkdir(str.c_str(), 0755) != 0)
      return LocalFileError();
  } else {
    if (!S_ISDIR(stats.st_mode)) {
      return Status(StatusCode::IO_ERROR, StrCat("This path should be directory: ", path));
    }
  }
  fail_on_check_ = options.fail_on_check;
  int rc = mdb_env_open(rep_->env, str.c_str(), MDB_WRITEMAP | MDB_NOSYNC , 0664);

  MDB_stat mdb_stats;
  if (MDB_SUCCESS == mdb_env_stat(rep_->env, &mdb_stats)) {
    LOG(INFO) << "Opened database " << path << " with " << mdb_stats.ms_entries << " entries";
  }

  RETURN_IF_MDB_ERR(rc);
  MDB_txn* txn = nullptr;
  rc = mdb_txn_begin(rep_->env, NULL, 0, &txn);
  RETURN_IF_MDB_ERR(rc);
  int flags = 0;
  if (options.constant_length_keys)
    flags |= MDB_INTEGERKEY;
  rc = mdb_dbi_open(txn, NULL, flags, &rep_->dbi);
  CHECK_EQ(MDB_SUCCESS, rc);
  rc = mdb_txn_commit(txn);
  CHECK_EQ(MDB_SUCCESS, rc);
  return Status::OK;
}

Status DiskTable::BeginTransaction(bool read_only) {
  CHECK(rep_->txn == nullptr);
  int rc = mdb_txn_begin(rep_->env, NULL, read_only ? MDB_RDONLY : 0, &rep_->txn);
    return MDB_ERR_STATUS(rc);
  }

Status DiskTable::PutInternal(Slice key, bool overwrite, Slice* val, bool* was_put) {
  MDB_val m_key, m_data;
  m_key.mv_size = key.size();
  m_key.mv_data = const_cast<char*>(key.charptr());
  m_data.mv_size = val->size();
  m_data.mv_data = const_cast<char*>(val->charptr());
  unsigned int flags = (overwrite ? 0 : MDB_NOOVERWRITE);
  int rc = mdb_put(CHECK_NOTNULL(rep_->txn), rep_->dbi, &m_key, &m_data, flags);
  if (rc == MDB_KEYEXIST) {
    *was_put = false;
    val->set(reinterpret_cast<uint8*>(m_data.mv_data), m_data.mv_size);
    return Status::OK;
  }
  RETURN_IF_MDB_ERR(rc);
  *was_put = true;
  return Status::OK;
}

bool DiskTable::Get(Slice key, Slice* val) const {
  MDB_val m_key, m_data;
  m_key.mv_size = key.size();
  m_key.mv_data = const_cast<char*>(key.charptr());
  int rc = mdb_get(CHECK_NOTNULL(rep_->txn), rep_->dbi, &m_key, &m_data);
  if (rc == MDB_NOTFOUND)
    return false;
  CHECK_EQ(MDB_SUCCESS, rc);
  val->set(reinterpret_cast<uint8*>(m_data.mv_data), m_data.mv_size);
  return true;
}

bool DiskTable::Delete(Slice key) {
  MDB_val m_key;
  m_key.mv_size = key.size();
  m_key.mv_data = const_cast<char*>(key.charptr());
  int rc = mdb_del(CHECK_NOTNULL(rep_->txn), rep_->dbi, &m_key, NULL);
  if (rc == MDB_NOTFOUND) {
    return false;
  }
  CHECK_EQ(MDB_SUCCESS, rc);
  return true;
}

Status DiskTable::CommitTransaction() {
  if (rep_->txn == nullptr)
    return Status::OK;
  int rc = mdb_txn_commit(rep_->txn);
  rep_->txn = nullptr;
  return MDB_ERR_STATUS(rc);
}

// Aborts currently opened transaction if exists.
void DiskTable::AbortTransaction() {
  if (rep_->txn == nullptr)
    return;
  mdb_txn_abort(rep_->txn);
  rep_->txn = nullptr;
}

DiskTable::Iterator DiskTable::GetIterator() {
  MDB_cursor* cursor = nullptr;
  int rc = mdb_cursor_open(rep_->txn, rep_->dbi, &cursor);
  CHECK_EQ(MDB_SUCCESS, rc);
  return Iterator(cursor);
}

/*Status DiskTable::NewTransaction(Transaction* dest) {
  return dest->Init(rep_.get());
}*/

Status DiskTable::Flush(bool sync) {
  int rc = mdb_env_sync(rep_->env, int(sync));
  return MDB_ERR_STATUS(rc);
}

DiskTable::Iterator::~Iterator() {
  mdb_cursor_close(cursor_);
}

DiskTable::Iterator& DiskTable::Iterator::operator=(Iterator&& other) {
  if (cursor_)
    mdb_cursor_close(cursor_);
  cursor_ = other.cursor_;
  return *this;
}

static bool GetByCursor(MDB_cursor_op op, MDB_cursor* cursor, Slice* key, Slice* val) {
  MDB_val m_key, m_data;
  int rc = mdb_cursor_get(cursor, &m_key, &m_data, op);
  if (rc != MDB_SUCCESS) {
    if (rc != MDB_NOTFOUND) {
      LOG(ERROR) << "Error: " << mdb_strerror(rc);
    }
    return false;
  }
  key->set(reinterpret_cast<uint8*>(m_key.mv_data), m_key.mv_size);
  val->set(reinterpret_cast<uint8*>(m_data.mv_data), m_data.mv_size);
  return true;
}

bool DiskTable::Iterator::Next(Slice* key, Slice* val) {
  return GetByCursor(MDB_NEXT, cursor_, key, val);
}

bool DiskTable::Iterator::First(strings::Slice* key, strings::Slice* val) {
  return GetByCursor(MDB_FIRST, cursor_, key, val);
}


}  // namespace util