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

inline static auto const EmptySHA3 = dev::sha3(dev::bytesConstRef());
inline static auto const EmptyListSHA3 = dev::sha3(dev::RLPStream(0).out());

/// The log bloom's size (2048-bit).
using LogBloom = dev::h2048;
/// Many log blooms.
using LogBlooms = std::vector<LogBloom>;
using Nonce = dev::h64;
using BlockNumber = uint64_t;
using TransactionHashes = h256s;

struct TransactionSkeleton {
  dev::Address from;
  dev::u256 value;
  dev::bytes data;
  std::optional<dev::Address> to;
  std::optional<uint64_t> nonce;
  std::optional<uint64_t> gas;
  std::optional<dev::u256> gas_price;
};

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

  template <typename E>
  inline void rlp(E encoding) {
    util::rlp_tuple(encoding, hash, ethereum_rlp_size, parent_hash, author, state_root, transactions_root,
                    receipts_root, log_bloom, number, gas_limit, gas_used, timestamp, extra_data);
  }

  void ethereum_rlp(dev::RLPStream& encoding) {
    util::rlp_tuple(encoding, parent_hash, BlockHeader::uncles(), author, state_root, transactions_root, receipts_root,
                    log_bloom, BlockHeader::difficulty(), number, gas_limit, gas_used, timestamp, extra_data,
                    BlockHeader::mix_hash(), BlockHeader::nonce());
  }
};

struct BlockHeaderWithTransactions : BlockHeader {
  BlockHeader h;
  std::variant<TransactionHashes, Transactions> trxs;
};

static const unsigned c_bloomIndexSize = 16;
static const unsigned c_bloomIndexLevels = 2;

using BlockLogBlooms = LogBlooms;
using BlocksBlooms = std::array<LogBloom, c_bloomIndexSize>;

struct TransactionLocation {
  h256 blk_h;
  uint64_t index = 0;

  template <typename E>
  inline void rlp(E encoding) {
    util::rlp_tuple(encoding, blk_h, index);
  }
};

struct LogEntry {
  Address address;
  h256s topics;
  bytes data;

  template <typename E>
  inline void rlp(E encoding) {
    util::rlp_tuple(encoding, address, topics, data);
  }

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
  Address contract_address;

  template <typename E>
  inline void rlp(E encoding) {
    util::rlp_tuple(encoding, status_code, gas_used, cumulative_gas_used, logs, contract_address);
  }

  auto bloom() const {
    LogBloom ret;
    for (auto const& l : logs) {
      ret |= l.bloom();
    }
    return ret;
  }
};

using TransactionReceipts = std::vector<TransactionReceipt>;

struct ExtendedTransactionLocation : TransactionLocation {
  BlockNumber blk_n;
  h256 trx_hash;
};

struct LocalisedTransaction {
  Transaction trx;
  ExtendedTransactionLocation trx_loc;
};

struct LocalisedTransactionReceipt {
  TransactionReceipt r;
  ExtendedTransactionLocation trx_loc;
  addr_t trx_from;
  std::optional<addr_t> trx_to;
};

struct LocalisedLogEntry {
  LogEntry le;
  ExtendedTransactionLocation trx_loc;
  uint position_in_receipt;
};

using LocalisedLogEntries = std::vector<LocalisedLogEntry>;

}  // namespace taraxa::final_chain
