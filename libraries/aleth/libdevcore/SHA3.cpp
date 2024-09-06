// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#include "SHA3.h"

#include <ethash/keccak.hpp>

#include "ethash/hash_types.hpp"

namespace dev {

bool sha3(bytesConstRef _input, bytesRef o_output) noexcept {
  if (o_output.size() != 32) return false;
  ethash::hash256 h = ethash::keccak256(_input.data(), _input.size());
  bytesConstRef{h.bytes, 32}.copyTo(o_output);
  return true;
}
}  // namespace dev
