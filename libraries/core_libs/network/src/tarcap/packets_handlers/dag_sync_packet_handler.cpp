#include "network/tarcap/packets_handlers/dag_sync_packet_handler.hpp"

#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "transaction_manager/transaction_manager.hpp"

namespace taraxa::network::tarcap {

DagSyncPacketHandler::DagSyncPacketHandler(std::shared_ptr<PeersState> peers_state,
                                           std::shared_ptr<PacketsStats> packets_stats,
                                           std::shared_ptr<SyncingState> syncing_state,
                                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<PbftManager> pbft_mgr,
                                           std::shared_ptr<DagManager> dag_mgr,
                                           std::shared_ptr<TransactionManager> trx_mgr,
                                           std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                                           const addr_t& node_addr)
    : ExtSyncingPacketHandler(std::move(peers_state), std::move(packets_stats), std::move(syncing_state),
                              std::move(pbft_chain), std::move(pbft_mgr), std::move(dag_mgr), std::move(dag_blk_mgr),
                              std::move(db), node_addr, "DAG_SYNC_PH"),
      trx_mgr_(std::move(trx_mgr)) {}

void DagSyncPacketHandler::process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) {
  std::string received_dag_blocks_str;
  std::unordered_set<blk_hash_t> missing_blks;

  auto it = packet_data.rlp_.begin();
  uint64_t transactions_count = (*it++).toInt<uint64_t>();
  SharedTransactions new_transactions;
  for (uint64_t i = 0; i < transactions_count; i++) {
    Transaction transaction(*it++);
    peer->markTransactionAsKnown(transaction.getHash());
    new_transactions.emplace_back(std::make_shared<Transaction>(std::move(transaction)));
  }

  trx_mgr_->insertBroadcastedTransactions(std::move(new_transactions));

  for (; it != packet_data.rlp_.end();) {
    DagBlock block(*it++);
    peer->markDagBlockAsKnown(block.getHash());

    received_dag_blocks_str += block.getHash().abridged() + " ";

    auto status = checkDagBlockValidation(block);
    if (!status.first) {
      LOG(log_er_) << "DagBlock " << block.getHash() << " validation failed " << status.second;
      status.second.insert(block.getHash());
      missing_blks.merge(status.second);
      continue;
    }

    LOG(log_dg_) << "Storing block " << block.getHash().abridged() << " with " << new_transactions.size()
                 << " transactions";
    if (block.getLevel() > peer->dag_level_) peer->dag_level_ = block.getLevel();

    dag_blk_mgr_->insertBroadcastedBlock(std::move(block));
  }

  if (missing_blks.size() > 0) {
    requestDagBlocks(packet_data.from_node_id_, missing_blks, DagSyncRequestType::MissingHashes);
  }
  syncing_state_->set_dag_syncing(false);

  LOG(log_nf_) << "Received DagDagSyncPacket with blocks: " << received_dag_blocks_str;
}

}  // namespace taraxa::network::tarcap
