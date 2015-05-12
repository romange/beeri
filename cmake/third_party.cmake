# Copyright 2013, Beeri 15.  All rights reserved.
# Author: Roman Gershman (romange@gmail.com)
#
SET_DIRECTORY_PROPERTIES(PROPERTIES EP_PREFIX ${THIRD_PARTY_DIR})

INCLUDE(ExternalProject)

set(THIRD_PARTY_LIB_DIR "${THIRD_PARTY_DIR}/libs")

function(add_third_party name)
  CMAKE_PARSE_ARGUMENTS(parsed "NOPARALLEL" "CMAKE_PASS_FLAGS" "" ${ARGN})
  set(BUILD_OPTIONS "-j4")
  if (parsed_NOPARALLEL)
    set(BUILD_OPTIONS "")
  endif()
  if (parsed_CMAKE_PASS_FLAGS)
    string(REPLACE " " ";" list_CMAKE_ARGS ${parsed_CMAKE_PASS_FLAGS})
  endif()
  ExternalProject_Add(${name}_project
    DOWNLOAD_DIR ${THIRD_PARTY_DIR}/${name}
    SOURCE_DIR ${THIRD_PARTY_DIR}/${name}
    UPDATE_COMMAND ""
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${BUILD_OPTIONS}
    # Wrap download, configure and build steps in a script to log output
    LOG_INSTALL ON
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON

    # we need those CMAKE_ARGS for cmake based 3rd party projects.
    CMAKE_ARGS -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${THIRD_PARTY_LIB_DIR}/${name}
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${THIRD_PARTY_LIB_DIR}/${name}
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_C_FLAGS:STRING=-O3 -DCMAKE_CXX_FLAGS:STRING=-O3
        -DCMAKE_INSTALL_PREFIX:PATH=${THIRD_PARTY_LIB_DIR}/${name}
        ${list_CMAKE_ARGS}
    ${parsed_UNPARSED_ARGUMENTS}
    )
endfunction()

add_third_party(
    gmock
    INSTALL_COMMAND echo ""
    URL http://googlemock.googlecode.com/files/gmock-1.7.0.zip
    # SVN_REPOSITORY "http://googlemock.googlecode.com/svn/trunk/"
)

set(GMOCK_DIR ${THIRD_PARTY_DIR}/gmock)
set(GMOCK_LIB_DIR "${THIRD_PARTY_LIB_DIR}/gmock")
set(GMOCK_INCLUDE_DIR "${GMOCK_DIR}/include")

set(GTEST_INCLUDE_DIR "${GMOCK_DIR}/gtest/include")

add_third_party(
    gperf
    #SVN_REPOSITORY "http://gperftools.googlecode.com/svn/trunk/"
    # URL https://googledrive.com/host/0B6NtGsLhIcf7MWxMMF9JdTN3UVk/gperftools-2.2.tar.gz
    URL https://googledrive.com/host/0B6NtGsLhIcf7MWxMMF9JdTN3UVk/gperftools-2.4.tar.gz
    #PATCH_COMMAND ./autogen.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --enable-frame-pointers --prefix=${THIRD_PARTY_LIB_DIR}/gperf
)
link_directories(${THIRD_PARTY_LIB_DIR}/gperf/lib)
set(GPERF_INCLUDE_DIR "${THIRD_PARTY_LIB_DIR}/gperf/include")

set(GLOG_DIR ${THIRD_PARTY_LIB_DIR}/glog)
set(GLOG_INCLUDE_DIR "${GLOG_DIR}/include")
set(GLOG_LIB_DIR "${GLOG_DIR}/lib")
add_third_party(
    glog
    GIT_REPOSITORY https://github.com/google/glog.git
    CONFIGURE_COMMAND <SOURCE_DIR>/configure
         #--disable-rtti we can not use rtti because of the fucking thrift.
         --with-gflags=/usr
         --prefix=${THIRD_PARTY_LIB_DIR}/glog
        "CXXFLAGS=-std=c++0x -O3 -DSTRIP_LOG=1"
    INSTALL_COMMAND make install
    COMMAND sed -i "s/GOOGLE_PREDICT_/GOOGLE_GLOG_PREDICT_/g" ${GLOG_INCLUDE_DIR}/glog/logging.h
 )


#Protobuf project
set(PROTOBUF_DIR ${THIRD_PARTY_LIB_DIR}/protobuf)
add_third_party(
    protobuf
    GIT_REPOSITORY https://github.com/romange/protobuf.git

    PATCH_COMMAND ./autogen.sh

    CONFIGURE_COMMAND <SOURCE_DIR>/configure --with-zlib  --with-tests=no
#     "CXXFLAGS=-O0 -ggdb -std=c++11"
      "CXXFLAGS=-O3 -std=c++11"
      --prefix=${PROTOBUF_DIR}
 )
set(PROTOBUF_INCLUDE_DIR "${PROTOBUF_DIR}/include")
set(PROTOBUF_LIB_DIR "${PROTOBUF_DIR}/lib")
set(PROTOC ${PROTOBUF_DIR}/bin/protoc)

