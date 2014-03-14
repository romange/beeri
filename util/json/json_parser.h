// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
// Based on the jsmn parser (https://bitbucket.org/zserge/jsmn/overview)
#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <ostream>
#include <vector>
#include <utility>

#include "base/logging.h"
#include "strings/stringpiece.h"

namespace util {
class JsonParser;

class JsonObject {
  friend class JsonParser;

public:
  enum Type {
    UNDEFINED,
    KEY_NAME,
    ARRAY,
    OBJECT,
    STRING,
    DOUBLE,
    INTEGER,
    PRIMITIVE,
  };

  enum PrimitiveValue { NULL_V, FALSE_V, TRUE_V };

  struct Value {
    union U {
      StringPiece token;
      int64 int_val;

      struct {
        // for an array and object int_val describes number of transitive subnodes in those types.
        uint32 transitive;

        // immediate tokens. For objects, counts both keys and values, i.e key-value pair
        // contributes 2.
        uint32 immediate;
      } children;

      double d_val;
      PrimitiveValue primitive;
      U() : token() {}
    } __attribute__((aligned(4), packed));

    U u;
    Type type = UNDEFINED;

    Value() {}
    Value(Type t) : type(t) {}

    bool is_composite() const { return type == OBJECT || type == ARRAY; }

    uint32 transitive_size() const {
      return is_composite() ? u.children.transitive : 0;
    }

    StringPiece token() const {
      DCHECK_EQ(type, STRING);
      return u.token;
    }

    PrimitiveValue primitive() const {
      DCHECK(type == PRIMITIVE);
      return u.primitive;
    }
  } __attribute__((aligned(4), packed));


  typedef strings::SliceBase<Value> ValueSlice;

  class Iterator {
  protected:
    ValueSlice slice_;
    bool check_fail_on_schema_errors_ = false;

    Iterator(ValueSlice slice, bool check_fail_on_schema_errors)
      : slice_(slice), check_fail_on_schema_errors_(check_fail_on_schema_errors) {}
    void AdvanceImmediate();
  public:
    Iterator() {}

    bool Done() const { return slice_.empty(); }
  };

  class ObjectIterator;
  class ArrayIterator;

  JsonObject() {}

  Type type() const { return is_defined() ? slice_[0].type : UNDEFINED; }

  JsonObject operator[](StringPiece key) const { return get(key); }
  JsonObject get(StringPiece key) const;

  // When this->is_defined() is false, both methods return valid iterators with Done() = true.
  // ObjectIterator is deprecated.
  ObjectIterator GetObjectIterator() const  __attribute__ ((deprecated));
  ArrayIterator GetArrayIterator() const;

  bool is_defined() const { return !slice_.empty(); }

  // IsNull returns true when this is a defined object with its value = null.
  // For example: '{ foo : null }'
  bool IsNull() const {
    return type() == PRIMITIVE && slice_[0].u.primitive == NULL_V;
  }

  bool IsNumber() const {
    Type t = type();
    return t == DOUBLE || t == INTEGER;
  }

  StringPiece GetStr() const;
  std::string GetString() const { return GetStr().as_string(); }

  int64 GetInt() const;

  bool GetBool() const;

  double GetDouble() const;

  uint32 Size() const;

  string ToString() const;

  // For objects that a value parts in JSON object struct, returns their corresponding name.
  // For example, with "{ foo : 2 }". For JSON integer object "2", its name will be "foo".
  StringPiece name() const { return name_; }

  static JsonObject Undefined() { return JsonObject(nullptr, false); }
private:
  explicit JsonObject(const Value* val, bool check_fail_on_schema_errors,
                      StringPiece name = StringPiece())
      : name_(name), check_fail_on_schema_errors_(check_fail_on_schema_errors) {
    if (val) slice_.set(val, val->transitive_size() + 1);
  }

