#ifndef TARAXA_NODE_TRX_ENGINE_TYPES_HPP_
#define TARAXA_NODE_TRX_ENGINE_TYPES_HPP_

#include <json/value.h>
#include <libethcore/Common.h>
#include <libethcore/LogEntry.h>
#include <libethereum/Transaction.h>
#include <libethereum/TransactionReceipt.h>

#include <unordered_map>

#include "../types.hpp"

namespace taraxa::trx_engine::types {
using dev::Address;
using dev::bytes;
using dev::h256;
using dev::u256;
using dev::eth::LogBloom;
using dev::eth::TransactionReceipts;
using dev::eth::Transactions;
using std::optional;
using std::string;
using std::unordered_map;
using std::vector;

struct TransactionOutput {
  bytes returnValue;
  string error;

  static TransactionOutput fromJson(Json::Value const& json);
};

struct StateTransitionResult {
  h256 stateRoot;
  u256 usedGas;
  TransactionReceipts receipts;
  vector<TransactionOutput> transactionOutputs;
  unordered_map<Address, u256> updatedBalances;

  static StateTransitionResult fromJson(Json::Value const& json);
};

struct Block {
  u256 const& number;
  Address const& miner;
  int64_t time;
  u256 const& difficulty;
  uint64_t gasLimit;
  Transactions const& transactions;

  Json::Value toJson() const;
};

struct StateTransitionRequest {
  h256 const& stateRoot;
  Block block;

  Json::Value toJson() const;
};

}  // namespace taraxa::trx_engine::types

#endif  // TARAXA_NODE_TRX_ENGINE_TYPES_HPP_
