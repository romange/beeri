add_library(rpc_server rpc_server.cc)
cxx_link(rpc_server base http stats_lib thrift)

add_library(rpc_client rpc_client.cc)
cxx_link(rpc_client base strings util)

cxx_proto_lib(rpc)
cxx_proto_lib(rpc_sample)

add_library(rpc rpc_channel.cc  rpc_common.cc rpc_context.cc rpc_message_reader.cc
            rpc_server2.cc server_connection.cc)

#add_library(evp STATIC IMPORTED)
#set_property(TARGET evp PROPERTY IMPORTED_LOCATION /home/roman/project/libevent-2.0.21-stable/.libs/libevent_pthreads.a)
#add_library(evc STATIC IMPORTED)
#set_property(TARGET evc PROPERTY IMPORTED_LOCATION /home/roman/project/libevent-2.0.21-stable/.libs/libevent_core.a)
#cxx_link(rpc rpc_proto base strings util evc evp http)
cxx_link(rpc rpc_proto base strings util event_pthreads)

#cxx_test(rpc_test rpc rpc_sample_proto)

add_executable(rpc_server2 rpc_server2_main.cc)
cxx_link(rpc_server2 rpc rpc_sample_proto)

#add_executable(rpc_load_test rpc_load_test.cc)
# cxx_link(rpc_load_test rpc rpc_sample_proto)