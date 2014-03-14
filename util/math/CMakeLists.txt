add_library(math mathlimits.cc mathutil.cc exactfloat/exactfloat.cc)
cxx_link(math base crypto)