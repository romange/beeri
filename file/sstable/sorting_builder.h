// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _FILE_SSTABLE_SORTED_SSTABLE_BUILDER_H
#define _FILE_SSTABLE_SORTED_SSTABLE_BUILDER_H

#include "base/arena.h"
#include "base/status.h"

#include "strings/stringpiece.h"
#include "file/sstable/options.h"

namespace file {
namespace sstable {

// SortingBuilder helps creating disk based sstables when the order of added keys is not in
// increasing order as required by sstable builder.
class SortingBuilder {
public:
  struct Options {
    // How much memory is allocated for storing the temporary table before sorting it and dumping
    // it on disk.
    unsigned mem_sort_size_mb = 128;
  };

  // basename is path to the output sstable not including the extension .sst.
  // For example, "/somepath/mytable".
  // SortingBuilder might create temporary lst files in format
  // basename + "%5d.lst"
  SortingBuilder(StringPiece basename, Options options);

  void Add(strings::Slice key, strings::Slice value);

  // Return non-ok iff some error has been detected.
  base::Status status() const;

  // Finish building the table.  Creates the file passed to the
  // constructor ( + ".sst").
  // REQUIRES: Finish() have not been called.
  // sstable::Options are used for creating the sstable.
  base::Status Finish(sstable::Options options);
private:

};

}  // namespace sstable
}  // namespace file

#endif  // _FILE_SSTABLE_SORTED_SSTABLE_BUILDER_H