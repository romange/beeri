//
// Copyright (C) 1999-2005 Google, Inc.
//

// TODO(user): visit each const_cast.  Some of them are no longer necessary
// because last Single Unix Spec and grte v2 are more const-y.

#include "strings/util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>           // for FastTimeToBuffer()
#include <algorithm>
using std::copy;
using std::max;
using std::min;
using std::reverse;
using std::sort;
using std::swap;
#include <string>
using std::string;
#include <vector>
using std::vector;

#include "base/logging.h"
#include "strings/ascii_ctype.h"
#include "strings/numbers.h"
#include "strings/stringpiece.h"

const char* strncaseprefix(const char* haystack, int haystack_size,
                                  const char* needle, int needle_size) {
  if (needle_size > haystack_size) {
    return NULL;
  } else {
    if (strncasecmp(haystack, needle, needle_size) == 0) {
      return haystack + needle_size;
    } else {
      return NULL;
    }
  }
}

char* strcasesuffix(char* str, const char* suffix) {
  const int lenstr = strlen(str);
  const int lensuffix = strlen(suffix);
  char* strbeginningoftheend = str + lenstr - lensuffix;

  if (lenstr >= lensuffix && 0 == strcasecmp(strbeginningoftheend, suffix)) {
    return (strbeginningoftheend);
  } else {
    return (NULL);
  }
}

const char* strnsuffix(const char* haystack, int haystack_size,
                              const char* needle, int needle_size) {
  if (needle_size > haystack_size) {
    return NULL;
  } else {
    const char* start = haystack + haystack_size - needle_size;
    if (strncmp(start, needle, needle_size) == 0) {
      return start;
    } else {
      return NULL;
    }
  }
}

const char* strncasesuffix(const char* haystack, int haystack_size,
                           const char* needle, int needle_size) {
  if (needle_size > haystack_size) {
    return NULL;
  } else {
    const char* start = haystack + haystack_size - needle_size;
    if (strncasecmp(start, needle, needle_size) == 0) {
      return start;
    } else {
      return NULL;
    }
  }
}

char* strchrnth(const char* str, const char& c, int n) {
  if (str == NULL)
    return NULL;
  if (n <= 0)
    return const_cast<char*>(str);
  const char* sp;
  int k = 0;
  for (sp = str; *sp != '\0'; sp ++) {
    if (*sp == c) {
      ++k;
      if (k >= n)
        break;
    }
  }
  return (k < n) ? NULL : const_cast<char*>(sp);
}

char* AdjustedLastPos(const char* str, char separator, int n) {
  if ( str == NULL )
    return NULL;
  const char* pos = NULL;
  if ( n > 0 )
    pos = strchrnth(str, separator, n);

  // if n <= 0 or separator appears fewer than n times, get the last occurrence
  if ( pos == NULL)
    pos = strrchr(str, separator);
  return const_cast<char*>(pos);
}


// ----------------------------------------------------------------------
// Misc. routines
// ----------------------------------------------------------------------

bool IsAscii(const char* str, int len) {
  const char* end = str + len;
  while (str < end) {
    if (!ascii_isascii(*str++)) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------
// StringReplace()
//    Give me a string and two patterns "old" and "new", and I replace
//    the first instance of "old" in the string with "new", if it
//    exists.  If "replace_all" is true then call this repeatedly until it
//    fails.  RETURN a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

string StringReplace(const StringPiece& s, const StringPiece& oldsub,
                     const StringPiece& newsub, bool replace_all) {
  string ret;
  StringReplace(s, oldsub, newsub, replace_all, &ret);
  return ret;
}


// ----------------------------------------------------------------------
// StringReplace()
//    Replace the "old" pattern with the "new" pattern in a string,
//    and append the result to "res".  If replace_all is false,
//    it only replaces the first instance of "old."
// ----------------------------------------------------------------------

void StringReplace(const StringPiece& s, const StringPiece& oldsub,
                   const StringPiece& newsub, bool replace_all,
                   string* res) {
  if (oldsub.empty()) {
    res->append(s.data(), s.length());  // If empty, append the given string.
    return;
  }

  StringPiece::size_type start_pos = 0;
  StringPiece::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == StringPiece::npos) {
      break;
    }
    res->append(s.data() + start_pos, pos - start_pos);
    res->append(newsub.data(), newsub.length());
    // Start searching again after the "old".
    start_pos = pos + oldsub.length();
  } while (replace_all);
  res->append(s.data() + start_pos, s.length() - start_pos);
}


