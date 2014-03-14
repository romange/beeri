// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef _FILE_SSTABLE_TABLE_H_
#define _FILE_SSTABLE_TABLE_H_

#include <cstdint>
#include "file/sstable/iterator.h"
#include "file/sstable/options.h"

namespace file {

class ReadonlyFile;

namespace sstable {

class Block;
class BlockHandle;
class Footer;

/* TODO:
enum SstCountEnum
{
    //
    // array index values/names
    //
    eSstCountKeys=0,           //!< how many keys in this sst
    eSstCountBlocks=1,         //!< how many blocks in this sst
    eSstCountCompressAborted=2,//!< how many blocks attempted compression and aborted use
    eSstCountKeySize=3,        //!< byte count of all keys
    eSstCountValueSize=4,      //!< byte count of all values
    eSstCountBlockSize=5,      //!< byte count of all blocks (pre-compression)
    eSstCountBlockWriteSize=6, //!< post-compression size, or BlockSize if no compression
    eSstCountIndexKeys=7,      //!< how many keys in the index block
    eSstCountKeyLargest=8,     //!< largest key in sst
    eSstCountKeySmallest=9,    //!< smallest key in sst
    eSstCountValueLargest=10,  //!< largest value in sst
    eSstCountValueSmallest=11, //!< smallest value in sst

    // must follow last index name to represent size of array
    eSstCountEnumSize,          //!< size of the array described by the enum values

    eSstCountVersion=1

};  // enum SstCountEnum

*/

// A Table is a sorted map from strings to strings.  Tables are
// immutable and persistent.  A Table may be safely accessed from
// multiple threads without external synchronization.
class Table {
 public:
  // Attempt to open the table that is stored in bytes [0..file_size)
  // of "file", and read the metadata entries necessary to allow
  // retrieving data from the table.
  //
  // If successful, returns ok and sets "*table" to the newly opened
  // table.  The client should delete "*table" when no longer needed.
  // If there was an error while initializing the table, sets "*table"
  // to NULL and returns a non-ok status.  Does not take ownership of
  // "*source", but the client must ensure that "source" remains live
  // for the duration of the returned table's lifetime.
  //
  // *file must remain live while this Table is in use.
  static base::StatusObject<Table*> Open(const ReadOptions& options,
      ReadonlyFile* file);

  ~Table();

  // Returns a new iterator over the table contents.
  // The result of NewIterator() is initially invalid (caller must
  // call one of the Seek methods on the iterator before using it).
  Iterator* NewIterator() const;

  // Given a key, return an approximate byte offset in the file where
  // the data for that key begins (or would begin if the key were
  // present in the file).  The returned value is in terms of file
  // bytes, and so includes effects like compression of the underlying data.
  // E.g., the approximate offset of the last key in the table will
  // be close to the file length.
  uint64_t ApproximateOffsetOf(const strings::Slice& key) const;

  const std::map<std::string, std::string>& GetMeta() const;
 private:
  struct Rep;
  Rep* rep_;

  explicit Table(Rep* rep) { rep_ = rep; }
  static Iterator* BlockReader(void*, const strings::Slice&);

  void ReadMeta(const Footer& footer);
  void ReadFilter(const strings::Slice& filter_handle_value);

  // No copying allowed
  Table(const Table&);
  void operator=(const Table&);
};

}  // namespace sstable
}  // namespace file

#endif  // _FILE_SSTABLE_TABLE_H_
