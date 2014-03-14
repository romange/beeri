# Copyright 2013, Beeri 15.  All rights reserved.
# Author: Roman Gershman (romange@gmail.com)
#
find_package(Threads)

enable_language(CXX)
# set(CMAKE_CXX_COMPILER g++)
# can not set -Wshadow  due to numerous warnings in protobuf compilation.
# can not see -fno-exceptions because of Thrift.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-rtti -Wextra -msse2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fno-builtin-malloc -fno-builtin-calloc ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-realloc -fno-builtin-free")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -Wno-unused-parameter ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-result")

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -gdwarf-2 -g2")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -DDEBUG -ggdb")
MESSAGE (CMAKE_CXX_COMPILER " ${CMAKE_CXX_COMPILER}")
#MESSAGE (CXX_FLAGS ${CMAKE_CXX_FLAGS})

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
  MESSAGE (CXX_FLAGS_DEBUG " " ${CMAKE_CXX_FLAGS_DEBUG})
ENDIF()
IF(CMAKE_BUILD_TYPE STREQUAL "Release")
  MESSAGE (CXX_FLAGS_RELEASE " ${CMAKE_CXX_FLAGS_RELEASE}")
ENDIF()

IF(EXISTS "${CMAKE_SOURCE_DIR}/../.git")
  FIND_PACKAGE(Git)
  IF(GIT_FOUND)
    EXECUTE_PROCESS(
      COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE "project_BUILD_VERSION"
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    MESSAGE( STATUS "Git version: ${project_BUILD_VERSION}" )
  ELSE(GIT_FOUND)
    SET(project_BUILD_VERSION 0)
  ENDIF(GIT_FOUND)
ENDIF()

IF (BUILD_TIME)
  MESSAGE("Build time ${BUILD_TIME}")
  SET(project_BUILD_TIME "${BUILD_TIME}")
ELSE(BUILD_TIME)
  SET(project_BUILD_TIME "FuckTime!")
ENDIF()

# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}  -fno-omit-frame-pointer")
set(PROJECT_CONTACT romange@gmail.com)

define_property(GLOBAL PROPERTY LIB_INCLUDE_DIR  BRIEF_DOCS "Include directory for that library"
                FULL_DOCS "Include directory for that library")
include(cmake/third_party.cmake)


set(ROOT_GEN_DIR ${CMAKE_SOURCE_DIR}/genfiles)

file(MAKE_DIRECTORY ${ROOT_GEN_DIR})

include_directories(${ROOT_GEN_DIR})

# Thrift requires these two definitions for some types that we use
add_definitions(-DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H)

MESSAGE (STATUS "Enabled both CPU and memory profiling via Google perftools")
#SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ltcmalloc_and_profiler")
#SET (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -ltcmalloc_and_profiler")

function(cur_gen_dir out_dir)
  #string(REPLACE ${CMAKE_SOURCE_DIR}/ "" rel_folder ${CMAKE_CURRENT_SOURCE_DIR})
  file(RELATIVE_PATH _rel_folder "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  set(_tmp_dir ${ROOT_GEN_DIR}/${_rel_folder})
  set(${out_dir} ${_tmp_dir} PARENT_SCOPE)
  file(MAKE_DIRECTORY ${_tmp_dir})
endfunction()

macro(add_include target)
  set_property(TARGET ${target}
               APPEND PROPERTY INCLUDE_DIRECTORIES ${ARGN})
endmacro()

macro(add_compile_flag target)
  set_property(TARGET ${target}
               APPEND PROPERTY COMPILE_FLAGS ${ARGN})
endmacro()

function(gen_cxx_files list_name extension dirname)
  set(_tmp_l "")
  foreach (_file ${ARGN})
#    Message("gen_cxx_files " ${_file})
    LIST(APPEND _tmp_l "${dirname}/${_file}.h" "${dirname}/${_file}.${extension}")
  endforeach(_file)
  set(${list_name} ${_tmp_l} PARENT_SCOPE)
endfunction()


function(cxx_link target)
  CMAKE_PARSE_ARGUMENTS(parsed "" "" "DATA" ${ARGN})
  get_target_property(target_full_name ${target} LOCATION)
  get_filename_component(target_dir ${target_full_name} PATH)
  if (parsed_DATA)
    # symlink data files into build directory
    set(data_dir "${target_dir}/${target}.runfiles")
    set(symlink_files)
    foreach (data_file ${parsed_DATA})
      get_filename_component(data_full_path ${data_file} ABSOLUTE)
      if (NOT EXISTS ${data_full_path})
        Message(FATAL_ERROR "Can not find ${data_full_path} when processing ${target}")
      endif()
      set(target_data_full "${data_dir}/${data_file}")
      get_filename_component(target_data_folder ${target_data_full} PATH)
      file(MAKE_DIRECTORY ${target_data_folder})
      LIST(APPEND symlink_files ${target_data_full})
      ADD_CUSTOM_COMMAND(
             OUTPUT "${target_data_full}"
             COMMAND ${CMAKE_COMMAND} -E create_symlink ${data_full_path} ${target_data_full}
             COMMENT "creating symlink  ${data_full_path} to ${target_data_full}" VERBATIM)
      set_source_files_properties(${target_data_full} PROPERTIES GENERATED TRUE)
      add_dependencies(${target} ${target_data_full})
    endforeach(data_file)
    add_custom_target(${target}_data_files DEPENDS ${symlink_files})
    add_dependencies(${target} ${target}_data_files)
  endif()
  # Message("target (${target} depends on " ${parsed_UNPARSED_ARGUMENTS})
  target_link_libraries(${target} ${parsed_UNPARSED_ARGUMENTS} -ltcmalloc_and_profiler)
  foreach (lib ${parsed_UNPARSED_ARGUMENTS})
    get_target_property(incl_dir ${lib} LIB_INCLUDE_DIR)
    # Message("cxx_link " ${target} " " ${lib} " " ${incl_dir})
    if(NOT ${incl_dir} MATCHES "-NOTFOUND$")
      add_include(${target} ${incl_dir})
      # Message("cxx_link " ${target} " " ${incl_dir})
    endif()
  endforeach(lib)
endfunction()

function(cxx_proto_lib name)
  cur_gen_dir(gen_dir)
  gen_cxx_files(srcs_full_path "cc" ${gen_dir} ${name}.pb)
  GET_FILENAME_COMPONENT(absolute_proto_name ${name}.proto ABSOLUTE)
  CMAKE_PARSE_ARGUMENTS(parsed "" "" "DEPENDS" ${ARGN})
  ADD_CUSTOM_COMMAND(
           OUTPUT ${srcs_full_path}
           COMMAND ${PROTOC} ${absolute_proto_name}
                   --proto_path=${CMAKE_SOURCE_DIR} --cpp_out=${ROOT_GEN_DIR}
           DEPENDS ${name}.proto DEPENDS protobuf_project ${parsed_DEPENDS}
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
           COMMENT "Generating sources from ${absolute_proto_name}" VERBATIM)
  set_source_files_properties(${srcs_full_path}
                              PROPERTIES GENERATED TRUE)
  set(lib_name "${name}_proto")
  add_library(${lib_name} ${srcs_full_path})
  target_link_libraries(${lib_name} ${parsed_DEPENDS} protobuf)
  add_include(${lib_name} ${PROTOBUF_INCLUDE_DIR})
  add_compile_flag(${lib_name} "-DGOOGLE_PROTOBUF_NO_RTTI -Wno-unused-parameter")
endfunction()

macro(sed_escape str_var string_val)
  string(REPLACE "(" "\\(" ${str_var} ${string_val})
  string(REPLACE ")" "\\)" ${str_var} ${${str_var}})
  string(REPLACE "/" "\\/" ${str_var} ${${str_var}})
endmacro()

function(cxx_thrift_lib name)
  cur_gen_dir(gen_dir)
  CMAKE_PARSE_ARGUMENTS(parsed "" "" "SERVICES;DEPENDS;INCLUDES" ${ARGN})
  gen_cxx_files(srcs_full_path "cpp" ${gen_dir} ${name}_types ${name}_constants ${parsed_SERVICES})
  set(include_files)
  set(depend_thrift_libs)
  foreach (_file ${parsed_INCLUDES})
    LIST(APPEND include_files "${gen_dir}/${_file}_types.h")
    LIST(APPEND depend_thrift_libs ${_file}_thrift)
  endforeach(_file)
  #sed_escape(match_prefix "(^#include \".*)(gen-cpp/)")
 # ADD_CUSTOM_TARGET(${name}_target DEPENDS ${srcs_full_path})
  ADD_CUSTOM_COMMAND(
           OUTPUT ${srcs_full_path}
           # TODO: to add pure_enums option to thrift args.
           COMMAND ${THRIFTC} --gen cpp:include_prefix,dense --out ${gen_dir} -I ${CMAKE_SOURCE_DIR} ${name}.thrift

           # due to bug in thrift generator we need to fix include paths.
           # TODO: to remove it once the bug is fixed.
           COMMAND sed -i "s/thrift\\/cxxfunctional.h/functional/" ${srcs_full_path}
           DEPENDS ${name}.thrift ${include_files} DEPENDS thrift_project
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
           COMMENT "Generating sources from ${name}.thrift" VERBATIM)
  set_source_files_properties(${srcs_full_path} PROPERTIES GENERATED TRUE)
  set(lib_name "${name}_thrift")
  # Message("cxx_thrift_lib " ${lib_name} " services: ${parsed_SERVICES}" " depends: " )
  add_library(${lib_name} ${srcs_full_path})
  target_link_libraries(${lib_name} thrift ${parsed_DEPENDS} ${depend_thrift_libs})
  add_include(${lib_name} ${THRIFT_INCLUDE_DIR} ${gen_dir})
  # set_property(TARGET ${lib_name} PROPERTY LIB_INCLUDE_DIR ${THRIFT_INCLUDE_DIR})
endfunction()

function(flex_lib name)
  GET_FILENAME_COMPONENT(_in ${name}.lex ABSOLUTE)
  cur_gen_dir(gen_dir)
  set(lib_name "${name}_flex")
  set(full_path_depends ${CMAKE_CURRENT_SOURCE_DIR}/${name}.ih ${gen_dir}/${name}.h
                        ${gen_dir}/plang_parser_base.h)
  add_library(${lib_name} ${gen_dir}/${name}.cc ${full_path_depends})

  set(full_path_cc ${gen_dir}/${name}.cc ${gen_dir}/${name}_base.h ${gen_dir}/${name}.h)
  ADD_CUSTOM_COMMAND(
           OUTPUT ${full_path_cc}
           COMMAND flexc++ ${_in} -i ${CMAKE_CURRENT_SOURCE_DIR}/${name}.ih
                           -b ${gen_dir}/${name}_base.h -c ${gen_dir}/${name}.h
                           --lex-source=${gen_dir}/${name}.cc --no-lines --target-directory=${gen_dir}
            #due to bug in flexc++, it does not create full path includes in scanner.cc, only local one.
            # as workaround, we soft link .ih file into genfiles directory
           COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/${name}.ih ${gen_dir}/${name}.ih
           DEPENDS ${_in}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
           COMMENT "Generating lexer from ${name}.lex" VERBATIM)

  set_source_files_properties(${gen_dir}/${name}.h ${gen_dir}/${name}.cc ${gen_dir}/${name}_base.h
                              PROPERTIES GENERATED TRUE)
  add_compile_flag(${lib_name} "-Wno-extra")
  target_link_libraries(${lib_name})
endfunction()

function(bison_lib name)
  GET_FILENAME_COMPONENT(_in ${name}.y ABSOLUTE)
  cur_gen_dir(gen_dir)
  set(lib_name "${name}_bison")
  add_library(${lib_name} ${gen_dir}/${name}.cc ${gen_dir}/plang_scanner_base.h)
  add_compile_flag(${lib_name} "-frtti")
  set(full_path_cc ${gen_dir}/${name}.cc ${gen_dir}/${name}_base.h)
  ADD_CUSTOM_COMMAND(
           OUTPUT ${full_path_cc}
           COMMAND bisonc++ ${name}.y -p ${gen_dir}/${name}.cc -c ${CMAKE_CURRENT_SOURCE_DIR}/${name}.h
                            -b ${gen_dir}/${name}_base.h -i ${CMAKE_CURRENT_SOURCE_DIR}/${name}.ih
           DEPENDS ${_in}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
           COMMENT "Generating parser from ${name}.y" VERBATIM)
 set_source_files_properties(${name}.cc ${name}_base.h PROPERTIES GENERATED TRUE)
endfunction()


function(cxx_test name)
  add_executable(${name} ${name}.cc)
  add_compile_flag(${name} "-Wno-sign-compare")
  add_include(${name} ${GTEST_INCLUDE_DIR} ${GMOCK_INCLUDE_DIR})
  cxx_link(${name} gtest_main gtest gmock ${ARGN})
  if (CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(${name} ${CMAKE_THREAD_LIBS_INIT})
  endif()
  add_test(${name} ${CMAKE_BINARY_DIR}/${name})
endfunction()