// ----------------------------------------------------------------------
// gstrcasestr is a case-insensitive strstr. Eventually we should just
// use the GNU libc version of strcasestr, but it isn't compiled into
// RedHat Linux by default in version 6.1.
//
// This function uses ascii_tolower() instead of tolower(), for speed.
// ----------------------------------------------------------------------

char *gstrcasestr(const char* haystack, const char* needle) {
  char c, sc;
  size_t len;

  if ((c = *needle++) != 0) {
    c = ascii_tolower(c);
    len = strlen(needle);
    do {
      do {
        if ((sc = *haystack++) == 0)
          return NULL;
      } while (ascii_tolower(sc) != c);
    } while (strncasecmp(haystack, needle, len) != 0);
    haystack--;
  }
  // This is a const violation but strstr() also returns a char*.
  return const_cast<char*>(haystack);
}

// ----------------------------------------------------------------------
// gstrncasestr is a case-insensitive strnstr.
//    Finds the occurence of the (null-terminated) needle in the
//    haystack, where no more than len bytes of haystack is searched.
//    Characters that appear after a '\0' in the haystack are not searched.
//
// This function uses ascii_tolower() instead of tolower(), for speed.
// ----------------------------------------------------------------------
const char *gstrncasestr(const char* haystack, const char* needle, size_t len) {
  char c, sc;

  if ((c = *needle++) != 0) {
    c = ascii_tolower(c);
    size_t needle_len = strlen(needle);
    do {
      do {
        if (len-- <= needle_len
            || 0 == (sc = *haystack++))
          return NULL;
      } while (ascii_tolower(sc) != c);
    } while (strncasecmp(haystack, needle, needle_len) != 0);
    haystack--;
  }
  return haystack;
}

// ----------------------------------------------------------------------
// gstrncasestr is a case-insensitive strnstr.
//    Finds the occurence of the (null-terminated) needle in the
//    haystack, where no more than len bytes of haystack is searched.
//    Characters that appear after a '\0' in the haystack are not searched.
//
//    This function uses ascii_tolower() instead of tolower(), for speed.
// ----------------------------------------------------------------------
char *gstrncasestr(char* haystack, const char* needle, size_t len) {
  return const_cast<char *>(gstrncasestr(static_cast<const char *>(haystack),
                                         needle, len));
}
// ----------------------------------------------------------------------
// gstrncasestr_split performs a case insensitive search
// on (prefix, non_alpha, suffix).
// ----------------------------------------------------------------------
char *gstrncasestr_split(const char* str,
                         const char* prefix, char non_alpha,
                         const char* suffix,
                         size_t n) {
  uint32 prelen = prefix == NULL ? 0 : strlen(prefix);
  uint32 suflen = suffix == NULL ? 0 : strlen(suffix);

  // adjust the string and its length to avoid unnessary searching.
  // an added benefit is to avoid unnecessary range checks in the if
  // statement in the inner loop.
  if (suflen + prelen >= n)  return NULL;
  str += prelen;
  n -= prelen;
  n -= suflen;

  const char* where = NULL;

  // for every occurance of non_alpha in the string ...
  while ((where = static_cast<const char*>(
            memchr(str, non_alpha, n))) != NULL) {
    // ... test whether it is followed by suffix and preceded by prefix
    if ((!suflen || strncasecmp(where + 1, suffix, suflen) == 0) &&
        (!prelen || strncasecmp(where - prelen, prefix, prelen) == 0)) {
      return const_cast<char*>(where - prelen);
    }
    // if not, advance the pointer, and adjust the length according
    n -= (where + 1) - str;
    str = where + 1;
  }

  return NULL;
}

