add_library(lmdb mdb.c midl.c disk_table.cc)
cxx_link(lmdb util strings)
cxx_test(disk_table_test lmdb file)