// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

namespace cpp bla

enum BlaBla {
  OK,
  CANCELLED,
  NOT_IMPLEMENTED_ERROR,
  RUNTIME_ERROR,
  INTERNAL_ERROR,
  IO_ERROR = 20,
  END_OF_STREAM = 100,
}

struct Request {
  1: required i32 status_code
  2: list<string> msgs
}

struct Response {
  1: required i32 code
}

service ExampleService {
    Response foo(1:Request req);
    i32 bar(1:i32 arg);
}