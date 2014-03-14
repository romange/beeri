add_library(proc_stats proc_stats.cc)
cxx_link(proc_stats strings)

add_library(util bzip_source.cc zlib_source.cc crc32c.cc
            scheduler.cc sinksource.cc)

cxx_link(util strings z bz2 status)

cxx_test(sinksource_test strings protobuf util)
cxx_test(crc32c_test util)
cxx_test(scheduler_test util proc_stats)

add_executable(pb_serializer_main pb_serializer.cc)
cxx_link(pb_serializer_main addressbook_proto base protobuf util pb_serializer coding)

add_library(threads executor.cc)
cxx_link(threads proc_stats event event_pthreads)
cxx_test(executor_test threads)

add_subdirectory(coding)
add_subdirectory(http)
add_subdirectory(math)
add_subdirectory(geometry)
add_subdirectory(rpc)
add_subdirectory(json)
add_subdirectory(lmdb)
add_subdirectory(plang)
add_subdirectory(tools)
add_subdirectory(stats)
