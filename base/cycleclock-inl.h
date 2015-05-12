// Copyright (C) 1999-2007 Google, Inc.
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
// All rights reserved.
// Extracted from base/timer.h by jrvb

// The implementation of CycleClock::Now()
// See cycleclock.h
//
// IWYU pragma: private, include "base/cycleclock.h"

// NOTE: only i386 and x86_64 have been well tested.
// PPC, sparc, alpha, and ia64 are based on
//    http://peter.kuscsik.com/wordpress/?p=14
// with modifications by m3b.  See also
//    https://setisvn.ssl.berkeley.edu/svn/lib/fftw-3.0.1/kernel/cycle.h

#ifndef SUPERSONIC_OPENSOURCE_AUXILIARY_CYCLECLOCK_INL_H_
#define SUPERSONIC_OPENSOURCE_AUXILIARY_CYCLECLOCK_INL_H_

#include <sys/time.h>

// Please do not nest #if directives.  Keep one section, and one #if per
// platform.

// For historical reasons, the frequency on some platforms is scaled to be
// close to the platform's core clock frequency.  This is not guaranteed by the
// interface, and may change in future implementations.

// ----------------------------------------------------------------
#if defined(__APPLE__)
#error do not support
// ----------------------------------------------------------------
#elif defined(__i386__)
#error do not support
// ----------------------------------------------------------------
#elif defined(__x86_64__) || defined(__amd64__)
inline uint64 CycleClock::Now() {
  uint64 low, high;
  __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
  return (high << 32) | low;
}

// ----------------------------------------------------------------
#else
// The soft failover to a generic implementation is automatic only for some
// platforms.  For other platforms the developer is expected to make an attempt
// to create a fast implementation and use generic version if nothing better is
// available.
#error You need to define CycleTimer for your O/S and CPU
#endif

#endif  // SUPERSONIC_OPENSOURCE_AUXILIARY_CYCLECLOCK_INL_H_
