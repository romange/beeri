add_library(strings ascii_ctype.cc charset.cc join.cc human_readable.cc escaping.cc
            strcat.cc stringpiece.cc memutil.cc serialize.cc
            stringprintf.cc split.cc strip.cc urlencode.cc util.cc numbers.cc strtoint.cc utf8/rune.c)
target_link_libraries(strings base)

cxx_test(stringpiece_test strings)
cxx_test(unique_strings_test strings)
cxx_test(urlencode_test strings)
cxx_test(strcat_test strings)