#pragma once

#include "common/global_const.hpp"
#include "common/types.hpp"

namespace taraxa {

GLOBAL_CONST(h256, ZeroHash);
GLOBAL_CONST(h256, EmptySHA3);
GLOBAL_CONST(h256, EmptyRLPListSHA3);
GLOBAL_CONST(h64, EmptyNonce);
GLOBAL_CONST(u256, ZeroU256);

static const blk_hash_t kNullBlockHash = blk_hash_t(0);

constexpr uint16_t kOnePercent = 100;
constexpr uint64_t kOneTara = 1e18;
constexpr uint16_t kMaxLevelsPerPeriod = 100;
}  // namespace taraxa