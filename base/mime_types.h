// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#ifndef _MIME_TYPES_H
#define _MIME_TYPES_H

namespace mime {
  struct Type {
    const char* ext;
    const char* mime_type;
  };

  extern Type type[516];

}  // namespace mime

#endif  // _MIME_TYPES_H
