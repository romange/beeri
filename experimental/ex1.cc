// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "base/googleinit.h"

// Could be found in /usr/include/cppconn/driver.h
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using namespace std;

DEFINE_string(input, "", "Input gzipped text file.");

int main(int argc, char **argv) {
  MainInitGuard guard(&argc, &argv);

  cout << "value of argument input is " << FLAGS_input << endl;

  return 0;
}
