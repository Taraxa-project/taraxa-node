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
constexpr uint64_t k_threshold_testnet_hard_fork_period = 110000;
constexpr uint64_t k_testnet_hardfork2_block_num = 155350;
constexpr uint64_t k_testnet_hardfork3_block_num = 219700;

}  // namespace taraxa