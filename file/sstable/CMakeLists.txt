add_library(sstable block.cc block_builder.cc filter_block.cc format.cc iterator.cc sstable.cc
            sorting_builder.cc sstable_builder.cc two_level_iterator.cc)
cxx_link(sstable file snappy status strings util)

cxx_test(filter_block_test sstable)
cxx_test(sstable_test sstable snappy test_util)