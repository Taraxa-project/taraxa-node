#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packets_handlers/common/get_blocks_request_type.hpp"
#include "network/tarcap/shared_states/syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

ExtSyncingPacketHandler::ExtSyncingPacketHandler(
    std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
    std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
    std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
    std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db, const addr_t &node_addr,
    const std::string &log_channel_name)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
      syncing_state_(std::move(syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      db_(std::move(db)) {}

void ExtSyncingPacketHandler::restartSyncingPbft(bool force) {
  if (syncing_state_->is_pbft_syncing() && !force) {
    LOG(log_dg_) << "restartSyncingPbft called but syncing_ already true";
    return;
  }

  std::shared_ptr<TaraxaPeer> max_pbft_chain_peer;
  uint64_t max_pbft_chain_size = 0;
  uint64_t max_node_dag_level = 0;

  if (!peers_state_->getPeersCount()) {
    LOG(log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
    return;
  }
  for (auto const &peer : peers_state_->getAllPeers()) {
    if (peer.second->pbft_chain_size_ > max_pbft_chain_size) {
      max_pbft_chain_size = peer.second->pbft_chain_size_;
      max_node_dag_level = peer.second->dag_level_;
      max_pbft_chain_peer = peer.second;
    } else if (peer.second->pbft_chain_size_ == max_pbft_chain_size && peer.second->dag_level_ > max_node_dag_level) {
      max_node_dag_level = peer.second->dag_level_;
      max_pbft_chain_peer = peer.second;
    }
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (max_pbft_chain_size > pbft_sync_period) {
    LOG(log_si_) << "Restarting syncing PBFT from peer " << max_pbft_chain_peer->getId() << ", peer PBFT chain size "
                 << max_pbft_chain_size << ", own PBFT chain synced at period " << pbft_sync_period;

    syncing_state_->set_dag_syncing(false);
    syncing_state_->set_pbft_syncing(true, max_pbft_chain_size - pbft_sync_period, max_pbft_chain_peer);
    syncPeerPbft(pbft_sync_period + 1);
    // Disable snapshots only if are syncing from scratch
    if (syncing_state_->is_deep_pbft_syncing()) {
      db_->disableSnapshots();
    }
  } else {
    LOG(log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                 << pbft_chain_->getPbftChainSize() << ")"
                 << " is greater or equal than max node pbft chain size:" << max_pbft_chain_size;
    syncing_state_->set_pbft_syncing(false);
    db_->enableSnapshots();

    if (force || (!syncing_state_->is_dag_syncing() &&
                  max_node_dag_level > std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()))) {
      LOG(log_nf_) << "Request pending " << max_node_dag_level << " "
                   << std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()) << "("
                   << dag_mgr_->getMaxLevel() << ")";
      syncing_state_->set_dag_syncing(true, max_pbft_chain_peer);
      requestPendingDagBlocks();
    } else {
      syncing_state_->set_dag_syncing(false);
    }
  }
}

void ExtSyncingPacketHandler::syncPeerPbft(unsigned long height_to_sync) {
  const auto node_id = syncing_state_->syncing_peer();
  LOG(log_nf_) << "Sync peer node " << node_id << " from pbft chain height " << height_to_sync;
  sealAndSend(node_id, SubprotocolPacketType::GetPbftSyncPacket, std::move(dev::RLPStream(1) << height_to_sync));
}

void ExtSyncingPacketHandler::requestPendingDagBlocks() {
  std::unordered_set<blk_hash_t> known_non_finalized_blocks;
  auto blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      known_non_finalized_blocks.insert(block);
    }
  }

  requestDagBlocks(syncing_state_->syncing_peer(), known_non_finalized_blocks, DagSyncRequestType::KnownHashes);
}

void ExtSyncingPacketHandler::requestDagBlocks(const dev::p2p::NodeID &_nodeID,
                                               const std::unordered_set<blk_hash_t> &blocks, DagSyncRequestType mode) {
  LOG(log_nf_) << "Sending GetDagSyncPacket";
  dev::RLPStream s(blocks.size() + 1);  // Mode + block itself
  s << static_cast<uint8_t>(mode);      // Send mode first
  for (const auto &blk : blocks) {
    s << blk;
  }
  sealAndSend(_nodeID, SubprotocolPacketType::GetDagSyncPacket, std::move(s));
}

std::pair<bool, std::unordered_set<blk_hash_t>> ExtSyncingPacketHandler::checkDagBlockValidation(
    const DagBlock &block) const {
  std::unordered_set<blk_hash_t> missing_blks;

  if (dag_blk_mgr_->getDagBlock(block.getHash())) {
    // The DAG block exist
    return std::make_pair(true, missing_blks);
  }

  level_t expected_level = 0;
  for (auto const &tip : block.getTips()) {
    auto tip_block = dag_blk_mgr_->getDagBlock(tip);
    if (!tip_block) {
      LOG(log_nf_) << "Block " << block.getHash().abridged() << " has a missing tip " << tip.abridged();
      missing_blks.insert(tip);
    } else {
      expected_level = std::max(tip_block->getLevel(), expected_level);
    }
  }

  const auto pivot = block.getPivot();
  const auto pivot_block = dag_blk_mgr_->getDagBlock(pivot);
  if (!pivot_block) {
    LOG(log_nf_) << "Block " << block.getHash().abridged() << " has a missing pivot " << pivot.abridged();
    missing_blks.insert(pivot);
  }

  if (missing_blks.size()) return std::make_pair(false, missing_blks);

  expected_level = std::max(pivot_block->getLevel(), expected_level);
  expected_level++;
  if (expected_level != block.getLevel()) {
    LOG(log_nf_) << "Invalid block level " << block.getLevel() << " for block " << block.getHash().abridged()
                 << " . Expected level " << expected_level;
    return std::make_pair(false, missing_blks);
  }

  return std::make_pair(true, missing_blks);
}

}  // namespace taraxa::network::tarcap
