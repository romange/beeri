// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//

#include <cstdio>
#include <cstdlib>
#include <gflags/gflags.h>

#include "base/logging.h"
#include "mapreduce/mapreduce.h"

DEFINE_string(input, "", "Input gzipped text file.");


int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  CHECK(!FLAGS_input.empty());
  mr::JobConfig job_config;
  mr::InputConfig input_config;
  input_config.path = FLAGS_input;
  input_config.mapper_type = "BeerMapper";
  job_config.input.push_back(input_config);
  job_config.output.num_shards = 10;
  job_config.output.reducer_type = "IdentityReducer";
  mr::Result result = mr::Run(job_config);
  google::ShutdownGoogleLogging();
  return 0;
}

