// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include "util/json/json_parser.h"

#include "base/gtest.h"
#include "file/file_util.h"
#include "strings/strcat.h"
#include <gmock/gmock.h>

namespace util {

MATCHER_P2(ValueWithInt, type, int_val, "") {
  return arg.type == type && arg.u.int_val == int_val;
}

MATCHER_P3(ComplexValue, type, immediate, transitive, "") {
  return arg.type == type && arg.u.children.immediate == immediate &&
         arg.u.children.transitive == transitive;
}

MATCHER_P2(ValueWithString, type, str, "") {
  return arg.type == type && arg.u.token == str;
}

MATCHER_P(Primitive, type, "") {
  return arg.type == JsonObject::PRIMITIVE && arg.u.primitive == type;
}

MATCHER_P(DoubleVal, val, "") {
  return arg.type == JsonObject::DOUBLE && arg.u.d_val == val;
}

class JsonParserTest : public testing::Test {
protected:
  JsonParser parser_;
};

TEST_F(JsonParserTest, Basic) {
  EXPECT_EQ(16, sizeof(JsonObject::Value::U));
  EXPECT_EQ(20, sizeof(JsonObject::Value));

  ASSERT_EQ(JsonParser::SUCCESS, parser_.Parse("{ foo : \"bar\", bar :null, "
                                               "arr : [234, 456.0, false] }"));
  ASSERT_EQ(10, parser_.value_size());
  EXPECT_THAT(parser_[0], ComplexValue(JsonObject::OBJECT, 6, 9));
  EXPECT_THAT(parser_[1], ValueWithString(JsonObject::KEY_NAME, "foo"));
  EXPECT_THAT(parser_[2], ValueWithString(JsonObject::STRING, "bar"));
  EXPECT_THAT(parser_[3], ValueWithString(JsonObject::KEY_NAME, "bar"));
  EXPECT_THAT(parser_[4], Primitive(JsonObject::NULL_V));
  EXPECT_THAT(parser_[5], ValueWithString(JsonObject::KEY_NAME, "arr"));
  EXPECT_THAT(parser_[6], ComplexValue(JsonObject::ARRAY, 3, 3));
  EXPECT_THAT(parser_[7], ValueWithInt(JsonObject::INTEGER, 234));
  EXPECT_THAT(parser_[8], DoubleVal(456.0));
  EXPECT_THAT(parser_[9], Primitive(JsonObject::FALSE_V));

  auto value = parser_["foo"];
  ASSERT_TRUE(value.is_defined());
  EXPECT_EQ("bar", value.GetStr());
  value = parser_["bar"];
  ASSERT_TRUE(value.is_defined());
  ASSERT_TRUE(value.IsNull());
  value = parser_["arr"];
  EXPECT_EQ(3, value.Size());
  auto it = value.GetArrayIterator();
  JsonObject item = *it;
  EXPECT_EQ(234, item.GetInt());
  ++it;
  ++it;
  EXPECT_FALSE(it.Done());
  EXPECT_FALSE(it->GetBool());
  ++it;
  EXPECT_TRUE(it.Done());
}

TEST_F(JsonParserTest, More) {
  ASSERT_EQ(JsonParser::SUCCESS, parser_.Parse(
    "[ { foo : [1,2, { key : 3}], bar : { key1 : [], key2: true}},\"str\"] "));
  auto root = parser_.root();
  ASSERT_EQ(2, root.Size());
  auto it = root.GetArrayIterator();
  JsonObject first = *it;
  JsonObject foo_obj = first.get("foo");
  ASSERT_EQ(3, foo_obj.Size());
  auto arr2 = foo_obj.GetArrayIterator();
  EXPECT_EQ(1, arr2->GetInt());
  ++arr2;
  EXPECT_EQ(2, arr2->GetInt());
  ++arr2;
  EXPECT_EQ(1, arr2->Size());
  EXPECT_EQ(3, arr2.GetObj()["key"].GetInt());
  ++arr2;
  EXPECT_TRUE(arr2.Done());

  JsonObject bar_obj = first["bar"];
  ASSERT_EQ(2, bar_obj.Size());
  EXPECT_EQ(0, bar_obj["key1"].Size());
  EXPECT_TRUE(bar_obj["key2"].GetBool());

  EXPECT_FALSE(bar_obj["key3"].is_defined());
  EXPECT_FALSE(first["key3"].is_defined());
  ++it;
  EXPECT_EQ("str", it.GetObj().GetStr());
  ++it;
  EXPECT_TRUE(it.Done());
}

TEST_F(JsonParserTest, Parsing) {
  EXPECT_EQ(JsonParser::SUCCESS, parser_.Parse("{ key\n: 2}"));
  EXPECT_EQ(JsonParser::SUCCESS, parser_.Parse("{ key\n: 2, }"));
  EXPECT_EQ(1, parser_.root().Size());
  EXPECT_EQ("{key: 2, }\n", parser_.root().ToString());

  EXPECT_EQ(JsonParser::SUCCESS, parser_.Parse("[1, 2, 3, ]"));
  EXPECT_EQ(JsonObject::ARRAY, parser_.root().type());
  EXPECT_EQ(3, parser_.root().Size());

  EXPECT_EQ(JsonParser::INVALID_JSON, parser_.Parse("{ key\n: 2, key2   : }"));

  EXPECT_EQ(JsonParser::SUCCESS, parser_.Parse("{ \"key\":2 }"));
  EXPECT_EQ(2, parser_["key"].GetInt());

  EXPECT_EQ(JsonParser::SUCCESS, parser_.Parse("{ key: \"\"}"));
  auto val = parser_["key"];
  EXPECT_EQ("", val.GetString());
  EXPECT_EQ(0, val.GetStr().size());
}

TEST_F(JsonParserTest, Iterator) {
  ASSERT_EQ(JsonParser::SUCCESS, parser_.Parse("{ key1: [1, 2], key2: [3, 4]}"));
  auto root = parser_.root();
  EXPECT_EQ("{key1: [1, 2], key2: [3, 4], }\n", root.ToString());

  auto it = root.GetObjectIterator();
  ASSERT_FALSE(it.Done());
  auto key_val = it.GetKeyVal();
  EXPECT_EQ("key1", key_val.first);
  auto arr_it = key_val.second.GetArrayIterator();
  EXPECT_EQ(1, arr_it.GetObj().GetInt());
  ++arr_it;
  EXPECT_EQ(2, arr_it.GetObj().GetInt());
  ++arr_it;
  EXPECT_TRUE(arr_it.Done());

  ++it;
  ASSERT_FALSE(it.Done());
  key_val = it.GetKeyVal();
  EXPECT_EQ("key2", key_val.first);
  arr_it = key_val.second.GetArrayIterator();
  EXPECT_EQ(3, arr_it.GetObj().GetInt());
  ++arr_it;
  EXPECT_EQ(4, arr_it.GetObj().GetInt());
  ++arr_it;
  EXPECT_TRUE(arr_it.Done());

  ++it;
  ASSERT_TRUE(it.Done());

  // The same but with array iterator.
  arr_it = root.GetArrayIterator();
  EXPECT_EQ("key1", arr_it->name());
  EXPECT_EQ(JsonObject::ARRAY, arr_it->type());
  ++arr_it;
  EXPECT_EQ("key2", arr_it->name());
  EXPECT_EQ(JsonObject::ARRAY, arr_it->type());
  ++arr_it;
  ASSERT_TRUE(arr_it.Done());
  ASSERT_FALSE(arr_it->is_defined());
}

TEST_F(JsonParserTest, Invalid) {
  auto root = parser_.root();
  ASSERT_FALSE(root.is_defined());
  ASSERT_EQ(JsonParser::SUCCESS, parser_.Parse(" "));
  root = parser_.root();
  ASSERT_FALSE(root.is_defined());
  ASSERT_FALSE(root["foo"].is_defined());

  ASSERT_EQ(JsonParser::SUCCESS, parser_.Parse("{}"));
  root = parser_.root();
  ASSERT_TRUE(root.is_defined());
  auto arr_it = root.GetArrayIterator();
  ASSERT_FALSE(arr_it->is_defined());
}

void BM_Parse(uint32 iters) {
  StopBenchmarkTiming();
  string file = base::ProgramRunfile("bidRequestMopub.json");
  string contents;
  file_util::ReadFileToStringOrDie(file, &contents);
  JsonParser parser;
  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    parser.Parse(contents);
  }
}
BENCHMARK(BM_Parse);

}  // namespace util