set(THRIFT_DIR ${THIRD_PARTY_LIB_DIR}/thrift)
add_third_party(
    thrift
    GIT_REPOSITORY https://github.com/romange/thrift.git
    GIT_TAG 0.9.x
    # URL https://dist.apache.org/repos/dist/release/thrift/0.9.1/thrift-0.9.1.tar.gz
    PATCH_COMMAND ./bootstrap.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${THRIFT_DIR}
       --with-python=no --with-java=no --with-php=no --with-csharp=no
       --with-tests=no "CXXFLAGS=-std=c++0x -O3"
       # enable this to debug thrift "CXXFLAGS=-O0 -ggdb"
    BUILD_IN_SOURCE 1
)
set(THRIFT_INCLUDE_DIR "${THRIFT_DIR}/include")
set(THRIFT_LIB_DIR "${THRIFT_DIR}/lib")
set(THRIFTC ${THRIFT_DIR}/bin/thrift)


set(SPARSE_HASH_DIR ${THIRD_PARTY_LIB_DIR}/sparsehash)
add_third_party(
  sparsehash
  GIT_REPOSITORY https://github.com/romange/sparsehash.git
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${SPARSE_HASH_DIR} "CXXFLAGS=-std=c++0x -O3"
)
set(SPARSE_HASH_INCLUDE_DIR ${SPARSE_HASH_DIR}/include)

set(BENCHMARK_INCLUDE_DIR ${THIRD_PARTY_LIB_DIR}/benchmark/include)
set(BENCHMARK_LIB_DIR ${THIRD_PARTY_LIB_DIR}/benchmark/lib)

add_third_party(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG d51ba32791ac95437e209abfb651bc6d30c61b8b
  #COMMAND
  #                                               ${BENCHMARK_INCLUDE_DIR}/benchmark/benchmark.h
)

add_third_party(osmium
  GIT_REPOSITORY https://github.com/joto/osmium.git
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND echo "foo"
  INSTALL_COMMAND echo ""
)
set(OSMIUM_INCLUDE_DIR ${THIRD_PARTY_DIR}/osmium/include)


set(CITYHASH_DIR ${THIRD_PARTY_LIB_DIR}/cityhash)
add_third_party(cityhash
  URL http://cityhash.googlecode.com/files/cityhash-1.1.1.tar.gz
  PATCH_COMMAND autoreconf --force --install
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${CITYHASH_DIR} --enable-sse4.2 "CXXFLAGS=-O3 -msse4.2"
)
set(CITYHASH_INCLUDE_DIR "${CITYHASH_DIR}/include")
set(CITYHASH_LIB_DIR "${CITYHASH_DIR}/lib")


add_third_party(evhtp GIT_REPOSITORY https://github.com/romange/libevhtp.git
# checking out the version that contains config support.
  GIT_TAG roman_fixes
  BUILD_IN_SOURCE 1
  CMAKE_PASS_FLAGS "-DEVHTP_DISABLE_SSL:STRING=ON  -DEVHTP_DISABLE_REGEX:STRING=ON"
)
set(EVHTP_INCLUDE_DIR "${THIRD_PARTY_LIB_DIR}/evhtp/include")
set(EVHTP_LIB_DIR "${THIRD_PARTY_LIB_DIR}/evhtp/lib")

set(SNAPPY_DIR "${THIRD_PARTY_LIB_DIR}/snappy")
add_third_party(snappy
  GIT_REPOSITORY https://github.com/google/snappy.git
  PATCH_COMMAND ./autogen.sh
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${THIRD_PARTY_LIB_DIR}/snappy
                     "CXXFLAGS=-std=c++0x -O3"
)
set(SNAPPY_INCLUDE_DIR "${SNAPPY_DIR}/include")
set(SNAPPY_LIB_DIR "${SNAPPY_DIR}/lib")

function(declare_imported_lib name path)
  add_library(${name} STATIC IMPORTED)
  set_property(TARGET ${name} PROPERTY IMPORTED_LOCATION ${path}/lib${name}.a)
  add_dependencies(${name} ${ARGN})
endfunction()


declare_imported_lib(gmock ${GMOCK_LIB_DIR} gmock_project)
declare_imported_lib(benchmark ${BENCHMARK_LIB_DIR} benchmark_project)
declare_imported_lib(gtest ${GMOCK_LIB_DIR} gmock_project)
declare_imported_lib(glog ${GLOG_LIB_DIR} glog_project)
declare_imported_lib(protobuf ${PROTOBUF_LIB_DIR} protobuf_project)
declare_imported_lib(thrift ${THRIFT_LIB_DIR} thrift_project)

declare_imported_lib(snappy ${SNAPPY_LIB_DIR} snappy_project)
declare_imported_lib(evhtp ${EVHTP_LIB_DIR} evhtp_project)
declare_imported_lib(cityhash ${CITYHASH_LIB_DIR} cityhash_project)

set_property(TARGET protobuf PROPERTY LIB_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIR})
set_property(TARGET snappy PROPERTY LIB_INCLUDE_DIR ${SNAPPY_INCLUDE_DIR})
set_property(TARGET cityhash PROPERTY LIB_INCLUDE_DIR ${CITYHASH_INCLUDE_DIR})

set_target_properties(thrift PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES "rt;"
                      LIB_INCLUDE_DIR ${THRIFT_INCLUDE_DIR})

set_target_properties(evhtp PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES event LIB_INCLUDE_DIR ${EVHTP_INCLUDE_DIR})

include_directories(${THIRD_PARTY_DIR}/include)
include_directories(${GLOG_INCLUDE_DIR})
include_directories(${SPARSE_HASH_INCLUDE_DIR})
include_directories(${THRIFT_INCLUDE_DIR})
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${GPERF_INCLUDE_DIR})
include_directories(${LEVELDB_INCLUDE_DIR})