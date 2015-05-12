// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: tomasz.kaftal@gmail.com (Tomasz Kaftal)
//
// The implementation of walltime functionalities.
#ifndef _GNU_SOURCE   // gcc3 at least defines it on the command line
#define _GNU_SOURCE   // Linux wants that for strptime in time.h
#endif

#define __STDC_FORMAT_MACROS 1

#include "base/walltime.h"
#include "base/logging.h"

#include <cstdio>

using std::string;

// This is exactly like mktime() except it is guaranteed to return -1 on
// failure.  Some versions of glibc allow mktime() to return negative
// values which the standard says are undefined.  See the standard at
// http://www.opengroup.org/onlinepubs/007904875/basedefs/xbd_chap04.html
// under the heading "Seconds Since the Epoch".
static inline time_t gmktime(struct tm *tm) {
  time_t rt = mktime(tm);
  return rt < 0 ? time_t(-1) : rt;
}

static void StringAppendStrftime(string* dst,
                                 const char* format,
                                 const struct tm* tm) {
  char space[1024];

  size_t result = strftime(space, sizeof(space), format, tm);

  if (result < sizeof(space)) {
    // It fit
    dst->append(space, result);
    return;
  }

  size_t length = sizeof(space);
  for (int sanity = 0; sanity < 5; ++sanity) {
    length *= 2;
    char* buf = new char[length];

    result = strftime(buf, length, format, tm);
    if (result < length) {
      // It fit
      dst->append(buf, result);
      delete[] buf;
      return;
    }

    delete[] buf;
  }

  // sanity failure
  return;
}

// Convert a "struct tm" interpreted as *GMT* into a time_t (technically
// a long since we can't include header files in header files bla bla bla).
// This is basically filling a hole in the standard library.
//
// There are several approaches to mkgmtime() implementation on the net,
// many of them wrong.  Simply reimplementing the logic seems to be the
// simplest and most efficient, though it does reimplement calendar logic.
// The calculation is mostly straightforward; leap years are the main issue.
//
// Like gmktime() this method returns -1 on failure. Negative results
// are considered undefined by the standard so these cases are
// considered failures and thus return -1.
time_t mkgmtime(const struct tm *tm) {
  // Month-to-day offset for non-leap-years.
  static const int month_day[12] =
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  // Most of the calculation is easy; leap years are the main difficulty.
  int month = tm->tm_mon % 12;
  int year = tm->tm_year + tm->tm_mon / 12;
  if (month < 0) {   // Negative values % 12 are still negative.
    month += 12;
    --year;
  }

  // This is the number of Februaries since 1900.
  const int year_for_leap = (month > 1) ? year + 1 : year;

  time_t rt = tm->tm_sec                           // Seconds
       + 60 * (tm->tm_min                          // Minute = 60 seconds
       + 60 * (tm->tm_hour                         // Hour = 60 minutes
       + 24 * (month_day[month] + tm->tm_mday - 1  // Day = 24 hours
       + 365 * (year - 70)                         // Year = 365 days
       + (year_for_leap - 69) / 4                  // Every 4 years is leap...
       - (year_for_leap - 1) / 100                 // Except centuries...
       + (year_for_leap + 299) / 400)));           // Except 400s.
  return rt < 0 ? -1 : rt;
}

bool WallTime_Parse_Timezone(const char* time_spec,
                             const char* format,
                             const struct tm* default_time,
                             bool local,
                             WallTime* result) {
  struct tm split_time;
  if (default_time) {
     split_time = *default_time;
  } else {
     memset(&split_time, 0, sizeof(split_time));
  }
  const char* parsed = strptime(time_spec, format, &split_time);
  if (parsed == NULL) return false;

  // If format ends with "%S", match fractional seconds
  double fraction = 0.0;
  char junk;
  if ((*parsed == '.') &&
     (strcmp(format + strlen(format) - 2, "%S") == 0) &&
     (sscanf(parsed, "%lf%c",  // NOLINT(runtime/printf)
             &fraction, &junk) == 1)) {
     parsed = format + strlen(format);   // Parsed it all!
  }
  if (*parsed != '\0') return false;

  // Convert into seconds since epoch.  Adjust so it is interpreted
  // w.r.t. the daylight-saving-state at the specified time.
  split_time.tm_isdst = -1;     // Ask gmktime() to find dst imfo
  time_t ptime;
  if (local) {
    ptime = gmktime(&split_time);
  } else {
    ptime = mkgmtime(&split_time);  // Returns time in GMT instead of local.
  }

  if (ptime == -1) return false;

  *result = ptime;
  *result += fraction;
  return true;
}

