#pragma once

#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

namespace taraxa {
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class TestState;

class DagBlockPacketHandler : public ExtSyncingPacketHandler {
 public:
  DagBlockPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                        std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                        std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                        std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<TransactionManager> trx_mgr,
                        std::shared_ptr<DbStorage> db, std::shared_ptr<TestState> test_state, const addr_t &node_addr);

  virtual ~DagBlockPacketHandler() = default;

  void sendBlock(dev::p2p::NodeID const &peer_id, DagBlock block, const SharedTransactions &trxs);
  void onNewBlockReceived(DagBlock &&block);
  void onNewBlockVerified(DagBlock const &block, bool proposed, SharedTransactions &&trxs);

 private:
  void validatePacketRlpFormat(const PacketData &packet_data) override;
  void process(const PacketData &packet_data, const std::shared_ptr<TaraxaPeer> &peer) override;

  std::shared_ptr<TestState> test_state_;
  std::shared_ptr<TransactionManager> trx_mgr_{nullptr};
  ExpirationCache<blk_hash_t> seen_dags_;
};

}  // namespace taraxa::network::tarcap