// ----------------------------------------------------------------------
// strcasestr_alnum is like a case-insensitive strstr, except that it
// ignores non-alphanumeric characters in both strings for the sake of
// comparison.
//
// This function uses ascii_isalnum() instead of isalnum() and
// ascii_tolower() instead of tolower(), for speed.
//
// E.g. strcasestr_alnum("i use google all the time", " !!Google!! ")
// returns pointer to "google all the time"
// ----------------------------------------------------------------------
char *strcasestr_alnum(const char *haystack, const char *needle) {
  const char *haystack_ptr;
  const char *needle_ptr;

  // Skip non-alnums at beginning
  while ( !ascii_isalnum(*needle) )
    if ( *needle++ == '\0' )
      return const_cast<char*>(haystack);
  needle_ptr = needle;

  // Skip non-alnums at beginning
  while ( !ascii_isalnum(*haystack) )
    if ( *haystack++ == '\0' )
      return NULL;
  haystack_ptr = haystack;

  while ( *needle_ptr != '\0' ) {
    // Non-alnums - advance
    while ( !ascii_isalnum(*needle_ptr) )
      if ( *needle_ptr++ == '\0' )
        return const_cast<char *>(haystack);

    while ( !ascii_isalnum(*haystack_ptr) )
      if ( *haystack_ptr++ == '\0' )
        return NULL;

    if ( ascii_tolower(*needle_ptr) == ascii_tolower(*haystack_ptr) ) {
      // Case-insensitive match - advance
      needle_ptr++;
      haystack_ptr++;
    } else {
      // No match - rollback to next start point in haystack
      haystack++;
      while ( !ascii_isalnum(*haystack) )
        if ( *haystack++ == '\0' )
          return NULL;
      haystack_ptr = haystack;
      needle_ptr = needle;
    }
  }
  return const_cast<char *>(haystack);
}


// ----------------------------------------------------------------------
// CountSubstring()
//    Return the number times a "substring" appears in the "text"
//    NOTE: this function's complexity is O(|text| * |substring|)
//          It is meant for short "text" (such as to ensure the
//          printf format string has the right number of arguments).
//          DO NOT pass in long "text".
// ----------------------------------------------------------------------
int CountSubstring(StringPiece text, StringPiece substring) {
  CHECK_GT(substring.length(), 0);

  int count = 0;
  StringPiece::size_type curr = 0;
  while (StringPiece::npos != (curr = text.find(substring, curr))) {
    ++count;
    ++curr;
  }
  return count;
}

// ----------------------------------------------------------------------
// strstr_delimited()
//    Just like strstr(), except it ensures that the needle appears as
//    a complete item (or consecutive series of items) in a delimited
//    list.
//
//    Like strstr(), returns haystack if needle is empty, or NULL if
//    either needle/haystack is NULL.
// ----------------------------------------------------------------------
const char* strstr_delimited(const char* haystack,
                             const char* needle,
                             char delim) {
  if (!needle || !haystack) return NULL;
  if (*needle == '\0') return haystack;

  int needle_len = strlen(needle);

  while (true) {
    // Skip any leading delimiters.
    while (*haystack == delim) ++haystack;

    // Walk down the haystack, matching every character in the needle.
    const char* this_match = haystack;
    int i = 0;
    for (; i < needle_len; i++) {
      if (*haystack != needle[i]) {
        // We ran out of haystack or found a non-matching character.
        break;
      }
      ++haystack;
    }

    // If we matched the whole needle, ensure that it's properly delimited.
    if (i == needle_len && (*haystack == '\0' || *haystack == delim)) {
      return this_match;
    }

    // No match. Consume non-delimiter characters until we run out of them.
    while (*haystack != delim) {
      if (*haystack == '\0') return NULL;
      ++haystack;
    }
  }
  LOG(FATAL) << "Unreachable statement";
  return NULL;
}


// ----------------------------------------------------------------------
// Older versions of libc have a buggy strsep.
// ----------------------------------------------------------------------

char* gstrsep(char** stringp, const char* delim) {
  char *s;
  const char *spanp;
  int c, sc;
  char *tok;

  if ((s = *stringp) == NULL)
    return NULL;

  tok = s;
  while (true) {
    c = *s++;
    spanp = delim;
    do {
      if ((sc = *spanp++) == c) {
        if (c == 0)
          s = NULL;
        else
          s[-1] = 0;
        *stringp = s;
        return tok;
      }
    } while (sc != 0);
  }

  return NULL; /* should not happen */
}