WallTime WallTime_Now() {
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec + ts.tv_nsec / static_cast<double>(1e9);
}

int32 GetDaysSinceEpoch(const char* date) {
  struct tm time;
  memset(&time, 0, sizeof(time));
  CHECK(strptime(date, "%Y-%m-%d", &time)) << "Could not parse date: " << date;
  int64 seconds_since_epoch = gmktime(&time);
  return seconds_since_epoch > 0 ? seconds_since_epoch / (60 * 60 * 24) : -1;
}

void StringAppendStrftime(string* dst,
                          const char* format,
                          time_t when,
                          bool local) {
  struct tm tm;
  bool conversion_error;
  if (local) {
    conversion_error = (localtime_r(&when, &tm) == NULL);
  } else {
    conversion_error = (gmtime_r(&when, &tm) == NULL);
  }
  if (conversion_error) {
    // If we couldn't convert the time, don't append anything.
    return;
  }
  StringAppendStrftime(dst, format, &tm);
}

std::string PrintLocalTime(uint64 seconds_epoch, const char* format) {
  time_t seconds = seconds_epoch;
  struct tm ts;
  char buf[512] = {0};

  localtime_r(&seconds, &ts);
  strftime(buf, sizeof(buf), format, &ts);
  return buf;
}

std::string GetTimerString(uint64 seconds) {
  char buf[128];
  uint32 hours = seconds / 3600;
  seconds = seconds % 3600;
  uint32 mins = seconds /60;
  uint32 secs = seconds % 60;
  snprintf(buf, sizeof buf, "%" PRIu32 ":%" PRIu32 ":%" PRIu32, hours, mins, secs);
  return buf;
}

void SleepForMilliseconds(uint32 milliseconds) {
 // Sleep for a few milliseconds
 struct timespec sleep_time;
 sleep_time.tv_sec = milliseconds / 1000;
 sleep_time.tv_nsec = (milliseconds % 1000) * 1000000;
 while (nanosleep(&sleep_time, &sleep_time) != 0 && errno == EINTR)
   ;  // Ignore signals and wait for the full interval to elapse.
}

static uint64 TimeSpecDiff(const struct timespec* from, const struct timespec *ts1) {
  uint64 nanos = (from->tv_sec - ts1->tv_sec) * 1000000000LL;
  nanos += (from->tv_nsec - ts1->tv_nsec);
  return nanos;
}

uint64 CycleClock::CycleFreq() {
  static uint64 freq = 0;
  if (freq) {
    return freq;
  }
  cpu_set_t cpuMask, old_mask;
  CPU_ZERO(&cpuMask);
  CPU_SET(0, &cpuMask);

  sched_getaffinity(0, sizeof(old_mask), &old_mask);
  sched_setaffinity(0, sizeof(cpuMask), &cpuMask);
  struct timespec begints, endts;
  clock_gettime(CLOCK_MONOTONIC, &begints);
  uint64 begin = Now();
  for (uint64_t i = 0; i < 10000000LL; i++) {
    asm("");
  }
  uint64 end = Now();
  clock_gettime(CLOCK_MONOTONIC, &endts);
  uint64 nsec_elapsed = TimeSpecDiff(&endts, &begints);
  freq = uint64(double(end - begin)/(double(nsec_elapsed)*1e-9));
  sched_setaffinity(0, sizeof(old_mask), &old_mask);
  return freq;
}

namespace base {

int TimezoneDiff(const char* tm_zone) {
  time_t now_utc = time(nullptr);
  struct tm tm_utc;
  char *tz = getenv("TZ");
  setenv("TZ", tm_zone, 1);
  tzset();
  gmtime_r(&now_utc, &tm_utc);
  tm_utc.tm_isdst = -1;  // Ask for help for DST.
  time_t there = mktime(&tm_utc);
  if (tz)
    setenv("TZ", tz, 1);
  else
    unsetenv("TZ");
  tzset();
  return (now_utc - there) / 3600;
}

}  // namespace base