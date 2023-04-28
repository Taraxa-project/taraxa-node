
#pragma once

#include <json/value.h>

#include <future>
#include <memory>
#include <stdexcept>

#include "DebugFace.h"
#include "node/node.hpp"

namespace dev::eth {
class Client;
}

namespace taraxa::net {

class InvalidAddress : public std::exception {
 public:
  virtual const char* what() const noexcept { return "Invalid account address"; }
};

class InvalidTracingParams : public std::exception {
 public:
  virtual const char* what() const noexcept { return "Invalid tracing params"; }
};

class Debug : public DebugFace {
 public:
  explicit Debug(const std::shared_ptr<taraxa::FullNode>& _full_node, uint64_t gas_limit)
      : full_node_(_full_node), kGasLimit(gas_limit) {}
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"debug", "1.0"}}; }

  virtual Json::Value debug_traceTransaction(const std::string& param1) override;
  virtual Json::Value debug_traceCall(const Json::Value& param1, const std::string& param2) override;
  virtual Json::Value trace_call(const Json::Value& param1, const Json::Value& param2,
                                 const std::string& param3) override;
  virtual Json::Value trace_replayTransaction(const std::string& param1, const Json::Value& param2) override;
  virtual Json::Value trace_replayBlockTransactions(const std::string& param1, const Json::Value& param2) override;

 private:
  state_api::EVMTransaction to_eth_trx(std::shared_ptr<Transaction> t) const;
  state_api::EVMTransaction to_eth_trx(const Json::Value& json, EthBlockNumber blk_num);
  EthBlockNumber parse_blk_num(const string& blk_num_str);
  state_api::Tracing parse_tracking_parms(const Json::Value& json) const;
  Address to_address(const string& s) const;
  std::pair<std::shared_ptr<Transaction>, std::optional<final_chain::TransactionLocation>>
  get_transaction_with_location(const std::string& transaction_hash) const;

  std::weak_ptr<taraxa::FullNode> full_node_;
  const uint64_t kGasLimit = ((uint64_t)1 << 53) - 1;
};

}  // namespace taraxa::net