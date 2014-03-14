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
    CMAKE_ARGS -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${THIRD_PARTY_LIB_DIR}/${name}
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${THIRD_PARTY_LIB_DIR}/${name}
        -DCMAKE_C_FLAGS:STRING=-O3 -DCMAKE_INSTALL_PREFIX:PATH=${THIRD_PARTY_LIB_DIR}/${name}
        ${list_CMAKE_ARGS}
    ${parsed_UNPARSED_ARGUMENTS}
    )
endfunction()

add_third_party(
    gmock
    INSTALL_COMMAND echo ""
    SVN_REPOSITORY "http://googlemock.googlecode.com/svn/trunk/"
)

set(GMOCK_DIR ${THIRD_PARTY_DIR}/gmock)
set(GMOCK_LIB_DIR "${THIRD_PARTY_LIB_DIR}/gmock")
set(GMOCK_INCLUDE_DIR "${GMOCK_DIR}/include")

set(GTEST_INCLUDE_DIR "${GMOCK_DIR}/gtest/include")

add_third_party(
    gperf
    SVN_REPOSITORY "http://gperftools.googlecode.com/svn/trunk/"
#    URL https://gperftools.googlecode.com/files/gperftools-2.0.tar.gz
    PATCH_COMMAND ./autogen.sh
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --enable-frame-pointers --prefix=${THIRD_PARTY_LIB_DIR}/gperf
)
link_directories(${THIRD_PARTY_LIB_DIR}/gperf/lib)
set(GPERF_INCLUDE_DIR "${THIRD_PARTY_LIB_DIR}/gperf/include")

add_third_party(
    gflags
    SVN_REPOSITORY "http://gflags.googlecode.com/svn/trunk/"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure  --prefix=${THIRD_PARTY_LIB_DIR}/gflags
)

set(GFLAGS_DIR ${THIRD_PARTY_LIB_DIR}/gflags)
set(GFLAGS_INCLUDE_DIR "${GFLAGS_DIR}/include")
set(GFLAGS_LIB_DIR "${GFLAGS_DIR}/lib")

add_third_party(
    glog
    SVN_REPOSITORY "http://google-glog.googlecode.com/svn/trunk/"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure
        --disable-rtti --with-gflags=${GFLAGS_DIR} --prefix=${THIRD_PARTY_LIB_DIR}/glog
        "CXXFLAGS=-std=c++0x -O3 -DSTRIP_LOG=1"
    DEPENDS gflags_project
 )
set(GLOG_DIR ${THIRD_PARTY_LIB_DIR}/glog)
set(GLOG_INCLUDE_DIR "${GLOG_DIR}/include")
set(GLOG_LIB_DIR "${GLOG_DIR}/lib")

#Protobuf project
set(PROTOBUF_DIR ${THIRD_PARTY_LIB_DIR}/protobuf)
add_third_party(
    protobuf
    URL http://protobuf.googlecode.com/files/protobuf-2.5.0.tar.bz2
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --with-zlib --prefix=${PROTOBUF_DIR}
 )
set(PROTOBUF_INCLUDE_DIR "${PROTOBUF_DIR}/include")
set(PROTOBUF_LIB_DIR "${PROTOBUF_DIR}/lib")
set(PROTOC ${PROTOBUF_DIR}/bin/protoc)

set(THRIFT_DIR ${THIRD_PARTY_LIB_DIR}/thrift)
add_third_party(
    thrift
    URL https://dist.apache.org/repos/dist/release/thrift/0.9.1/thrift-0.9.1.tar.gz
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${THRIFT_DIR}
       --with-python=no --with-java=no --with-php=no --with-csharp=no --with-tests=no
    BUILD_IN_SOURCE 1
 )
set(THRIFT_INCLUDE_DIR "${THRIFT_DIR}/include")
set(THRIFT_LIB_DIR "${THRIFT_DIR}/lib")
set(THRIFTC ${THRIFT_DIR}/bin/thrift)

set(SPARSE_HASH_DIR ${THIRD_PARTY_LIB_DIR}/sparsehash)
add_third_party(
  sparsehash
  URL https://sparsehash.googlecode.com/files/sparsehash-2.0.2.tar.gz
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${SPARSE_HASH_DIR} "CXXFLAGS=-std=c++0x -O3"
)
set(SPARSE_HASH_INCLUDE_DIR ${SPARSE_HASH_DIR}/include)

