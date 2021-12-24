#pragma once

#include "common/global_const.hpp"
#include "common/types.hpp"

namespace taraxa {

GLOBAL_CONST(h256, ZeroHash);
GLOBAL_CONST(h256, EmptySHA3);
GLOBAL_CONST(h256, EmptyRLPListSHA3);
GLOBAL_CONST(h64, EmptyNonce);
GLOBAL_CONST(u256, ZeroU256);

constexpr uint16_t kOnePercent = 100;
}  // namespace taraxa