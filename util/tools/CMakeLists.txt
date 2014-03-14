add_executable(proto_db proto_db.cc)
cxx_link(proto_db file protobuf)

add_library(pprint_utils pprint_utils.cc)
cxx_link(pprint_utils protobuf strings)

add_executable(pprint pprint.cc)
cxx_link(pprint file pprint_utils plang_parser_bison lmdb proto_writer leveldb)

add_executable(lst2sst lst2sst.cc)
cxx_link(lst2sst file pprint_utils proto_writer)