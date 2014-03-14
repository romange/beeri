add_library(coding bit_pack.cc coder.cc varint.cc int_coder.cc string_coder.cc)
cxx_link(coding base z fastpfor)
cxx_test(coding_test coding file DATA testdata/small_numbers.txt testdata/medium2.txt)
cxx_test(bit_pack_test coding)

add_library(pb_serializer pb_writer.cc pb_reader.cc)
cxx_link(pb_serializer base coding protobuf)

add_library(fastpfor fastpfor/bitpacking.cc fastpfor/fastpfor.cc)
cxx_link(fastpfor base)

cxx_test(pb_serializer_test file pb_serializer strings util addressbook_proto)
cxx_test(string_coder_test coding util)
cxx_test(fastpfor_test fastpfor file DATA testdata/small_numbers.txt testdata/medium1.txt
         testdata/numbers64.txt.gz)