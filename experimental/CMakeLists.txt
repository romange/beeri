cxx_proto_lib(beer_review)

cxx_test(streams_test protobuf test_util)

cxx_thrift_lib(thrift_service SERVICES ExampleService)

add_executable(rpc_server_main rpc_server_main.cc)
cxx_link(rpc_server_main rpc_server thrift_service_thrift file DATA index.html)

add_executable(rpc_client_main rpc_client_main.cc)
cxx_link(rpc_client_main base strings util rpc_client thrift_service_thrift)

add_executable(xml_parser xml_parser.cc)
cxx_link(xml_parser http_client)

add_executable(read_sstable read_sstable.cc)
cxx_link(read_sstable base sstable)