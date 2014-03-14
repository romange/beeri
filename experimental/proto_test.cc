#include <gtest/gtest.h>
#include "experimental/addressbook.pb.h"


class ProtoTest : public testing::Test {
};

TEST_F(ProtoTest, Basic) {
  tutorial::Person person;
  person.set_name("Roman");
}