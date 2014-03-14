//
// Copyright 1999-2006 and onwards Google, Inc.
//
// Useful string functions and so forth.  This is a grab-bag file.
//
// You might also want to look at memutil.h, which holds mem*()
// equivalents of a lot of the str*() functions in string.h,
// eg memstr, mempbrk, etc.
//
// These functions work fine for UTF-8 strings as long as you can
// consider them to be just byte strings.  For example, due to the
// design of UTF-8 you do not need to worry about accidental matches,
// as long as all your inputs are valid UTF-8 (use \uHHHH, not \xHH or \oOOO).
//
// Caveats:
// * all the lengths in these routines refer to byte counts,
//   not character counts.
// * case-insensitivity in these routines assumes that all the letters
//   in question are in the range A-Z or a-z.
//
// If you need Unicode specific processing (for example being aware of
// Unicode character boundaries, or knowledge of Unicode casing rules,
// or various forms of equivalence and normalization), take a look at
// files in i18n/utf8.

#ifndef STRINGS_UTIL_H_
#define STRINGS_UTIL_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>  // for strcasecmp, but msvc does not have this header
#endif

#include <functional>
using std::binary_function;
using std::less;
#include <string>
using std::string;
#include <vector>
using std::vector;

#include "base/integral_types.h"
#include "base/port.h"
#include "strings/stringpiece.h"

// Newer functions.

namespace strings {

inline StringPiece GetFileExtension(StringPiece path) {
  auto p = path.rfind('.');
  if (p == StringPiece::npos) return StringPiece();
  return StringPiece(path, p + 1);
}

// Finds the next end-of-line sequence.
// An end-of-line sequence is one of:
//   \n    common on unix, including mac os x
//   \r    common on macos 9 and before
//   \r\n  common on windows
//
// Returns a StringPiece that contains the end-of-line sequence (a pointer into
// the input, 1 or 2 characters long).
//
// If the input does not contain an end-of-line sequence, returns an empty
// StringPiece located at the end of the input:
//    StringPiece(sp.data() + sp.length(), 0).

StringPiece FindEol(StringPiece sp);


// ----------------------------------------------------------------------
// LowerString()
// UpperString()
//    Convert the characters in "s" to lowercase or uppercase.  ASCII-only:
//    these functions intentionally ignore locale because they are applied to
//    identifiers used in the Protocol Buffer language, not to natural-language
//    strings.
// ----------------------------------------------------------------------

inline void LowerString(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    // tolower() changes based on locale.  We don't want this!
    if ('A' <= *i && *i <= 'Z') *i += 'a' - 'A';
  }
}

inline void UpperString(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    // toupper() changes based on locale.  We don't want this!
    if ('a' <= *i && *i <= 'z') *i += 'A' - 'a';
  }
}

// Replaces all occurrences of substring in s with replacement. Returns the
// number of instances replaced. s must be distinct from the other arguments.
//
// Less flexible, but faster, than RE::GlobalReplace().
int GlobalReplaceSubstring(const StringPiece& substring,
                           const StringPiece& replacement,
                           string* s);

bool ReplaceSuffix(StringPiece suffix, StringPiece new_suffix, string* str);

// Returns whether str begins with prefix.
inline bool HasPrefixString(const StringPiece& str,
                            const StringPiece& prefix) {
  return str.starts_with(prefix);
}

// Returns whether str ends with suffix.
inline bool HasSuffixString(const StringPiece& str,
                            const StringPiece& suffix) {
  return str.ends_with(suffix);
}

}  // namespace strings

// Compares two char* strings for equality. (Works with NULL, which compares
// equal only to another NULL). Useful in hash tables:
//    hash_map<const char*, Value, hash<const char*>, streq> ht;
struct streq : public std::binary_function<const char*, const char*, bool> {
  bool operator()(const char* s1, const char* s2) const {
    return ((s1 == 0 && s2 == 0) ||
            (s1 && s2 && *s1 == *s2 && strcmp(s1, s2) == 0));
  }
};

// Compares two char* strings. (Works with NULL, which compares greater than any
// non-NULL). Useful in maps:
//    map<const char*, Value, strlt> m;
struct strlt : public std::binary_function<const char*, const char*, bool> {
  bool operator()(const char* s1, const char* s2) const {
    return (s1 != s2) && (s2 == 0 || (s1 != 0 && strcmp(s1, s2) < 0));
  }
};

// Returns whether str has only Ascii characters (as defined by ascii_isascii()
// in strings/ascii_ctype.h).
bool IsAscii(const char* str, int len);
inline bool IsAscii(const StringPiece& str) {
  return IsAscii(str.data(), str.size());
}

// Returns the smallest lexicographically larger string of equal or smaller
// length. Returns an empty string if there is no such successor (if the input
// is empty or consists entirely of 0xff bytes).
// Useful for calculating the smallest lexicographically larger string
// that will not be prefixed by the input string.
//
// Examples:
// "a" -> "b", "aaa" -> "aab", "aa\xff" -> "ab", "\xff" -> "", "" -> ""
// string PrefixSuccessor(const StringPiece& prefix);

// Returns the immediate lexicographically-following string. This is useful to
// turn an inclusive range into something that can be used with Bigtable's
// SetLimitRow():
//
//     // Inclusive range [min_element, max_element].
//     string min_element = ...;
//     string max_element = ...;
//
//     // Equivalent range [range_start, range_end).
//     string range_start = min_element;
//     string range_end = ImmediateSuccessor(max_element);
//
// WARNING: Returns the input string with a '\0' appended; if you call c_str()
// on the result, it will compare equal to s.
//
// WARNING: Transforms "" -> "\0"; this doesn't account for Bigtable's special
// treatment of "" as infinity.
// string ImmediateSuccessor(const StringPiece& s);

