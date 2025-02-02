#pragma once

#include <libdevcore/Common.h>

#include "common/global_const.hpp"
#include "common/types.hpp"

namespace taraxa {

GLOBAL_CONST(h256, ZeroHash);
GLOBAL_CONST(h256, EmptyRLPListSHA3);
GLOBAL_CONST(h64, EmptyNonce);
GLOBAL_CONST(u256, ZeroU256);

static const blk_hash_t kNullBlockHash;

constexpr uint16_t kOnePercent = 100;
constexpr uint16_t kMaxLevelsPerPeriod = 100;
constexpr uint32_t kDagExpiryLevelLimit = 1000;
constexpr uint32_t kDagBlockMaxTips = 16;

const uint32_t kMaxTransactionsInPacket{500};
const uint32_t kMaxTransactionsSizeInPacket{500000};
const uint32_t kMaxHashesInPacket{5000};

const uint32_t kPeriodicEventsThreadCount{2};

const uint64_t kMinTxGas{21000};

constexpr uint32_t kMinTransactionPoolSize{30000};
constexpr uint32_t kDefaultTransactionPoolSize{200000};
constexpr uint32_t kMaxNonFinalizedTransactions{1000000};
constexpr uint32_t kMaxNonFinalizedDagBlocks{100};

const size_t kV3NetworkVersion = 3;

const uint32_t kRecentlyFinalizedTransactionsFactor = 2;

constexpr uint64_t kTxGas = 21000;
constexpr uint64_t kTxGasContractCreation = 53000;
constexpr uint64_t kTxDataZeroGas = 4;
constexpr uint64_t kTxDataNonZeroGas = 68;

// The various denominations; here for ease of use where needed within code.
static const u256 kOneTara = dev::exp10<18>();
// static const u256 kFinney = exp10<15>();
// static const u256 kSzabo = exp10<12>();
// static const u256 kShannon = dev::exp10<9>();
// static const u256 kWei = exp10<0>();

static const addr_t kTaraxaSystemAccount = addr_t("0x0054617261786153797374656d4163636f756e74");
}  // namespace taraxa