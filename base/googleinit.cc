// Copyright 2014, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include "base/googleinit.h"
#include "base/logging.h"

#include <gperftools/malloc_extension.h>

namespace __internal__ {

ModuleInitializer::ModuleInitializer(VoidFunction ctor, VoidFunction dtor)
     : destructor_(dtor) {
    if (ctor) ctor();
}

ModuleInitializer::~ModuleInitializer() {
  if (destructor_) destructor_();
}

}  // __internal__

#undef MainInitGuard

MainInitGuard::MainInitGuard(int* argc, char*** argv) {
  MallocExtension::Initialize();
  google::ParseCommandLineFlags(argc, argv, true);
  google::InitGoogleLogging((*argv)[0]);

#if defined NDEBUG
  LOG(INFO) << (*argv)[0] << " running in opt mode.";
#else
  LOG(INFO) << (*argv)[0] << " running in debug mode.";
#endif
}

MainInitGuard::~MainInitGuard() {
  google::ShutdownGoogleLogging();
}