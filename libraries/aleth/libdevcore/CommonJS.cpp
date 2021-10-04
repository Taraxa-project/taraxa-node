// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "CommonJS.h"

using namespace std;

namespace dev {

bytes jsToBytes(string const& _s, OnFailed _f) {
  try {
    return fromHex(_s, WhenError::Throw);
  } catch (...) {
    if (_f == OnFailed::InterpretRaw)
      return asBytes(_s);
    else if (_f == OnFailed::Throw)
      throw invalid_argument("Cannot intepret '" + _s + "' as bytes; must be 0x-prefixed hex or decimal.");
  }
  return bytes();
}

}  // namespace dev