  ValueSlice slice_; // slice to elements in JsonParser.values_
  StringPiece name_; // the name of the object if exists.
  bool check_fail_on_schema_errors_ = false;
};


class JsonObject::ObjectIterator : public Iterator {
  friend class JsonObject;
  mutable std::pair<StringPiece, JsonObject> res_;
protected:
  ObjectIterator(ValueSlice slice, bool check_fail_on_schema_errors)
      : Iterator(slice, check_fail_on_schema_errors) {}

public:
  ObjectIterator() {}
  std::pair<StringPiece, JsonObject> GetKeyVal() const;

  const std::pair<StringPiece, JsonObject>* operator->() const {
    res_ = GetKeyVal();
    return &res_;
  }

  ObjectIterator& operator++();  // prefix
};

class JsonObject::ArrayIterator : public Iterator {
  friend class JsonObject;
  mutable JsonObject obj_;
  bool is_array_;
protected:
  ArrayIterator(ValueSlice slice, bool check_fail_on_schema_errors, bool is_array)
     : Iterator(slice, check_fail_on_schema_errors), is_array_(is_array) {}

public:
  ArrayIterator() {}
  ArrayIterator& operator++();  // prefix

  JsonObject GetObj() const;

  JsonObject operator*() const { return GetObj(); }

  const JsonObject* operator->() const {
    obj_ = GetObj();
    return &obj_;
  }
};

class JsonParser {
public:
  enum Status {
    SUCCESS,
    MORE_INPUT_EXPECTED,
    INVALID_JSON,
  };

  // if check_fail_on_schema_errors is true then json objects returned by the parser will checkfail
  // when accessed wrongly. For example when array is accessed with obj["keyname"].
  JsonParser(bool check_fail_on_schema_errors = false);

  Status Parse(StringPiece str);

  size_t value_size() const { return values_.size(); }

  const JsonObject::Value& operator[](size_t i) const {
    DCHECK_LT(i, values_.size());
    return values_[i];
  }

  JsonObject root() const;

  JsonObject operator[](StringPiece key) const;

private:
  std::vector<JsonObject::Value> values_;
  std::vector<uint32> depth_;
  uint32 parent_size_ = 0;
  bool check_fail_on_schema_errors_;
};

std::ostream& operator<<(std::ostream& os, const JsonObject::Value& v);

inline JsonObject JsonParser::root() const {
  return values_.empty() ?  JsonObject::Undefined() :
                            JsonObject(values_.data(), check_fail_on_schema_errors_);
}

inline JsonObject JsonParser::operator[](StringPiece key) const {
  return root()[key];
}

inline void JsonObject::Iterator::AdvanceImmediate() {
  uint32 prefix_sz = slice_[0].is_composite() ? 1 + slice_[0].transitive_size() : 1;
  slice_.remove_prefix(prefix_sz);
}

inline std::pair<StringPiece, JsonObject>
    JsonObject::ObjectIterator::GetKeyVal() const {
  DCHECK(slice_.size() > 1 && slice_[0].type == KEY_NAME);
  DCHECK(slice_[1].type != KEY_NAME);
  StringPiece name = slice_[0].u.token;
  return std::make_pair(name, JsonObject(slice_.data() + 1, check_fail_on_schema_errors_, name));
}

inline JsonObject::ObjectIterator& JsonObject::ObjectIterator::operator++() {
  DCHECK(slice_.size() > 1 && slice_[0].type == KEY_NAME);
  AdvanceImmediate();
  AdvanceImmediate();
  return *this;
};

inline JsonObject JsonObject::ArrayIterator::GetObj() const {
  if (slice_.empty()) {
    return JsonObject::Undefined();
  }
  if (is_array_) {
    return JsonObject(slice_.data(), check_fail_on_schema_errors_);
  }
  DCHECK(slice_.size() > 1 && slice_[0].type == KEY_NAME);
  DCHECK(slice_[1].type != KEY_NAME);
  StringPiece name = slice_[0].u.token;
  return JsonObject(slice_.data() + 1, check_fail_on_schema_errors_, name);
}

inline JsonObject::ArrayIterator& JsonObject::ArrayIterator::operator++() {
  DCHECK(!slice_.empty());
  AdvanceImmediate();
  if (!is_array_)
    AdvanceImmediate();
  return *this;
};

}  // namespace util

#endif  // JSON_PARSER_H