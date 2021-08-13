#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class DagBlockManager;
class DbStorage;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;
class SyncingHandler;
class TestState;

class DagPacketsHandler : public PacketHandler {
 public:
  DagPacketsHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                    std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
                    std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                    std::shared_ptr<DbStorage> db, std::shared_ptr<TestState> test_state,
                    uint16_t network_min_dag_block_broadcast, uint16_t network_max_dag_block_broadcast,
                    const addr_t &node_addr = {});

  void requestBlock(dev::p2p::NodeID const &peer_id, blk_hash_t hash);
  void sendBlock(dev::p2p::NodeID const &peer_id, DagBlock block);
  void sendBlockHash(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block);
  void onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions);
  void onNewBlockVerified(DagBlock const &block);

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data, const std::shared_ptr<dev::p2p::Host>& host, const std::shared_ptr<TaraxaPeer>& peer) override;

  inline void processNewBlockPacket(const dev::RLP &packet_rlp, const PacketData &packet_data, const std::shared_ptr<TaraxaPeer>& peer);
  inline void processNewBlockHashPacket(const dev::RLP &packet_rlp, const PacketData &packet_data, const std::shared_ptr<TaraxaPeer>& peer);
  inline void processGetNewBlockPacket(const dev::RLP &packet_rlp, const PacketData &packet_data, const std::shared_ptr<TaraxaPeer>& peer);

  bool hasBlockRequest(const blk_hash_t& block_hash) const;
  void insertBlockRequest(const blk_hash_t& block_hash);

  std::pair<std::vector<dev::p2p::NodeID>, std::vector<dev::p2p::NodeID>> randomPartitionPeers(
      std::vector<dev::p2p::NodeID> const &_peers, std::size_t _number);

  std::vector<dev::p2p::NodeID> selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate);

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<SyncingHandler> syncing_handler_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<DbStorage> db_;
  std::shared_ptr<TestState> test_state_;

  const uint16_t network_min_dag_block_broadcast_;
  const uint16_t network_max_dag_block_broadcast_;

  mutable std::shared_mutex block_requestes_mutex_;
  std::unordered_set<blk_hash_t> block_requestes_set_;

  thread_local static std::mt19937_64 urng_;
};

}  // namespace taraxa::network::tarcap