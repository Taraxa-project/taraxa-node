#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
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

  std::shared_ptr<TaraxaPeer> peer = getMaxChainPeer();

  if (!peer) {
    syncing_state_->set_pbft_syncing(false);
    LOG(log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
    return;
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (peer->pbft_chain_size_ > pbft_sync_period) {
    LOG(log_si_) << "Restarting syncing PBFT from peer " << peer->getId() << ", peer PBFT chain size "
                 << peer->pbft_chain_size_ << ", own PBFT chain synced at period " << pbft_sync_period;

    syncing_state_->set_pbft_syncing(true, pbft_sync_period, std::move(peer));

    // Handle case where syncing peer just disconnected
    if (!syncPeerPbft(pbft_sync_period + 1)) {
      return restartSyncingPbft(true);
    }

    // Disable snapshots only if are syncing from scratch
    if (syncing_state_->is_deep_pbft_syncing()) {
      db_->disableSnapshots();
    }
  } else {
    LOG(log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                 << pbft_chain_->getPbftChainSize() << ")"
                 << " is greater or equal than max node pbft chain size:" << peer->pbft_chain_size_;
    syncing_state_->set_pbft_syncing(false);
    db_->enableSnapshots();
  }
}

bool ExtSyncingPacketHandler::syncPeerPbft(unsigned long height_to_sync) {
  const auto node_id = syncing_state_->syncing_peer();
  LOG(log_nf_) << "Sync peer node " << node_id << " from pbft chain height " << height_to_sync;
  return sealAndSend(node_id, SubprotocolPacketType::GetPbftSyncPacket, std::move(dev::RLPStream(1) << height_to_sync));
}

std::shared_ptr<TaraxaPeer> ExtSyncingPacketHandler::getMaxChainPeer() {
  std::shared_ptr<TaraxaPeer> max_pbft_chain_peer;
  uint64_t max_pbft_chain_size = 0;
  uint64_t max_node_dag_level = 0;

  // Find peer with max pbft chain and dag level
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
  return max_pbft_chain_peer;
}

void ExtSyncingPacketHandler::requestPendingDagBlocks(std::shared_ptr<TaraxaPeer> peer) {
  if (!peer) {
    peer = getMaxChainPeer();
  }

  if (!peer) {
    LOG(log_nf_) << "requestPendingDagBlocks not possible since no connected peers";
    return;
  }

  // This prevents ddos requesting dag blocks. We can only request this one time from one peer.
  if (peer->peer_dag_synced_) {
    LOG(log_nf_) << "requestPendingDagBlocks not possible since already requested for peer";
    return;
  }

  // Only request dag blocks if periods are matching
  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (pbft_sync_period == peer->pbft_chain_size_) {
    // This prevents parallel requests
    if (bool b = false; !peer->peer_dag_syncing_.compare_exchange_strong(b, !b)) {
      LOG(log_nf_) << "requestPendingDagBlocks not possible since already requesting for peer";
      return;
    }
    LOG(log_nf_) << "Request pending blocks from peer " << peer->getId();
    std::unordered_set<blk_hash_t> known_non_finalized_blocks;
    auto [period, blocks] = dag_mgr_->getNonFinalizedBlocks();
    for (auto &level_blocks : blocks) {
      for (auto &block : level_blocks.second) {
        known_non_finalized_blocks.insert(block);
      }
    }

    requestDagBlocks(peer->getId(), known_non_finalized_blocks, period);
  }
}

void ExtSyncingPacketHandler::requestDagBlocks(const dev::p2p::NodeID &_nodeID,
                                               const std::unordered_set<blk_hash_t> &blocks, uint64_t period) {
  LOG(log_nf_) << "Sending GetDagSyncPacket";
  dev::RLPStream s(blocks.size() + 1);  // Period + block itself
  s << static_cast<uint64_t>(period);   // Send period first
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
      LOG(log_tr_) << "Block " << block.getHash().abridged() << " has a missing tip " << tip.abridged();
      missing_blks.insert(tip);
    } else {
      expected_level = std::max(tip_block->getLevel(), expected_level);
    }
  }

  const auto pivot = block.getPivot();
  const auto pivot_block = dag_blk_mgr_->getDagBlock(pivot);
  if (!pivot_block) {
    LOG(log_tr_) << "Block " << block.getHash().abridged() << " has a missing pivot " << pivot.abridged();
    missing_blks.insert(pivot);
  }

  if (missing_blks.size()) return std::make_pair(false, missing_blks);

  expected_level = std::max(pivot_block->getLevel(), expected_level);
  expected_level++;
  if (expected_level != block.getLevel()) {
    LOG(log_er_) << "Invalid block level " << block.getLevel() << " for block " << block.getHash().abridged()
                 << " . Expected level " << expected_level;
    return std::make_pair(false, missing_blks);
  }

  return std::make_pair(true, missing_blks);
}

}  // namespace taraxa::network::tarcap
