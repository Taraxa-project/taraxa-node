#pragma once

#include <libdevcore/Common.h>
#include <libdevcore/Guards.h>
#include <libdevcore/Log.h>
#include <libdevcore/SHA3.h>

#include <algorithm>

#include "common/types.hpp"
#include "transaction_manager/transaction.hpp"
#include "util/encoding_rlp.hpp"

namespace taraxa::final_chain {

inline static h256 const ZeroHash;
inline static auto const EmptySHA3 = dev::sha3(dev::bytesConstRef());
inline static auto const EmptyListSHA3 = dev::sha3(dev::RLPStream(0).out());

/// The log bloom's size (2048-bit).
using LogBloom = dev::h2048;
using LogBlooms = std::vector<LogBloom>;
using Nonce = dev::h64;
using BlockNumber = uint64_t;

struct BlockHeader {
  h256 hash;
  h256 parent_hash;
  h256 state_root;
  h256 transactions_root;
  h256 receipts_root;
  LogBloom log_bloom;
  BlockNumber number = 0;
  uint64_t gas_limit = 0;
  uint64_t gas_used = 0;
  bytes extra_data;
  uint64_t timestamp = 0;
  Address author;

  uint64_t ethereum_rlp_size = 0;

  RLP_FIELDS(hash, ethereum_rlp_size, parent_hash, author, state_root, transactions_root, receipts_root, log_bloom,
             number, gas_limit, gas_used, timestamp, extra_data)

  static h256 const& uncles() { return EmptyListSHA3; }
  static Nonce const& nonce() {
    static const Nonce ret;
    return ret;
  }
  static u256 const& difficulty() {
    static const u256 ret = 0;
    return ret;
  }
  static h256 const& mix_hash() {
    static const h256 ret;
    return ret;
  }

  void ethereum_rlp(dev::RLPStream& encoding) const {
    util::rlp_tuple(encoding, parent_hash, BlockHeader::uncles(), author, state_root, transactions_root, receipts_root,
                    log_bloom, BlockHeader::difficulty(), number, gas_limit, gas_used, timestamp, extra_data,
                    BlockHeader::mix_hash(), BlockHeader::nonce());
  }
};

static constexpr auto c_bloomIndexSize = 16;
static constexpr auto c_bloomIndexLevels = 2;

using BlockLogBlooms = LogBlooms;
using BlocksBlooms = std::array<LogBloom, c_bloomIndexSize>;

struct LogEntry {
  Address address;
  h256s topics;
  bytes data;

  RLP_FIELDS(address, topics, data)

  auto bloom() const {
    LogBloom ret;
    ret.shiftBloom<3>(sha3(address.ref()));
    for (auto t : topics) {
      ret.shiftBloom<3>(sha3(t.ref()));
    }
    return ret;
  }
};

using LogEntries = std::vector<LogEntry>;

struct TransactionReceipt {
  uint8_t status_code = 0;
  uint64_t gas_used = 0;
  uint64_t cumulative_gas_used = 0;
  LogEntries logs;
  std::optional<Address> new_contract_address;

  RLP_FIELDS(status_code, gas_used, cumulative_gas_used, logs, new_contract_address)

  auto bloom() const {
    LogBloom ret;
    for (auto const& l : logs) {
      ret |= l.bloom();
    }
    return ret;
  }
};

using TransactionReceipts = std::vector<TransactionReceipt>;

struct TransactionLocation {
  BlockNumber blk_n = 0;
  uint64_t index = 0;

  RLP_FIELDS(blk_n, index)
};

}  // namespace taraxa::final_chain