#level db
set(LEVELDB_DIR ${THIRD_PARTY_DIR}/leveldb)
add_third_party(
  leveldb
  GIT_REPOSITORY https://github.com/basho/leveldb.git
  GIT_TAG ae40f7cefa141941d1c8dc7e62baee777efd83e2
#  GIT_REPOSITORY https://code.google.com/p/leveldb/
  CONFIGURE_COMMAND echo "foo"
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND echo ""
)
set(LEVELDB_INCLUDE_DIR "${LEVELDB_DIR}/include")
set(LEVELDB_LIB_DIR ${LEVELDB_DIR})

add_third_party(osmium
  GIT_REPOSITORY https://github.com/joto/osmium.git
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND echo "foo"
  INSTALL_COMMAND echo ""
)
set(OSMIUM_INCLUDE_DIR ${THIRD_PARTY_DIR}/osmium/include)

if(false)
# Mongoose
set(mongoose_patch_cmd ${CMAKE_COMMAND} -E echo "add_library(mongoose STATIC mongoose.c)"
                      > <SOURCE_DIR>/CMakeLists.txt
#                      && echo "target_link_libraries(mongoose dl)" >> <SOURCE_DIR>/CMakeLists.txt
                      # && mkdir -p "${THIRD_PARTY_DIR}/include/mongoose"
)
add_third_party(mongoose GIT_REPOSITORY git://github.com/valenok/mongoose.git
  GIT_TAG "4.1"
  PATCH_COMMAND ${mongoose_patch_cmd}
  INSTALL_COMMAND echo ""EV
)
endif()

add_third_party(evhtp GIT_REPOSITORY git://github.com/ellzey/libevhtp.git
# checking out the version that contains config support.
  GIT_TAG 374a269
  BUILD_IN_SOURCE 1
  CMAKE_PASS_FLAGS "-DEVHTP_DISABLE_SSL:STRING=ON  -DEVHTP_DISABLE_REGEX:STRING=ON"
)
set(EVHTP_INCLUDE_DIR "${THIRD_PARTY_LIB_DIR}/evhtp/include")
set(EVHTP_LIB_DIR "${THIRD_PARTY_LIB_DIR}/evhtp/lib")

set(SNAPPY_DIR "${THIRD_PARTY_LIB_DIR}/snappy")
add_third_party(snappy SVN_REPOSITORY http://snappy.googlecode.com/svn/trunk/
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


declare_imported_lib(gflags ${GFLAGS_LIB_DIR} gflags_project)
declare_imported_lib(gmock ${GMOCK_LIB_DIR} gmock_project)
declare_imported_lib(gtest ${GMOCK_LIB_DIR} gmock_project)
declare_imported_lib(glog ${GLOG_LIB_DIR} glog_project)
declare_imported_lib(protobuf ${PROTOBUF_LIB_DIR} protobuf_project)
declare_imported_lib(thrift ${THRIFT_LIB_DIR} thrift_project)
declare_imported_lib(mongoose ${THIRD_PARTY_LIB_DIR}/mongoose mongoose_project)
declare_imported_lib(snappy ${SNAPPY_LIB_DIR} snappy_project)
declare_imported_lib(evhtp ${EVHTP_LIB_DIR} evhtp_project)
declare_imported_lib(leveldb ${LEVELDB_LIB_DIR} leveldb_project)

set_property(TARGET protobuf PROPERTY LIB_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIR})
set_property(TARGET snappy PROPERTY LIB_INCLUDE_DIR ${SNAPPY_INCLUDE_DIR})
add_dependencies(glog gflags)
set_target_properties(glog PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES gflags)
set_target_properties(thrift PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES rt
                      LIB_INCLUDE_DIR ${THRIFT_INCLUDE_DIR})
set_target_properties(mongoose PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES dl)
set_target_properties(evhtp PROPERTIES IMPORTED_LINK_INTERFACE_LIBRARIES event LIB_INCLUDE_DIR ${EVHTP_INCLUDE_DIR})

include_directories(${THIRD_PARTY_DIR}/include)
include_directories(${GLOG_INCLUDE_DIR})
include_directories(${GFLAGS_INCLUDE_DIR})
include_directories(${SPARSE_HASH_INCLUDE_DIR})
include_directories(${THRIFT_INCLUDE_DIR})
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${GPERF_INCLUDE_DIR})
include_directories(${LEVELDB_INCLUDE_DIR})