// Fills in *separator with a short string less than limit but greater than or
// equal to start. If limit is greater than start, *separator is the common
// prefix of start and limit, followed by the successor to the next character in
// start. Examples:
// FindShortestSeparator("foobar", "foxhunt", &sep) => sep == "fop"
// FindShortestSeparator("abracadabra", "bacradabra", &sep) => sep == "b"
// If limit is less than or equal to start, fills in *separator with start.
// void FindShortestSeparator(const StringPiece& start, const StringPiece& limit,
  //                         string* separator);

// Replaces the first occurrence (if replace_all is false) or all occurrences
// (if replace_all is true) of oldsub in s with newsub. In the second version,
// *res must be distinct from all the other arguments.
string StringReplace(const StringPiece& s, const StringPiece& oldsub,
                     const StringPiece& newsub, bool replace_all);
void StringReplace(const StringPiece& s, const StringPiece& oldsub,
                   const StringPiece& newsub, bool replace_all,
                   string* res);

// Gets the next token from string *stringp, where tokens are strings separated
// by characters from delim.
char* gstrsep(char** stringp, const char* delim);


#if 0
// Case-insensitive strstr(); use system strcasestr() instead.
// WARNING: Removes const-ness of string argument!
char* gstrcasestr(const char* haystack, const char* needle);

// Finds (case insensitively) the first occurrence of (null terminated) needle
// in at most the first len bytes of haystack. Returns a pointer into haystack,
// or NULL if needle wasn't found.
// WARNING: Removes const-ness of haystack!
const char* gstrncasestr(const char* haystack, const char* needle, size_t len);
char* gstrncasestr(char* haystack, const char* needle, size_t len);

// Finds (case insensitively), in str (which is a list of tokens separated by
// non_alpha), a token prefix and a token suffix. Returns a pointer into str of
// the position of prefix, or NULL if not found.
// WARNING: Removes const-ness of string argument!
char* gstrncasestr_split(const char* str,
                         const char* prefix, char non_alpha,
                         const char* suffix,
                         size_t n);

// Finds (case insensitively) needle in haystack, paying attention only to
// alphanumerics in either string. Returns a pointer into haystack, or NULL if
// not found.
// Example: strcasestr_alnum("This is a longer test string", "IS-A-LONGER")
// returns a pointer to "is a longer".
// WARNING: Removes const-ness of string argument!
char* strcasestr_alnum(const char* haystack, const char* needle);

// Returns the number times substring appears in text.
// Note: Runs in O(text.length() * substring.length()). Do *not* use on long
// strings.
int CountSubstring(StringPiece text, StringPiece substring);

// Finds, in haystack (which is a list of tokens separated by delim), an token
// equal to needle. Returns a pointer into haystack, or NULL if not found (or
// either needle or haystack is empty).
const char* strstr_delimited(const char* haystack,
                             const char* needle,
                             char delim);

// Finds, in the_string, the first "word" (consecutive !ascii_isspace()
// characters). Returns pointer to the beginning of the word, and sets *end_ptr
// to the character after the word (which may be space or '\0'); returns NULL
// (and *end_ptr is undefined) if no next word found.
// end_ptr must not be NULL.
const char* ScanForFirstWord(const char* the_string, const char** end_ptr);
inline char* ScanForFirstWord(char* the_string, char** end_ptr) {
  // implicit_cast<> would be more appropriate for casting to const,
  // but we save the inclusion of "base/casts.h" here by using const_cast<>.
  return const_cast<char*>(
      ScanForFirstWord(const_cast<const char*>(the_string),
                       const_cast<const char**>(end_ptr)));
}

// For the following functions, an "identifier" is a letter or underscore,
// followed by letters, underscores, or digits.

// Returns a pointer past the end of the "identifier" (see above) beginning at
// str, or NULL if str doesn't start with an identifier.
const char* AdvanceIdentifier(const char* str);
/*inline char* AdvanceIdentifier(char* str) {
  // implicit_cast<> would be more appropriate for casting to const,
  // but we save the inclusion of "base/casts.h" here by using const_cast<>.
  return const_cast<char*>(AdvanceIdentifier(const_cast<const char*>(str)));
}*/

// Finds the first tag and value in a string of tag/value pairs.
//
// The first pair begins after the first occurrence of attribute_separator (or
// string_terminal, if not '\0'); tag_value_separator separates the tag and
// value; and the value ends before the following occurrence of
// attribute_separator (or string_terminal, if not '\0').
//
// Returns true (and populates tag, tag_len, value, and value_len) if a
// tag/value pair is founds; returns false otherwise.
bool FindTagValuePair(const char* in_str, char tag_value_separator,
                      char attribute_separator, char string_terminal,
                      char** tag, int* tag_len,
                      char** value, int* value_len);
#endif

// Returns whether s contains only whitespace characters (including the case
// where s is empty).
bool OnlyWhitespace(const StringPiece& s);

// Formats a string in the same fashion as snprintf(), but returns either the
// number of characters written, or zero if not enough space was available.
// (snprintf() returns the number of characters that would have been written if
// enough space had been available.)
//
// A drop-in replacement for the safe_snprintf() macro.
int SafeSnprintf(char* str, size_t size, const char* format, ...)
    PRINTF_ATTRIBUTE(3, 4);

// Reads a line (terminated by delim) from file into *str. Reads delim from
// file, but doesn't copy it into *str. Returns true if read a delim-terminated
// line, or false on end-of-file or error.
// bool GetlineFromStdioFile(FILE* file, string* str, char delim);

#endif  // STRINGS_UTIL_H_
