// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
#include <cstdlib>
#include "json_parser.h"

#include "base/logging.h"
#include "strings/ascii_ctype.h"
#include "strings/escaping.h"
#include "strings/numbers.h"

namespace util {

std::ostream& operator<<(std::ostream& os, const JsonObject::Value& v) {
  os << "Value(" << v.type << ", ";
  switch (v.type) {
    case JsonObject::INTEGER:
      os << v.u.int_val;
      break;
    case JsonObject::OBJECT:
    case JsonObject::ARRAY:
      os << v.u.children.immediate << ", " << v.u.children.transitive;
      break;
    case JsonObject::STRING:
    case JsonObject::KEY_NAME:
      os << v.u.token;
      break;
    case JsonObject::PRIMITIVE:
      os << v.u.primitive;
      break;
    case JsonObject::DOUBLE:
      os << v.u.d_val;
      break;
    case JsonObject::UNDEFINED:
      os << "undef";
      break;
  }
  os << ") ";
  return os;
}


static JsonParser::Status parse_primitive(StringPiece str, JsonObject::Value* val,
                                          uint32* skip) {
  bool is_float = false;
  bool is_number = true;
  size_t i = 0;
  for (; i < str.size(); ++i) {
    char c = str[i];
    switch (c) {
      /* In strict mode primitive must be followed by "," or "}" or "]" */
      case ':':
      case '\t': case '\r' : case '\n' : case ' ' :
      case ',' : case ']'  : case '}' :
        goto found;
      case '.':
        is_float = true;
        break;
      default:
        if (c > '9')
          is_number = false;
    }
    if (c < 32 || c >= 127) {
      VLOG(1) << "Invalid character '" << int(c) << "' in " << str;
      return JsonParser::INVALID_JSON;
    }
  }

found:
  errno = 0;
  char* endptr = nullptr;
  if (is_float) {
    val->type = JsonObject::DOUBLE;
    val->u.d_val = strtod(str.data(), &endptr);
  } else if (is_number) {
    val->type = JsonObject::INTEGER;
    val->u.int_val = strtoll(str.data(), &endptr, 10);
  } else if (i == 4 && str.starts_with("null")) {
    val->type = JsonObject::PRIMITIVE;
    val->u.primitive = JsonObject::NULL_V;
  } else if (i == 4 && str.starts_with("true")) {
    val->type = JsonObject::PRIMITIVE;
    val->u.primitive = JsonObject::TRUE_V;
  } else if (i == 5 && str.starts_with("false")) {
    val->type = JsonObject::PRIMITIVE;
    val->u.primitive = JsonObject::FALSE_V;
  } else {
    size_t end = i;
    while (i < str.size() && ascii_isspace(str[i])) ++i;
    if (i < str.size() && str[i] == ':') {
      val->type = JsonObject::KEY_NAME;
      val->u.token.set(str.data(), end);
    } else {
      VLOG(1) << "key name is not followed by ':', key: " << StringPiece(str.data(), end);
      return JsonParser::INVALID_JSON;
   }
  }
  *skip = i - 1;
  return errno ? JsonParser::INVALID_JSON : JsonParser::SUCCESS;
}

static JsonParser::Status parse_string(StringPiece str, JsonObject::Value* val) {
  /* Skip starting quote */
  for (size_t i = 1; i < str.size(); ++i) {
    char c = str[i];
    /* Quote: end of string */
    if (c == '\"') {
      val->type = JsonObject::STRING;
      val->u.token.set(str.data() + 1, i - 1);
      return JsonParser::SUCCESS;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\') {
      i++;
      switch (str[i]) {
        /* Allowed escaped symbols */
        case '\"': case '/' : case '\\' : case 'b' :
        case 'f' : case 'r' : case 'n'  : case 't' :
          break;
        /* Allows escaped symbol \uXXXX */
        case 'u':
          // We do not validate the escaping rules.
          break;
        /* Unexpected symbol */
        default:
          VLOG(1) << "Unexpected symbol '" << c << "'";
          return JsonParser::INVALID_JSON;
      }
    }
  }
  return JsonParser::MORE_INPUT_EXPECTED;
}

JsonParser::JsonParser(bool check_fail_on_schema_errors)
  : check_fail_on_schema_errors_(check_fail_on_schema_errors) {
  depth_.reserve(16);
  values_.reserve(128);
}

JsonParser::Status JsonParser::Parse(StringPiece str) {
  depth_.clear();
  values_.clear();
  parent_size_ = 0;

  JsonObject::Type type;
  JsonObject::Value val;
  JsonParser::Status st = SUCCESS;
  uint32 line = 1;
  for (size_t pos = 0; pos < str.size(); ++pos) {
    char c = str[pos];
    switch (c) {
      case '{': case '[':
        type = (c == '{') ? JsonObject::OBJECT : JsonObject::ARRAY;
        if (!depth_.empty()) {
          values_[depth_.back()].u.children.immediate += (parent_size_ + 1);
        }
        parent_size_ = 0;
        depth_.push_back(values_.size());
        values_.emplace_back(type);
        values_.back().u.children.transitive = 0;
        break;
      case '}': case ']': {
          type = (c == '}') ? JsonObject::OBJECT : JsonObject::ARRAY;
          if (depth_.empty()) {
            VLOG(1) << "Unbalanced " << c << " line " << line;
            return INVALID_JSON; // todo: to add more info
          }
          DCHECK_LT(depth_.back(), values_.size());
          JsonObject::Value& v = values_[depth_.back()];
          if (v.type != type) {
            VLOG(1) << "Unmatched parenthis for " << c << " line " << line;
            return INVALID_JSON; // mismatched bracket.
          }
          v.u.children.immediate += parent_size_;
          if (type == JsonObject::OBJECT && v.u.children.immediate % 2 != 0) {
            VLOG(1) << "Odd number of tokens in object."  << " line " << line;
            return INVALID_JSON; // mismatched bracket.
          }
          parent_size_ = 0;
          v.u.children.transitive = values_.size() - depth_.back() - 1;
          depth_.pop_back();
        }
        break;
      case '\"':
        st = parse_string(StringPiece(str, pos), &val);
        if (st != SUCCESS) return st;
        values_.push_back(val);
        pos += val.u.token.size() + 1; // skip the string value.
        ++parent_size_;
        break;
      case '\t' : case '\r' : case ',': case ' ':
        break;
      case '\n':
        ++line;
        break;
      case ':':  // dictionary, we must be inside object
        if (depth_.empty())
          return INVALID_JSON;
        DCHECK(!values_.empty());
        if (values_.back().type != JsonObject::KEY_NAME) {
          if (values_.back().type != JsonObject::STRING) {
            VLOG(1) << "Unexpected type for key in json object " << values_.back().type;
            return INVALID_JSON; // mismatched bracket.
          }
          values_.back().type = JsonObject::KEY_NAME;
        }
        break;
      /* In non-strict mode every unquoted value is a primitive */
      default: {
          uint32 skip = 0;
          st = parse_primitive(StringPiece(str, pos), &val, &skip);
          if (st != SUCCESS) return st;
          values_.push_back(val);
          pos += skip;
          ++parent_size_;
        }
        break;
    }
  }
  if (!depth_.empty())
     return MORE_INPUT_EXPECTED;
  return SUCCESS;
}

JsonObject JsonObject::get(StringPiece key) const {
   if (slice_.empty())
     return JsonObject(nullptr, false, key);
   const JsonObject::Value& root = slice_[0];
   if (check_fail_on_schema_errors_) {
     CHECK_EQ(JsonObject::OBJECT, root.type) << ", key: " << key;
   } else {
    if (root.type != JsonObject::OBJECT)
     return JsonObject::Undefined();
   }
   DCHECK_LT(root.transitive_size(), slice_.size());
   ObjectIterator it(ValueSlice(slice_, 1, root.transitive_size()), check_fail_on_schema_errors_);
   for (; !it.Done(); ++it) {
     auto key_val = it.GetKeyVal();
     if (key_val.first == key) {
       JsonObject res(key_val.second);
       return res;
     }
   }
   return JsonObject(nullptr, false, key);
}

JsonObject::ObjectIterator JsonObject::GetObjectIterator() const {
  if (type() == JsonObject::OBJECT) {
    return ObjectIterator(ValueSlice(slice_, 1, slice_[0].transitive_size()),
                          check_fail_on_schema_errors_);
  }
  if (!check_fail_on_schema_errors_ || !is_defined()) {
    return ObjectIterator();
  }

  LOG(FATAL) << "Non object type: " << type();
  return ObjectIterator(); // never gets here.
}

JsonObject::ArrayIterator JsonObject::GetArrayIterator() const {
  Type t = type();
  bool is_array = true;
  switch (t) {
    case JsonObject::OBJECT:
      is_array = false;
    case JsonObject::ARRAY:
      return ArrayIterator(ValueSlice(slice_, 1, slice_[0].transitive_size()),
                           check_fail_on_schema_errors_, is_array);
    case JsonObject::UNDEFINED:
      return ArrayIterator();
    default: {}
  }
  if (!check_fail_on_schema_errors_) {
    return ArrayIterator();
  }
  LOG(FATAL) << "Non array type: " << type();
  return ArrayIterator();
}

StringPiece JsonObject::GetStr() const {
  CHECK_EQ(type(), JsonObject::STRING) << name_;
  return slice_[0].token();
}

int64 JsonObject::GetInt() const {
  CHECK_EQ(type(), JsonObject::INTEGER);
  return slice_[0].u.int_val;
}

double JsonObject::GetDouble() const {
  CHECK(type() == JsonObject::DOUBLE || type() == JsonObject::INTEGER);
  if (type() == JsonObject::DOUBLE)
    return slice_[0].u.d_val;
  return slice_[0].u.int_val;
}

bool JsonObject::GetBool() const {
  auto res = slice_[0].primitive();
  CHECK(res != JsonObject::NULL_V);
  return res == JsonObject::TRUE_V;
}

uint32 JsonObject::Size() const {
  CHECK(slice_[0].is_composite());
  return type() == JsonObject::ARRAY ? slice_[0].u.children.immediate :
         slice_[0].u.children.immediate / 2;
}

string JsonObject::ToString() const {
  string res;
  switch(type()) {
    case INTEGER: res = SimpleItoa(slice_[0].u.int_val); break;
    case STRING: {
      StringPiece token = slice_[0].token();
      res.append("\"").append(token.data(), token.size()).append("\"");
    }
    break;
    case DOUBLE: res = SimpleDtoa(slice_[0].u.d_val); break;
    case PRIMITIVE: {
      auto p = slice_[0].primitive();
      switch (p) {
        case NULL_V: res = "null"; break;
        case TRUE_V: res = "true"; break;
        case FALSE_V: res = "false"; break;
      }
    }
    break;
    case ARRAY: {
      res.append("[");
      uint32 sz = slice_[0].u.children.immediate;
      uint32 i = 0;
      for (auto it = GetArrayIterator(); !it.Done(); ++it) {
        res.append(it.GetObj().ToString());
        if (++i < sz) res.append(", ");
      }
      res.append("]");
    }
    break;
    case OBJECT: {
      res.append("{");
      for (auto it = GetArrayIterator(); !it.Done(); ++it) {
        StringPiece name = it->name();
        res.append(name.data(), name.size()).append(": ")
           .append(it->ToString()).append(", ");
      }
      res.append("}\n");
    }
    break;

    default:
      LOG(FATAL) << "unsupported " << type();
  }
  return res;
}

}  // namespace util