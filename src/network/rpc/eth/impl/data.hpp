#pragma once

#include "final_chain/data.hpp"

namespace taraxa::net::rpc::eth {
using namespace ::dev;
using namespace ::taraxa::final_chain;

struct TransactionLocationWithBlockHash : TransactionLocation {
  h256 blk_h{};
};

struct LocalisedTransaction {
  Transaction trx{};
  optional<TransactionLocationWithBlockHash> trx_loc{};
};

struct ExtendedTransactionLocation : TransactionLocationWithBlockHash {
  h256 trx_hash{};
};

struct LocalisedTransactionReceipt {
  TransactionReceipt r;
  ExtendedTransactionLocation trx_loc;
  addr_t trx_from;
  optional<addr_t> trx_to;
};

struct LocalisedLogEntry {
  LogEntry le;
  ExtendedTransactionLocation trx_loc;
  uint64_t position_in_receipt = 0;
};

struct TransactionSkeleton {
  Address from;
  u256 value;
  bytes data;
  optional<Address> to;
  optional<uint64_t> nonce;
  optional<uint64_t> gas;
  optional<u256> gas_price;
};

}  // namespace taraxa::net::rpc::eth