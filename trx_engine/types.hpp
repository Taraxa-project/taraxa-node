#ifndef TARAXA_NODE_TRX_ENGINE_TYPES_HPP_
#define TARAXA_NODE_TRX_ENGINE_TYPES_HPP_

#include <json/value.h>
#include <libethcore/Common.h>
#include <unordered_map>
#include "../types.hpp"

namespace taraxa::trx_engine::types {
using dev::Address;
using dev::bytes;
using dev::h256;
using dev::u256;
using dev::eth::LogBloom;
using std::optional;
using std::string;
using std::unordered_map;
using std::vector;

struct LogEntry {
  Address address;
  vector<h256> topics;
  bytes data;
  u256 blockNumber;
  h256 transactionHash;
  int transactionIndex;
  h256 blockHash;
  int logIndex;

  void fromJson(Json::Value const&);
};

struct EthTransactionReceipt {
  optional<bytes> root;
  optional<int> status;
  u256 cumulativeGasUsed;
  LogBloom logsBloom;
  vector<LogEntry> logs;
  h256 transactionHash;
  Address contractAddress;
  u256 gasUsed;

  void fromJson(Json::Value const&);
};

struct TaraxaTransactionReceipt {
  bytes returnValue;
  EthTransactionReceipt ethereumReceipt;
  string error;

  void fromJson(Json::Value const&);
};

struct StateTransitionResult {
  h256 stateRoot;
  vector<TaraxaTransactionReceipt> receipts;
  vector<LogEntry> allLogs;
  u256 usedGas;
  unordered_map<Address, u256> updatedBalances;

  void fromJson(Json::Value const&);
};

struct Transaction {
  optional<Address> to;
  Address from;
  uint64_t nonce;
  u256 value;
  uint64_t gas;
  u256 gasPrice;
  bytes input;
  h256 hash;

  Json::Value toJson() const;
};

struct Block {
  u256 number;
  Address miner;
  int64_t time;
  u256 difficulty;
  uint64_t gasLimit;
  h256 hash;
  vector<Transaction> transactions;

  Json::Value toJson() const;
};

struct StateTransitionRequest {
  h256 stateRoot;
  Block block;

  Json::Value toJson() const;
};

}  // namespace taraxa::trx_engine::types

#endif  // TARAXA_NODE_TRX_ENGINE_TYPES_HPP_
