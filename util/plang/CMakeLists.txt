cxx_proto_lib(addressbook)
cxx_test(proto_test addressbook_proto)

add_library(plang plang.cc)
cxx_link(plang protobuf base)

flex_lib(plang_scanner)

bison_lib(plang_parser)
cxx_link(plang_parser_bison strings plang plang_scanner_flex)

cxx_test(plang_test plang plang_parser_bison addressbook_proto)