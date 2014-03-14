// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "file/sstable/sstable.h"

#include <memory>
#include "file/file.h"
#include "file/meta_map_block.h"
#include "file/sstable/filter_policy.h"
#include "file/sstable/options.h"
#include "file/sstable/block.h"
#include "file/sstable/filter_block.h"
#include "file/sstable/format.h"
#include "file/sstable/two_level_iterator.h"

namespace file {
namespace sstable {

using strings::Slice;
using base::Status;
using base::StatusCode;

struct Table::Rep {
  ~Rep() {
    delete filter;
    delete index_block;
  }

  ReadOptions options;
  Status status;
  ReadonlyFile* file;
  FilterBlockReader* filter;
  std::unique_ptr<uint8[]> filter_data;

  BlockHandle metaindex_handle;  // Handle to metaindex_block: saved from footer
  Block* index_block;
  MetaMapBlock meta_map_block;
};

 base::StatusObject<Table*> Table::Open(const ReadOptions& options,
                                        ReadonlyFile* file) {
  size_t size = file->Size();
  if (size < Footer::kEncodedLength) {
    return Status(StatusCode::INVALID_ARGUMENT, "file is too short to be an sstable");
  }

  uint8 footer_space[Footer::kEncodedLength];
  Slice footer_input;
  Status s = file->Read(size - Footer::kEncodedLength, Footer::kEncodedLength,
                        &footer_input, footer_space);
  if (!s.ok()) return s;

  Footer footer;
  s = footer.DecodeFrom(footer_input);
  if (!s.ok()) return s;

  // Read the index block
  BlockContents contents;

  s = ReadBlock(file, ReadOptions(), footer.index_handle(), &contents);
  if (!s.ok()) return s;

  // We've successfully read the footer and the index block: we're
  // ready to serve requests.
  Rep* rep = new Table::Rep;
  rep->options = options;
  rep->file = file;
  rep->metaindex_handle = footer.metaindex_handle();
  rep->index_block = new Block(contents);
  rep->filter_data = NULL;
  rep->filter = NULL;
  Table* table = new Table(rep);
  table->ReadMeta(footer);

  return table;
}

void Table::ReadMeta(const Footer& footer) {
  // TODO(sanjay): Skip this if footer.metaindex_handle() size indicates
  // it is an empty block.
  ReadOptions opt;
  BlockContents contents;
  if (!ReadBlock(rep_->file, opt, footer.metaindex_handle(), &contents).ok()) {
    // Do not propagate errors since meta info is not needed for operation
    LOG(ERROR) << "Error reading meta block";
    return;
  }
  Block* meta = new Block(contents);

  std::unique_ptr<Iterator> iter(meta->NewIterator());
  if (rep_->options.filter_policy != NULL) {
    std::string key(kFilterNamePrefix);
    key.append(rep_->options.filter_policy->Name());
    iter->Seek(key);
    if (iter->Valid() && iter->key() == Slice(key)) {
      ReadFilter(iter->value());
    }
  }
  Slice meta_map_key = Slice::FromCstr(kMetaBlockKey);
  iter->Seek(meta_map_key);
  if (iter->Valid() && iter->key() == meta_map_key) {
    auto st = rep_->meta_map_block.DecodeFrom(iter->value());
    if (!st.ok()) {
      LOG(ERROR) << "Could not decode meta block";
    }
  }
  delete meta;
}

void Table::ReadFilter(const Slice& filter_handle_value) {
  Slice v = filter_handle_value;
  BlockHandle filter_handle;
  if (!filter_handle.DecodeFrom(&v).ok()) {
    return;
  }

  // We might want to unify with ReadBlock() if we start
  // requiring checksum verification in Table::Open.
  ReadOptions opt;
  BlockContents block;
  if (!ReadBlock(rep_->file, opt, filter_handle, &block).ok()) {
    return;
  }
  if (block.heap_allocated) {
    rep_->filter_data.reset(const_cast<uint8*>(block.data.data()));     // Will need to delete later
  }
  rep_->filter = new FilterBlockReader(rep_->options.filter_policy, block.data);
}

Table::~Table() {
  delete rep_;
}

static void DeleteBlock(void* arg) {
  delete reinterpret_cast<Block*>(arg);
}

// Convert an index iterator value (i.e., an encoded BlockHandle)
// into an iterator over the contents of the corresponding block.
Iterator* Table::BlockReader(void* arg,
                             const Slice& index_value) {
  Table* table = reinterpret_cast<Table*>(arg);
  Block* block = NULL;
  BlockHandle handle;
  Slice input = index_value;
  Status s = handle.DecodeFrom(&input);
  // We intentionally allow extra stuff in index_value so that we
  // can add more features in the future.

  if (s.ok()) {
    BlockContents contents;
    s = ReadBlock(table->rep_->file, table->rep_->options, handle, &contents);
    if (s.ok()) {
      block = new Block(contents);
      Iterator* iter = block->NewIterator();
      iter->RegisterCleanup(&DeleteBlock, block);
      return iter;
    }
  }

  return NewErrorIterator(s);
}

Iterator* Table::NewIterator() const {
  return NewTwoLevelIterator(
      rep_->index_block->NewIterator(),
      &Table::BlockReader, const_cast<Table*>(this));
}

uint64_t Table::ApproximateOffsetOf(const Slice& key) const {
  Iterator* index_iter = rep_->index_block->NewIterator();
  index_iter->Seek(key);
  uint64_t result;
  if (index_iter->Valid()) {
    BlockHandle handle;
    Slice input = index_iter->value();
    Status s = handle.DecodeFrom(&input);
    if (s.ok()) {
      result = handle.offset();
    } else {
      // Strange: we can't decode the block handle in the index block.
      // We'll just return the offset of the metaindex block, which is
      // close to the whole file size for this case.
      result = rep_->metaindex_handle.offset();
    }
  } else {
    // key is past the last key in the file.  Approximate the offset
    // by returning the offset of the metaindex block (which is
    // right near the end of the file).
    result = rep_->metaindex_handle.offset();
  }
  delete index_iter;
  return result;
}

const std::map<string, string>& Table::GetMeta() const {
  return rep_->meta_map_block.meta();
}

}  // namespace sstable
}  // namespace file