add_library(json json_parser.cc)
target_link_libraries(json strings)
cxx_test(json_parser_test json file DATA bidRequestMopub.json)