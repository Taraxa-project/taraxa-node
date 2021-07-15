#include "syncing_state.hpp"

#include "consensus/pbft_chain.hpp"
#include "dag/dag.hpp"
#include "dag/dag_block_manager.hpp"
#include "network/tarcap/packet_types.hpp"
#include "peers_state.hpp"

namespace taraxa::network::tarcap {

SyncingState::SyncingState(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PbftChain> pbft_chain,
                           std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
                           const addr_t &node_addr)
    : peers_state_(std::move(peers_state)),
      pbft_chain_(std::move(pbft_chain)),
      dag_mgr_(std::move(dag_mgr)),
      dag_blk_mgr_(std::move(dag_blk_mgr)) {
  LOG_OBJECTS_CREATE("SYNC");
}

void SyncingState::restartSyncingPbft(bool force) {
  if (syncing_ && !force) {
    LOG(log_dg_) << "restartSyncingPbft called but syncing_ already true";
    return;
  }

  dev::p2p::NodeID max_pbft_chain_nodeID;
  uint64_t max_pbft_chain_size = 0;
  uint64_t max_node_dag_level = 0;
  {
    boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
    if (peers_state_->peers_.empty()) {
      LOG(log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
      return;
    }
    for (auto const &peer : peers_state_->peers_) {
      if (peer.second->pbft_chain_size_ > max_pbft_chain_size) {
        max_pbft_chain_size = peer.second->pbft_chain_size_;
        max_pbft_chain_nodeID = peer.first;
        max_node_dag_level = peer.second->dag_level_;
      } else if (peer.second->pbft_chain_size_ == max_pbft_chain_size && peer.second->dag_level_ > max_node_dag_level) {
        max_pbft_chain_nodeID = peer.first;
        max_node_dag_level = peer.second->dag_level_;
      }
    }
  }

  auto pbft_sync_period = pbft_chain_->pbftSyncingPeriod();
  if (max_pbft_chain_size > pbft_sync_period) {
    LOG(log_si_) << "Restarting syncing PBFT from peer " << max_pbft_chain_nodeID << ", peer PBFT chain size "
                 << max_pbft_chain_size << ", own PBFT chain synced at period " << pbft_sync_period;
    requesting_pending_dag_blocks_ = false;
    // TODO: When set sycning to false if never get peer response?
    syncing_ = true;
    peer_syncing_pbft_ = max_pbft_chain_nodeID;
    syncPeerPbft(peer_syncing_pbft_, pbft_sync_period + 1);
  } else {
    LOG(log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                 << pbft_chain_->getPbftChainSize() << ")"
                 << " is greater or equal than max node pbft chain size:" << max_pbft_chain_size;
    syncing_ = false;
    if (force || (!requesting_pending_dag_blocks_ &&
                  max_node_dag_level > std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()))) {
      LOG(log_nf_) << "Request pending " << max_node_dag_level << " "
                   << std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()) << "("
                   << dag_mgr_->getMaxLevel() << ")";
      requesting_pending_dag_blocks_ = true;
      requesting_pending_dag_blocks_node_id_ = max_pbft_chain_nodeID;
      requestPendingDagBlocks(max_pbft_chain_nodeID);
    }
  }
}

void SyncingState::syncPeerPbft(dev::p2p::NodeID const &_nodeID, unsigned long height_to_sync) {
  LOG(log_nf_) << "Sync peer node " << _nodeID << " from pbft chain height " << height_to_sync;
  requestPbftBlocks(_nodeID, height_to_sync);
}

void SyncingState::requestPbftBlocks(dev::p2p::NodeID const &_id, size_t height_to_sync) {
  LOG(log_dg_) << "Sending GetPbftBlockPacket with height: " << height_to_sync;
  peers_state_->sealAndSend(_id, SubprotocolPacketType::GetPbftBlockPacket, dev::RLPStream(1) << height_to_sync);
}

void SyncingState::requestPendingDagBlocks(dev::p2p::NodeID const &_id) {
  std::vector<blk_hash_t> known_non_finalized_blocks;
  auto blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      known_non_finalized_blocks.push_back(block);
    }
  }
  requestBlocks(_id, known_non_finalized_blocks, KnownHashes);
}

void SyncingState::requestBlocks(const dev::p2p::NodeID &_nodeID, std::vector<blk_hash_t> const &blocks,
                                 GetBlocksPacketRequestType mode) {
  LOG(log_nf_) << "Sending GetBlocksPacket";
  dev::RLPStream s(blocks.size() + 1);  // Mode + block itself
  s << mode;                            // Send mode first
  for (auto blk : blocks) {
    s << blk;
  }
  peers_state_->sealAndSend(_nodeID, SubprotocolPacketType::GetBlocksPacket, s);
}

void SyncingState::processDisconnect(const dev::p2p::NodeID &node_id) {
  if (syncing_ && peer_syncing_pbft_ == node_id && peers_state_->getPeersCount() > 0) {
    LOG(log_dg_) << "Syncing PBFT is stopping";
    restartSyncingPbft(true);
  } else if (requesting_pending_dag_blocks_ && requesting_pending_dag_blocks_node_id_ == node_id) {
    requesting_pending_dag_blocks_ = false;
    restartSyncingPbft(true);
  }
}

}  // namespace taraxa::network::tarcap