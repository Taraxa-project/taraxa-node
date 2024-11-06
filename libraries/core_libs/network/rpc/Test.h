#pragma once

#include <future>

#include "TestFace.h"
#include "node/node.hpp"

namespace dev::eth {
class Client;
}

namespace taraxa::net {

class Test : public TestFace {
 public:
  explicit Test(const std::shared_ptr<taraxa::AppBase>& app) : app_(app), kChainId(app->getConfig().genesis.chain_id) {}
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"test", "1.0"}}; }

  virtual Json::Value get_sortition_change(const Json::Value& param1) override;
  virtual Json::Value send_coin_transaction(const Json::Value& param1) override;
  virtual Json::Value send_coin_transactions(const Json::Value& param1) override;
  virtual Json::Value get_account_address() override;
  virtual Json::Value get_peer_count() override;
  virtual Json::Value get_node_status() override;
  virtual Json::Value get_all_nodes() override;

 private:
  std::weak_ptr<taraxa::AppBase> app_;
  const uint64_t kChainId;
};

}  // namespace taraxa::net
