// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "Common.h"

#include "Exceptions.h"
#include "Log.h"

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace std;

namespace dev {
bytes const NullBytes;
std::string const EmptyString;

TimerHelper::~TimerHelper() {
  auto e = std::chrono::high_resolution_clock::now() - m_t;
  if (!m_ms || e > chrono::milliseconds(m_ms))
    clog(VerbosityDebug, "timer") << m_id << " " << chrono::duration_cast<chrono::milliseconds>(e).count() << " ms";
}

int64_t utcTime() {
  // TODO: Fix if possible to not use time(0) and merge only after testing in
  // all platforms time_t t = time(0); return mktime(gmtime(&t));
  return time(0);
}

}  // namespace dev
