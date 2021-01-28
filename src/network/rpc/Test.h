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
  explicit Test(std::shared_ptr<taraxa::FullNode> const& _full_node);
  virtual RPCModules implementedModules() const override { return RPCModules{RPCModule{"test", "1.0"}}; }

  virtual Json::Value insert_dag_block(const Json::Value& param1) override;
  virtual Json::Value get_dag_block(const Json::Value& param1) override;
  virtual Json::Value send_coin_transaction(const Json::Value& param1) override;
  virtual Json::Value create_test_coin_transactions(const Json::Value& param1) override;
  virtual Json::Value get_num_proposed_blocks() override;
  virtual Json::Value get_account_address() override;
  virtual Json::Value get_account_balance(const Json::Value& param1) override;
  virtual Json::Value get_peer_count() override;
  virtual Json::Value get_node_status() override;
  virtual Json::Value get_node_version() override;
  virtual Json::Value get_node_count() override;
  virtual Json::Value get_all_peers() override;
  virtual Json::Value get_all_nodes() override;
  virtual Json::Value should_speak(const Json::Value& param1) override;
  virtual Json::Value place_vote(const Json::Value& param1) override;
  virtual Json::Value get_votes(const Json::Value& param1) override;
  virtual Json::Value draw_graph(const Json::Value& param1) override;
  virtual Json::Value get_transaction_count(const Json::Value& param1) override;
  virtual Json::Value get_executed_trx_count(const Json::Value& param1) override;
  virtual Json::Value get_executed_blk_count(const Json::Value& param1) override;
  virtual Json::Value get_dag_size(const Json::Value& param1) override;
  virtual Json::Value get_dag_blk_count(const Json::Value& param1) override;
  virtual Json::Value get_pbft_chain_size() override;
  virtual Json::Value get_pbft_chain_blocks(const Json::Value& param1) override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
  std::future<void> trx_creater_;
};

}  // namespace taraxa::net


