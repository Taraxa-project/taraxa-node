#pragma once

#include "dag/dag_block.hpp"
#include "network/tarcap/packets_handlers/common/packet_handler.hpp"
#include "transaction/transaction.hpp"

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
                           const addr_t& node_addr);

  virtual ~TransactionPacketHandler() = default;

  void onNewTransactions(SharedTransactions&& transactions, bool fromNetwork);
  void sendTransactions(dev::p2p::NodeID const& peer_id, std::vector<taraxa::bytes> const& transactions);

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<TransactionManager> trx_mgr_;

  // FOR TESTING ONLY
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<TestState> test_state_;

  const uint16_t network_transaction_interval_;
  std::atomic<uint64_t> received_trx_count_{0};
  std::atomic<uint64_t> unique_received_trx_count_{0};

  const uint32_t MAX_TRANSACTIONS_IN_PACKET{1000};
};

}  // namespace taraxa::network::tarcap
