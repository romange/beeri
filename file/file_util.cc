// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: tomasz.kaftal@gmail.com (Tomasz Kaftal)
//
// Modified by Roman Gershman (romange@gmail.com)
// File management utilities' implementation.

#define __STDC_FORMAT_MACROS 1

#include "file/file_util.h"

#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <vector>

#include "base/walltime.h"

#include "base/logging.h"
#include "strings/join.h"
#include "strings/stringprintf.h"

using std::vector;
using base::Status;

namespace file_util {

static string error_str(int err) {
  string buf(512, '\0');
  char* result = strerror_r(err, &buf.front(), buf.size());
  if (result == buf.c_str())
    return buf;
  else
    return string(result);
}

static void TraverseRecursivelyInternal(StringPiece path,
                                        std::function<void(StringPiece)> cb, uint32 offset) {
  struct stat stats;
  if (stat(path.data(), &stats) != 0) {
    LOG(ERROR) << "TraverseRecursively failed for " << path << "  with error "
               << error_str(errno);
    return;
  }

  char buf[offsetof(struct dirent, d_name) + NAME_MAX + 1 ];
  struct dirent* entry = new (buf) dirent;

  if (S_ISDIR(stats.st_mode)) {
    string current_name;
    DIR* dir = opendir(path.data());
    if (dir == NULL) {
      LOG(ERROR) << "TraverseRecursively: error opening dir " << path << ", error: "
                  << error_str(errno);
      return;
    }
    while (true) {
      struct dirent* der = nullptr;
      CHECK_EQ(readdir_r(dir, entry, &der), 0);
      if (der == nullptr)
        break;
      StringPiece entry_name(entry->d_name);
      if (entry_name == StringPiece(".") || entry_name == StringPiece("..")) {
        continue;
      }
      current_name = JoinPath(path, entry_name);
      if (entry->d_type == DT_DIR) {
        TraverseRecursivelyInternal(current_name, cb, offset);
      } else {
        StringPiece cn(current_name);
        cn.remove_prefix(offset);
        cb(cn);
      }
    }
    closedir(dir);
  } else if (S_ISREG(stats.st_mode)) {
    path.remove_prefix(offset);
    cb(path);
  } else {
    LOG(WARNING) << "unknown type " << stats.st_mode;
  }
}

inline File* TryCreate(const char *directory_prefix) {
  // Attempt to create a temporary file.
  string filename;
  if (!TempFile::TempFilename(directory_prefix, &filename))
    return NULL;
  File* fp = file::Open(filename, "w+");
  if (fp) {
    DLOG(INFO) << "Created fname: " << fp->create_file_name();
    return fp;
  }
  return NULL;
}

string JoinPath(StringPiece dirname, StringPiece basename) {
  if ((!basename.empty() && basename[0] == '/') || dirname.empty()) {
    return basename.ToString();
  } else if (dirname[dirname.size() - 1] == '/') {
    return StrCat(dirname, basename);
  } else {
    return StrCat(dirname, "/", basename);
  }
}

File* OpenOrDie(StringPiece file_name, StringPiece mode) {
  CHECK(!file_name.empty());
  File* fp = file::Open(file_name, mode);
  if (fp == NULL) {
    LOG(FATAL) << "Cannot open file " << file_name << "in mode: " << mode;
    return NULL;
  }
  return fp;
}


bool ReadFileToString(StringPiece name, string* output) {
  uint8 buffer[1024];
  file::FileCloser fcloser(file::Open(name, "r"));
  if (fcloser.get() == nullptr) return false;

  Status status;
  while (!fcloser->eof() && status.ok()) {
    size_t read_size = 0;
    status = fcloser->Read(sizeof(buffer), buffer, &read_size);
    output->append(reinterpret_cast<char*>(buffer), read_size);
  }
  return status.ok();
}

void ReadFileToStringOrDie(StringPiece name, string* output) {
  CHECK(ReadFileToString(name, output)) << "Could not read: " << name;
}

void WriteStringToFileOrDie(StringPiece contents, StringPiece name) {
  FILE* file = fopen(name.data(), "wb");
  CHECK(file != NULL)
      << "fopen(" << name << ", \"wb\"): " << strerror(errno);
  CHECK_EQ(fwrite(contents.data(), 1, contents.size(), file),
                  contents.size())
      << "fwrite(" << name << "): " << strerror(errno);
  CHECK(fclose(file) == 0)
      << "fclose(" << name << "): " << strerror(errno);
}

bool CreateDir(StringPiece name, int mode) {
  return mkdir(name.data(), mode) == 0;
}

bool RecursivelyCreateDir(StringPiece path, int mode) {
  if (CreateDir(path, mode)) return true;

  if (file::Exists(path)) return false;

  // Try creating the parent.
  string::size_type slashpos = path.find_last_of('/');
  if (slashpos == string::npos) {
    // No parent given.
    return false;
  }

  return RecursivelyCreateDir(path.substr(0, slashpos), mode) &&
         CreateDir(path, mode);
}

void DeleteRecursively(StringPiece name) {
  // We don't care too much about error checking here since this is only used
  // in tests to delete temporary directories that are under /tmp anyway.

  // Use opendir()!  Yay!
  // lstat = Don't follow symbolic links.
  struct stat stats;
  if (lstat(name.data(), &stats) != 0) return;

  if (S_ISDIR(stats.st_mode)) {
    DIR* dir = opendir(name.data());
    if (dir != NULL) {
      while (true) {
        struct dirent* entry = readdir(dir);
        if (entry == NULL) break;
        string entry_name = entry->d_name;
        if (entry_name != "." && entry_name != "..") {
          DeleteRecursively(StrCat(name, "/", entry_name));
        }
      }
    }

    closedir(dir);
    rmdir(name.data());

  } else if (S_ISREG(stats.st_mode)) {
    remove(name.data());
  }
}

void TraverseRecursively(StringPiece path, std::function<void(StringPiece)> cb) {
  CHECK(!path.empty());
  uint32 factor = !path.ends_with("/");
  TraverseRecursivelyInternal(path, cb, path.size() + factor);
}

// Tries to create a tempfile in directory 'directory_prefix' or get a
// directory from GetExistingTempDirectories().
/* static */
File* TempFile::Create(const char *directory_prefix) {
  // If directory_prefix is not provided an already-existing temp directory
  // will be used
  if (!(directory_prefix && *directory_prefix)) {
    return TryCreate(NULL);
  }

  struct stat st;
  if (!(stat(directory_prefix, &st) == 0 && S_ISDIR(st.st_mode))) {
    // Directory_prefix does not point to a directory.
    LOG(ERROR) << "Not a directory: " << directory_prefix;
    return NULL;
  }
  return TryCreate(directory_prefix);
}

// Creates a temporary file name using standard library utilities.
static inline void TempFilenameInDir(const char *directory_prefix,
                                     string *filename) {
  int32 tid = static_cast<int32>(pthread_self());
  int32 pid = static_cast<int32>(getpid());
  int64 now = CycleClock::Now();
  int64 now_usec = GetCurrentTimeMicros();
  *filename = JoinPath(directory_prefix, StringPrintf("tempfile-%x-%d-%" PRId64 "x-%" PRId64 "x",
                       tid, pid, now, now_usec));
}

/* static */
bool TempFile::TempFilename(const char *directory_prefix, string *filename) {
  CHECK(filename != NULL);
  filename->clear();

  if (directory_prefix != NULL) {
    TempFilenameInDir(directory_prefix, filename);
    return true;
  }

  // Re-fetching available dirs ensures thread safety.
  vector<string> dirs;
  // TODO(tkaftal): Namespace might vary depending on glog installation options,
  // I should probably use a configure/make flag here.
  google::GetExistingTempDirectories(&dirs);

  // Try each directory, as they might be full, have inappropriate
  // permissions or have different problems at times.
  for (size_t i = 0; i < dirs.size(); ++i) {
    TempFilenameInDir(dirs[i].c_str(), filename);
    if (file::Exists(*filename)) {
      LOG(WARNING) << "unique tempfile already exists in " << *filename;
      filename->clear();
    } else {
      return true;
    }
  }

  LOG(ERROR) << "Couldn't find a suitable TempFile anywhere. Tried "
             << dirs.size() << " directories";
  return false;
}

/* static */
string TempFile::TempFilename(const char *directory_prefix) {
  string file_name;
  CHECK(TempFilename(directory_prefix, &file_name))
      << "Could not create temporary file with prefix: " << directory_prefix;
  return file_name;
}

}  // file_util
