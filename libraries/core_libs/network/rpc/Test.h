#pragma once

#include <future>

#include "TestFace.h"

namespace dev::eth {
class Client;
}

namespace taraxa {
class FullNode;
}

namespace taraxa::net {

class Test : public TestFace {
 public:
  explicit Test(std::shared_ptr<taraxa::FullNode> const& _full_node) : full_node_(_full_node) {}
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"test", "1.0"}}; }

  virtual Json::Value get_sortition_change(const Json::Value& param1) override;
  virtual Json::Value send_coin_transaction(const Json::Value& param1) override;
  virtual Json::Value get_account_address() override;
  virtual Json::Value get_peer_count() override;
  virtual Json::Value get_node_status() override;
  virtual Json::Value get_packets_stats() override;
  virtual Json::Value get_all_nodes() override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
};

}  // namespace taraxa::net
