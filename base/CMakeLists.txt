cur_gen_dir(gen_dir)
set(VERSION_FILE ${gen_dir}/version.cc)
CONFIGURE_FILE(version.cc.in ${VERSION_FILE} @ONLY)
set_source_files_properties(${VERSION_FILE} PROPERTIES GENERATED TRUE)
add_library(base arena.cc bits.cc cuckoo_map.cc googleinit.cc hash.cc histogram.cc logging.cc mime_types.cc
            random.cc walltime.cc ${VERSION_FILE})
cxx_link(base gflags glog rt ${CMAKE_THREAD_LIBS_INIT})

# Define default gtest_main for tests.
add_library(gtest_main gtest_main.cc)
target_link_libraries(gtest_main base glog gmock)
add_include(gtest_main ${GTEST_INCLUDE_DIR})

add_library(status status.cc)
cxx_link(status base status_proto)

cxx_test(sync_queue_test base)
cxx_test(refcount_test base)
cxx_test(walltime_test base)
cxx_test(cuckoo_map_test base)
cxx_test(histogram_test base)

cxx_proto_lib(status)