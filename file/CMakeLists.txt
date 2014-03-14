add_library(file file.cc file_util.cc filesource.cc list_file.cc list_file_reader.cc
                 meta_map_block.cc s3_file.cc)
cxx_link(file base coding snappy strings util s3)
cxx_test(file_test file)

add_library(test_util test_util.cc)
target_link_libraries(test_util base file)

cxx_test(list_file_test file test_util)

cxx_proto_lib(proto_writer_test DEPENDS addressbook_proto)

add_library(proto_writer proto_writer.cc)
cxx_link(proto_writer file protobuf sstable util)

cxx_test(proto_writer_test proto_writer proto_writer_test_proto)

add_subdirectory(sstable)