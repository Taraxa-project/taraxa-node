#pragma once

#include <libdevcore/Common.h>

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
constexpr uint16_t kMaxLevelsPerPeriod = 100;
constexpr uint32_t kDagExpiryLevelLimit = 1000;

const uint32_t kMaxVotesInPacket{1000};
const uint32_t kMaxTransactionsInPacket{1000};

const uint32_t kPeriodicEventsThreadCount{2};

const uint64_t kMinTxGas{21000};

// The various denominations; here for ease of use where needed within code.
static const u256 kOneTara = dev::exp10<18>();
// static const u256 kFinney = exp10<15>();
// static const u256 kSzabo = exp10<12>();
// static const u256 kShannon = dev::exp10<9>();
// static const u256 kWei = exp10<0>();

}  // namespace taraxa