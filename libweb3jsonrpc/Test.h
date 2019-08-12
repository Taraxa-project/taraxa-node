#pragma once
#include "../full_node.hpp"
#include "TestFace.h"

namespace dev {

namespace eth {
class Client;
}

namespace rpc {

class Test : public TestFace {
 public:
  Test(std::shared_ptr<taraxa::FullNode>& _full_node);
  virtual RPCModules implementedModules() const override {
    return RPCModules{RPCModule{"test", "1.0"}};
  }

  virtual Json::Value insert_dag_block(const Json::Value& param1) override;
  virtual Json::Value insert_stamped_dag_block(
      const Json::Value& param1) override;
  virtual Json::Value get_dag_block(const Json::Value& param1) override;
  virtual Json::Value get_dag_block_children(
      const Json::Value& param1) override;
  virtual Json::Value get_dag_block_siblings(
      const Json::Value& param1) override;
  virtual Json::Value get_dag_block_tips(const Json::Value& param1) override;
  virtual Json::Value get_dag_block_pivot_chain(
      const Json::Value& param1) override;
  virtual Json::Value get_dag_block_subtree(const Json::Value& param1) override;
  virtual Json::Value get_dag_block_epfriend(
      const Json::Value& param1) override;
  virtual Json::Value send_coin_transaction(const Json::Value& param1) override;
  virtual Json::Value create_test_coin_transactions(
      const Json::Value& param1) override;
  virtual Json::Value get_num_proposed_blocks() override;
  virtual Json::Value send_pbft_schedule_block(
      const Json::Value& param1) override;
  virtual Json::Value get_account_address() override;
  virtual Json::Value get_account_balance(const Json::Value& param1) override;
  virtual Json::Value get_peer_count() override;
  virtual Json::Value get_all_peers() override;
  virtual Json::Value node_stop() override;
  virtual Json::Value node_reset() override;
  virtual Json::Value node_start() override;
  virtual Json::Value should_speak(const Json::Value& param1) override;
  virtual Json::Value place_vote(const Json::Value& param1) override;
  virtual Json::Value get_votes(const Json::Value& param1) override;
  virtual Json::Value draw_graph(const Json::Value& param1) override;
  virtual Json::Value get_transaction_count(const Json::Value& param1) override;
  virtual Json::Value get_executed_trx_count(const Json::Value& param1) override;
  virtual Json::Value get_dag_size(const Json::Value& param1) override;

 private:
  std::weak_ptr<taraxa::FullNode> full_node_;
};

}  // namespace rpc
}  // namespace dev
