#pragma once

#include "dag/dag_block.hpp"
#include "network/tarcap/packets_handlers/common/packet_handler.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa {
class DagBlockManager;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class TestState;

class TransactionPacketHandler : public PacketHandler {
 public:
  TransactionPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                           std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                           std::shared_ptr<TestState> test_state, uint16_t network_transaction_interval,
                           const addr_t &node_addr = {});

  void onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork);
  void sendTransactions(dev::p2p::NodeID const &peer_id, std::vector<taraxa::bytes> const &transactions);

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data, const std::shared_ptr<dev::p2p::Host>& host, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<TransactionManager> trx_mgr_;

  // FOR TESTING ONLY
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TestState> test_state_;

  uint16_t network_transaction_interval_ = 0;
  uint64_t received_trx_count_ = 0;
  uint64_t unique_received_trx_count_ = 0;
};

}  // namespace taraxa::network::tarcap