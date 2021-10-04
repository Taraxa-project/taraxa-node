#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/syncing_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<PacketsStats> packets_stats,
                                           std::shared_ptr<SyncingState> syncing_state,
                                           std::shared_ptr<SyncingHandler> syncing_handler,
                                           std::shared_ptr<DagBlockManager> dag_blk_mgr, const addr_t& node_addr)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, "DAG_SYNC_PH"),
      syncing_state_(std::move(syncing_state)),
      syncing_handler_(std::move(syncing_handler)),
      dag_blk_mgr_(std::move(dag_blk_mgr)) {}

void DagSyncPacketHandler::process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  std::string received_dag_blocks_str;
  std::unordered_set<blk_hash_t> missing_blks;

  auto it = packet_data.rlp_.begin();

  for (; it != packet_data.rlp_.end();) {
    DagBlock block(*it++);
    peer->markDagBlockAsKnown(block.getHash());

    std::vector<Transaction> new_transactions;
    for (size_t i = 0; i < block.getTrxs().size(); i++) {
      Transaction transaction(*it++);
      peer->markTransactionAsKnown(transaction.getHash());
      LOG(log_er_) << "Received DagSyncPacket trx" << transaction.getHash().abridged() << " peer id "
                   << packet_data.from_node_id_ << " id " << packet_data.id_ << " time "
                   << packet_data.receive_time_.time_since_epoch().count();
      new_transactions.push_back(std::move(transaction));
    }

    received_dag_blocks_str += block.getHash().abridged() + " ";

    auto status = syncing_handler_->checkDagBlockValidation(block);
    if (!status.first) {
      LOG(log_er_) << "DagBlockValidation failed " << status.second;
      status.second.insert(block.getHash());
      missing_blks.merge(status.second);
      continue;
    }

    LOG(log_dg_) << "Storing block " << block.getHash().abridged() << " with " << new_transactions.size()
                 << " transactions";
    LOG(log_er_) << "Received DagSyncPacket block" << block.getHash().abridged() << " peer id "
                 << packet_data.from_node_id_ << " id " << packet_data.id_ << " time "
                 << packet_data.receive_time_.time_since_epoch().count();
    if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();
    dag_blk_mgr_->insertBroadcastedBlockWithTransactions(block, new_transactions);
  }

  if (missing_blks.size() > 0) {
    syncing_handler_->requestBlocks(packet_data.from_node_id_, missing_blks, DagSyncRequestType::MissingHashes);
  }
  syncing_state_->set_dag_syncing(false);

  LOG(log_nf_) << "Received DagDagSyncPacket with blocks: " << received_dag_blocks_str;
}

}  // namespace taraxa::network::tarcap
