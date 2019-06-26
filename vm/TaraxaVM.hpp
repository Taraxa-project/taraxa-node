#ifndef TARAXA_NODE_TARAXAVM_HPP
#define TARAXA_NODE_TARAXAVM_HPP

#include <libethcore/Common.h>
#include <boost/property_tree/ptree.hpp>
#include <map>
#include <string>
#include "types.hpp"
extern "C" {
#include "cgo_generated/taraxa_vm_cgo.h"
}

namespace taraxa::vm {

//struct PtreeSerializable {
//  boost::property_tree::ptree toPtree() const {
//    throw std::runtime_error("not implemented");
//  };
//};
//
// struct PtreeDeserializable {
//  void fromPtree(const boost::property_tree::ptree &ptree) {
//    throw std::runtime_error("not implemented");
//  };
//};

struct DBConfig {
  std::string type;
  boost::property_tree::ptree options;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &ptree);
};

struct StateDBConfig {
  DBConfig db;
  int cacheSize;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &ptree);
};

struct VmConfig {
  StateDBConfig readDB;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &ptree);
};

struct LogEntry {
  dev::Address address;
  std::vector<dev::h256> topics;
  dev::bytes data;
  int64_t blockNumber;
  dev::h256 transactionHash;
  uint transactionIndex;
  dev::h256 blockHash;
  uint logIndex;
  bool removed;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &p);
};

struct EthTransactionReceipt {
  std::optional<dev::bytes> root;
  std::optional<int64_t> status;
  dev::u256 cumulativeGasUsed;
  dev::eth::LogBloom logsBloom;
  std::vector<LogEntry> logs;
  dev::h256 transactionHash;
  dev::Address contractAddress;
  dev::u256 gasUsed;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &p);
};

struct TaraxaTransactionReceipt {
  dev::bytes returnValue;
  EthTransactionReceipt ethereumReceipt;
  std::string contractError;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &p);
};

struct StateTransitionResult {
  dev::h256 stateRoot;
  std::vector<TaraxaTransactionReceipt> receipts;
  std::vector<LogEntry> allLogs;
  dev::u256 usedGas;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &p);
};

struct Transaction {
  std::optional<dev::Address> to;
  dev::Address from;
  dev::u256 nonce;
  dev::u256 amount;
  dev::u256 gasLimit;
  dev::u256 gasPrice;
  dev::bytes data;
  dev::h256 hash;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &ptree);
};

struct Block {
  dev::Address coinbase;
  dev::u256 number;
  dev::u256 difficulty;
  int64_t time;
  dev::u256 gasLimit;
  dev::h256 hash;
  std::vector<Transaction> transactions;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &ptree);
};

struct StateTransitionRequest {
  dev::h256 stateRoot;
  Block block;

  boost::property_tree::ptree toPtree() const;
  void fromPtree(const boost::property_tree::ptree &ptree);
};

class TaraxaVM {
  std::string goAddress;

 public:
  explicit TaraxaVM(std::string goAddress);

  static std::shared_ptr<TaraxaVM> fromConfig(const VmConfig &config);
  StateTransitionResult transitionState(const StateTransitionRequest &req);
};

}  // namespace taraxa::vm

#endif  // TARAXA_NODE_TARAXAVM_HPP
