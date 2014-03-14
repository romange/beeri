// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/sstable/two_level_iterator.h"

#include "file/sstable/options.h"
#include "file/sstable/iterator_wrapper.h"

namespace file {
namespace sstable {

using strings::Slice;
using base::Status;

namespace {

class TwoLevelIterator: public Iterator {
 public:
  TwoLevelIterator(
    Iterator* index_iter,
    BlockFunction block_function,
    void* arg);

  virtual ~TwoLevelIterator();

  virtual void Seek(const Slice& target);
  virtual void SeekToFirst();
  virtual void SeekToLast();
  virtual void Next();
  virtual void Prev();

  virtual bool Valid() const {
    return data_iter_.Valid();
  }
  virtual Slice key() const {
    DCHECK(Valid());
    return data_iter_.key();
  }
  virtual Slice value() const {
    DCHECK(Valid());
    return data_iter_.value();
  }
  virtual Status status() const {
    // It'd be nice if status() returned a const Status& instead of a Status
    if (!index_iter_.status().ok()) {
      return index_iter_.status();
    } else if (data_iter_.iter() != NULL && !data_iter_.status().ok()) {
      return data_iter_.status();
    } else {
      return status_;
    }
  }

 private:
  void SaveError(const Status& s) {
    if (status_.ok() && !s.ok()) status_ = s;
  }
  void SkipEmptyDataBlocksForward();
  void SkipEmptyDataBlocksBackward();
  void SetDataIterator(Iterator* data_iter);
  bool InitDataBlock();

  BlockFunction block_function_;
  void* arg_;
  Status status_;
  IteratorWrapper index_iter_;
  IteratorWrapper data_iter_; // May be NULL
  // If data_iter_ is non-NULL, then "data_block_handle_" holds the
  // "index_value" passed to block_function_ to create the data_iter_.
  std::string data_block_handle_;
};

TwoLevelIterator::TwoLevelIterator(
    Iterator* index_iter,
    BlockFunction block_function,
    void* arg)
    : block_function_(block_function),
      arg_(arg),
      index_iter_(index_iter),
      data_iter_(NULL) {
}

TwoLevelIterator::~TwoLevelIterator() {
}

void TwoLevelIterator::Seek(const Slice& target) {
  VLOG(1) << "Seeking index to " << target.as_string();
  index_iter_.Seek(target);
  if (InitDataBlock()) {
    VLOG(1) << "Seeking data to " << target.as_string();
    data_iter_.Seek(target);
  }
  SkipEmptyDataBlocksForward();
}

void TwoLevelIterator::SeekToFirst() {
  index_iter_.SeekToFirst();
  if (InitDataBlock()) {
    VLOG(1) << "data_iter_.SeekToFirst()";
    data_iter_.SeekToFirst();
  }
  SkipEmptyDataBlocksForward();
}

void TwoLevelIterator::SeekToLast() {
  index_iter_.SeekToLast();
  if (InitDataBlock())
    data_iter_.SeekToLast();
  SkipEmptyDataBlocksBackward();
}

void TwoLevelIterator::Next() {
  DCHECK(Valid());
  data_iter_.Next();
  SkipEmptyDataBlocksForward();
}

void TwoLevelIterator::Prev() {
  DCHECK(Valid());
  data_iter_.Prev();
  SkipEmptyDataBlocksBackward();
}


void TwoLevelIterator::SkipEmptyDataBlocksForward() {
  while (data_iter_.iter() == NULL || !data_iter_.Valid()) {
    // Move to next block
    if (!index_iter_.Valid()) {
      VLOG(2) << "SkipEmptyDataBlocksForward: Index iter not valid";
      SetDataIterator(NULL);
      return;
    }
    VLOG(2) << "SkipEmptyDataBlocksForward: move to next index";
    index_iter_.Next();
    if (InitDataBlock()) data_iter_.SeekToFirst();
  }
}

void TwoLevelIterator::SkipEmptyDataBlocksBackward() {
  while (data_iter_.iter() == NULL || !data_iter_.Valid()) {
    // Move to next block
    if (!index_iter_.Valid()) {
      SetDataIterator(NULL);
      return;
    }
    index_iter_.Prev();
    if (InitDataBlock()) data_iter_.SeekToLast();
  }
}

void TwoLevelIterator::SetDataIterator(Iterator* data_iter) {
  if (data_iter_.iter() != NULL) SaveError(data_iter_.status());
  data_iter_.Set(data_iter);
}

bool TwoLevelIterator::InitDataBlock() {
  if (!index_iter_.Valid()) {
    VLOG(1) << "Index not valid";
    SetDataIterator(NULL);
    return false;
  }
  Slice handle = index_iter_.value();
  if (data_iter_.iter() != NULL && handle == Slice(data_block_handle_)) {
    VLOG(1) << "Same data block";
    // data_iter_ is already constructed with this iterator, so
    // no need to change anything
  } else {
    Iterator* iter = (*block_function_)(arg_, handle);
    data_block_handle_.assign(handle.charptr(), handle.size());
    SetDataIterator(iter);
    VLOG(1) << "Created new data iterator";
  }
  return data_iter_.iter() != nullptr;
}

}  // namespace

Iterator* NewTwoLevelIterator(
    Iterator* index_iter,
    BlockFunction block_function,
    void* arg) {
  return new TwoLevelIterator(index_iter, block_function, arg);
}

}  // namespace sstable
}  // namespace file
