#pragma once

#include "dag/dag_block.hpp"
#include "packet_handler.hpp"
#include "transaction_manager/transaction.hpp"

namespace taraxa {
class DagBlockManager;
class DbStorage;
class TransactionManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

class DagAndTransactionPacketsHandler : public PacketHandler {
 public:
  DagAndTransactionPacketsHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<SyncingState> syncing_state,
                         std::shared_ptr<TransactionManager> trx_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                         std::shared_ptr<DbStorage> db, uint16_t network_min_dag_block_broadcast,
                         uint16_t network_max_dag_block_broadcast, uint16_t network_transaction_interval,
                         const addr_t &node_addr = {});

  void requestBlock(dev::p2p::NodeID const &peer_id, blk_hash_t hash);
  void sendBlock(dev::p2p::NodeID const &peer_id, DagBlock block);
  void sendBlockHash(dev::p2p::NodeID const &peer_id, taraxa::DagBlock block);
  void onNewBlockReceived(DagBlock block, std::vector<Transaction> transactions);
  void onNewBlockVerified(DagBlock const &block);

  void onNewTransactions(std::vector<taraxa::bytes> const &transactions, bool fromNetwork);
  void sendTransactions(dev::p2p::NodeID const &peer_id, std::vector<taraxa::bytes> const &transactions);

 private:
  void process(const PacketData &packet_data, const dev::RLP &packet_rlp) override;

  inline void processNewBlockPacket(const PacketData &packet_data, const dev::RLP &packet_rlp);
  inline void processNewBlockHashPacket(const PacketData &packet_data, const dev::RLP &packet_rlp);
  inline void processGetNewBlockPacket(const PacketData &packet_data, const dev::RLP &packet_rlp);
  inline void processTransactionPacket(const PacketData &packet_data, const dev::RLP &packet_rlp);

  std::pair<std::vector<dev::p2p::NodeID>, std::vector<dev::p2p::NodeID>> randomPartitionPeers(
      std::vector<dev::p2p::NodeID> const &_peers, std::size_t _number) const;

  std::vector<dev::p2p::NodeID> selectPeers(std::function<bool(TaraxaPeer const &)> const &_predicate);

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<TransactionManager> trx_mgr_;
  std::shared_ptr<DagBlockManager> dag_blk_mgr_;
  std::shared_ptr<DbStorage> db_;

  mutable std::mt19937_64 urng_;
  uint16_t network_min_dag_block_broadcast_ = 0;
  uint16_t network_max_dag_block_broadcast_ = 0;
  uint16_t network_transaction_interval_ = 0;
  uint64_t received_trx_count_ = 0;
  uint64_t unique_received_trx_count_ = 0;

  std::unordered_set<blk_hash_t> block_requestes_set_;

  // TODO: all those packets are together because of those testing structures
  // but they should be split apart
  // FOR TESTING ONLY
  std::unordered_map<blk_hash_t, DagBlock> test_blocks_;
  std::unordered_map<trx_hash_t, Transaction> test_transactions_;
};

}  // namespace taraxa::network::tarcap