namespace strings {

// FindEol()
// Returns the location of the next end-of-line sequence.

StringPiece FindEol(StringPiece s) {
  for (size_t i = 0; i < s.length(); ++i) {
    if (s[i] == '\n') {
      return StringPiece(s.data() + i, 1);
    }
    if (s[i] == '\r') {
      if (i+1 < s.length() && s[i+1] == '\n') {
        return StringPiece(s.data() + i, 2);
      } else {
        return StringPiece(s.data() + i, 1);
      }
    }
  }
  return StringPiece(s.data() + s.length(), 0);
}

// ----------------------------------------------------------------------
// GlobalReplaceSubstring()
//    Replaces all instances of a substring in a string.  Does nothing
//    if 'substring' is empty.  Returns the number of replacements.
//
//    NOTE: The string pieces must not overlap s.
// ----------------------------------------------------------------------

int GlobalReplaceSubstring(const StringPiece& substring,
                           const StringPiece& replacement,
                           string* s) {
  CHECK(s != NULL);
  if (s->empty() || substring.empty())
    return 0;
  string tmp;
  int num_replacements = 0;
  int pos = 0;
  for (size_t match_pos = s->find(substring.data(), pos, substring.length());
       match_pos != string::npos;
       pos = match_pos + substring.length(),
           match_pos = s->find(substring.data(), pos, substring.length())) {
    ++num_replacements;
    // Append the original content before the match.
    tmp.append(*s, pos, match_pos - pos);
    // Append the replacement for the match.
    tmp.append(replacement.begin(), replacement.end());
  }
  // Append the content after the last match. If no replacements were made, the
  // original string is left untouched.
  if (num_replacements > 0) {
    tmp.append(*s, pos, s->length() - pos);
    s->swap(tmp);
  }
  return num_replacements;
}

bool ReplaceSuffix(StringPiece suffix, StringPiece new_suffix, string* str) {
  if (suffix.empty() || str->size() < suffix.size())
    return false;
  size_t pos = str->size() - suffix.size();
  if (str->compare(pos, suffix.size(), suffix.data()) == 0) {
    str->resize(pos);
    str->append(new_suffix.data(), new_suffix.size());
    return true;
  }
  return false;
}

}  // namespace strings

//------------------------------------------------------------------------
// OnlyWhitespace()
//  return true if string s contains only whitespace characters
//------------------------------------------------------------------------
bool OnlyWhitespace(const StringPiece& s) {
  for ( uint32 i = 0; i < s.size(); ++i ) {
    if ( !ascii_isspace(s[i]) ) return false;
  }
  return true;
}

#if 0
string PrefixSuccessor(const StringPiece& prefix) {
  // We can increment the last character in the string and be done
  // unless that character is 255, in which case we have to erase the
  // last character and increment the previous character, unless that
  // is 255, etc. If the string is empty or consists entirely of
  // 255's, we just return the empty string.
  bool done = false;
  string limit(prefix.data(), prefix.size());
  int index = limit.length() - 1;
  while (!done && index >= 0) {
    if (limit[index] == char(255)) {
      limit.erase(index);
      index--;
    } else {
      limit[index]++;
      done = true;
    }
  }
  if (!done) {
    return "";
  } else {
    return limit;
  }
}

string ImmediateSuccessor(const StringPiece& s) {
  // Return the input string, with an additional NUL byte appended.
  string out;
  out.reserve(s.size() + 1);
  out.append(s.data(), s.size());
  out.push_back('\0');
  return out;
}

void FindShortestSeparator(const StringPiece& start,
                           const StringPiece& limit,
                           string* separator) {
  // Find length of common prefix
  size_t min_length = std::min(start.size(), limit.size());
  size_t diff_index = 0;
  while ((diff_index < min_length) &&
         (start[diff_index] == limit[diff_index])) {
    diff_index++;
  }

  if (diff_index >= min_length) {
    // Handle the case where either string is a prefix of the other
    // string, or both strings are identical.
    start.CopyToString(separator);
    return;
  }

  if (diff_index+1 == start.size()) {
    // If the first difference is in the last character, do not bother
    // incrementing that character since the separator will be no
    // shorter than "start".
    start.CopyToString(separator);
    return;
  }

  if (start[diff_index] == char(0xff)) {
    // Avoid overflow when incrementing start[diff_index]
    start.CopyToString(separator);
    return;
  }

  separator->assign(start.data(), diff_index);
  separator->push_back(start[diff_index] + 1);
  if (*separator >= limit) {
    // Never pick a separator that causes confusion with "limit"
    start.CopyToString(separator);
  }
}
#endif
int SafeSnprintf(char *str, size_t size, const char *format, ...) {
  va_list printargs;
  va_start(printargs, format);
  int ncw = vsnprintf(str, size, format, printargs);
  va_end(printargs);
  return (ncw >= 0 && size_t(ncw) < size) ? ncw : 0;
}
