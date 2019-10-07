#ifndef TARAXA_NODE_TRX_ENGINE_TYPES_HPP_
#define TARAXA_NODE_TRX_ENGINE_TYPES_HPP_

#include <json/value.h>
#include <libethcore/Common.h>
#include <unordered_map>
#include "../types.hpp"

namespace taraxa::trx_engine {

struct LogEntry {
  dev::Address address;
  std::vector<dev::h256> topics;
  dev::bytes data;
  dev::u256 blockNumber;
  dev::h256 transactionHash;
  int transactionIndex;
  dev::h256 blockHash;
  int logIndex;

  void fromJson(Json::Value const&);
};

struct EthTransactionReceipt {
  std::optional<dev::bytes> root;
  std::optional<int> status;
  dev::u256 cumulativeGasUsed;
  dev::eth::LogBloom logsBloom;
  std::vector<LogEntry> logs;
  dev::h256 transactionHash;
  dev::Address contractAddress;
  dev::u256 gasUsed;

  void fromJson(Json::Value const&);
};

struct TaraxaTransactionReceipt {
  dev::bytes returnValue;
  EthTransactionReceipt ethereumReceipt;
  std::string error;

  void fromJson(Json::Value const&);
};

struct StateTransitionResult {
  dev::h256 stateRoot;
  std::vector<TaraxaTransactionReceipt> receipts;
  std::vector<LogEntry> allLogs;
  dev::u256 usedGas;
  std::unordered_map<dev::Address, dev::u256> updatedBalances;

  void fromJson(Json::Value const&);
};

struct Transaction {
  std::optional<dev::Address> to;
  dev::Address from;
  uint64_t nonce;
  dev::u256 value;
  uint64_t gas;
  dev::u256 gasPrice;
  dev::bytes input;
  dev::h256 hash;

  Json::Value toJson() const;
};

struct Block {
  dev::u256 number;
  dev::Address miner;
  int64_t time;
  dev::u256 difficulty;
  uint64_t gasLimit;
  dev::h256 hash;
  std::vector<Transaction> transactions;

  Json::Value toJson() const;
};

struct StateTransitionRequest {
  dev::h256 stateRoot;
  Block block;

  Json::Value toJson() const;
};

}  // namespace taraxa::trx_engine

#endif  // TARAXA_NODE_TRX_ENGINE_TYPES_HPP_
