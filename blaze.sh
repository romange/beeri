#!/bin/bash

root=`dirname "$0"`
root=`cd "$root"; pwd`

TARGET_BUILD_TYPE=Debug
BUILD_DIR=build
for ARG in $*
do
    case "$ARG" in
    -release)
        TARGET_BUILD_TYPE=Release
        BUILD_DIR=build-opt
        shift
        ;;
    esac
done
if [ ! -d build ]; then
  mkdir build
fi
cd build
set -x
cmake -DCMAKE_BUILD_TYPE=$TARGET_BUILD_TYPE ..
if [ $# == 0 ]; then
  make thrift_project
fi
make -j4 $@
cd $root

