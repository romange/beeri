// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
// Internal - should not be used directly.
#ifndef _S3_FILE_H
#define _S3_FILE_H

#include "strings/stringpiece.h"
#include "base/status.h"

namespace file {
class ReadonlyFile;

base::StatusObject<ReadonlyFile*> OpenS3File(StringPiece name);
bool ExistsS3File(StringPiece name);

}  // namespace file

#endif  // _S3_FILE_H