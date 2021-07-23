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
      dag_blk_mgr_(std::move(dag_blk_mgr)),
      malicious_peers_(300, 50) {
  LOG_OBJECTS_CREATE("SYNC");
}

void SyncingState::restartSyncingPbft(bool force) {
  if (is_pbft_syncing() && !force) {
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

    set_dag_syncing(false);
    set_pbft_syncing(true, max_pbft_chain_nodeID);
    syncPeerPbft(pbft_sync_period + 1);
  } else {
    LOG(log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                 << pbft_chain_->getPbftChainSize() << ")"
                 << " is greater or equal than max node pbft chain size:" << max_pbft_chain_size;
    set_pbft_syncing(false);

    if (force || (!is_dag_syncing() &&
                  max_node_dag_level > std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()))) {
      LOG(log_nf_) << "Request pending " << max_node_dag_level << " "
                   << std::max(dag_mgr_->getMaxLevel(), dag_blk_mgr_->getMaxDagLevelInQueue()) << "("
                   << dag_mgr_->getMaxLevel() << ")";
      set_dag_syncing(true, max_pbft_chain_nodeID);
      requestPendingDagBlocks();
    } else {
      set_dag_syncing(false);
    }
  }
}

void SyncingState::syncPeerPbft(unsigned long height_to_sync) {
  const auto &node_id = syncing_peer();
  LOG(log_nf_) << "Sync peer node " << node_id << " from pbft chain height " << height_to_sync;
  requestPbftBlocks(node_id, height_to_sync);
}

void SyncingState::requestPbftBlocks(dev::p2p::NodeID const &_id, size_t height_to_sync) {
  LOG(log_dg_) << "Sending GetPbftBlockPacket with height: " << height_to_sync;
  peers_state_->sealAndSend(_id, SubprotocolPacketType::GetPbftBlockPacket, dev::RLPStream(1) << height_to_sync);
}

void SyncingState::requestPendingDagBlocks() {
  std::vector<blk_hash_t> known_non_finalized_blocks;
  auto blocks = dag_mgr_->getNonFinalizedBlocks();
  for (auto &level_blocks : blocks) {
    for (auto &block : level_blocks.second) {
      known_non_finalized_blocks.push_back(block);
    }
  }

  requestBlocks(syncing_peer(), known_non_finalized_blocks, KnownHashes);
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

void SyncingState::syncPbftNextVotes(uint64_t const pbft_round, size_t const pbft_previous_round_next_votes_size) {
  dev::p2p::NodeID peer_node_ID;
  uint64_t peer_max_pbft_round = 1;
  size_t peer_max_previous_round_next_votes_size = 0;
  {
    boost::shared_lock<boost::shared_mutex> lock(peers_state_->peers_mutex_);
    // Find max peer PBFT round
    for (auto const &peer : peers_state_->peers_) {
      if (peer.second->pbft_round_ > peer_max_pbft_round) {
        peer_max_pbft_round = peer.second->pbft_round_;
        peer_node_ID = peer.first;
      }
    }

    if (pbft_round == peer_max_pbft_round) {
      // No peers ahead, find peer PBFT previous round max next votes size
      for (auto const &peer : peers_state_->peers_) {
        if (peer.second->pbft_previous_round_next_votes_size_ > peer_max_previous_round_next_votes_size) {
          peer_max_previous_round_next_votes_size = peer.second->pbft_previous_round_next_votes_size_;
          peer_node_ID = peer.first;
        }
      }
    }
  }

  if (pbft_round < peer_max_pbft_round ||
      (pbft_round == peer_max_pbft_round &&
       pbft_previous_round_next_votes_size < peer_max_previous_round_next_votes_size)) {
    // TODO: was log_dg_next_votes_sync_
    LOG(log_dg_) << "Syncing PBFT next votes. Current PBFT round " << pbft_round << ", previous round next votes size "
                 << pbft_previous_round_next_votes_size << ". Peer " << peer_node_ID << " is in PBFT round "
                 << peer_max_pbft_round << ", previous round next votes size "
                 << peer_max_previous_round_next_votes_size;
    requestPbftNextVotes(peer_node_ID, pbft_round, pbft_previous_round_next_votes_size);
  }
}

void SyncingState::requestPbftNextVotes(dev::p2p::NodeID const &peerID, uint64_t const pbft_round,
                                        size_t const pbft_previous_round_next_votes_size) {
  // TODO: was log_dg_next_votes_sync_
  LOG(log_dg_) << "Sending GetPbftNextVotes with round " << pbft_round << " previous round next votes size "
               << pbft_previous_round_next_votes_size;
  peers_state_->sealAndSend(peerID, GetPbftNextVotes,
                            RLPStream(2) << pbft_round << pbft_previous_round_next_votes_size);
}

void SyncingState::processDisconnect(const dev::p2p::NodeID &node_id) {
  if (is_pbft_syncing() && syncing_peer() == node_id) {
    if (peers_state_->getPeersCount() > 0) {
      LOG(log_dg_) << "Restart PBFT/DAG syncing due to syncing peer disconnect.";
      restartSyncingPbft(true);
    } else {
      LOG(log_dg_) << "Stop PBFT/DAG syncing due to syncing peer disconnect and no other peers available.";
      set_pbft_syncing(false);
      set_dag_syncing(false);
    }
  }
}

void SyncingState::set_peer(const dev::p2p::NodeID &peer_id) {
  std::unique_lock lock(peer_mutex_);
  peer_id_ = peer_id;
}

const dev::p2p::NodeID &SyncingState::syncing_peer() const {
  std::shared_lock lock(peer_mutex_);
  return peer_id_;
}

void SyncingState::set_pbft_syncing(bool syncing, const std::optional<dev::p2p::NodeID> &peer_id) {
  assert((syncing && peer_id) || !syncing);
  pbft_syncing_ = syncing;

  if (peer_id) {
    set_peer(peer_id.value());
  }

  if (syncing) {
    // Reset last sync packet time when syncing is restarted/fresh syncing flag is set
    set_last_sync_packet_time();
  }
}

void SyncingState::set_dag_syncing(bool syncing, const std::optional<dev::p2p::NodeID> &peer_id) {
  assert((syncing && peer_id) || !syncing);
  dag_syncing_ = syncing;

  if (peer_id) {
    set_peer(peer_id.value());
  }
}

void SyncingState::set_last_sync_packet_time() {
  std::unique_lock lock(time_mutex_);
  last_received_sync_packet_time_ = std::chrono::steady_clock::now();
}

bool SyncingState::is_actively_syncing() const {
  auto now = std::chrono::steady_clock::now();

  std::shared_lock lock(time_mutex_);
  return std::chrono::duration_cast<std::chrono::seconds>(now - last_received_sync_packet_time_) <=
         SYNCING_INACTIVITY_THRESHOLD;
}

void SyncingState::set_peer_malicious() {
  // this lock is for peer_id_ not the malicious_peers_
  std::shared_lock lock(peer_mutex_);
  malicious_peers_.insert(peer_id_);
}

bool SyncingState::is_peer_malicious(const dev::p2p::NodeID &peer_id) const { return malicious_peers_.count(peer_id); }

bool SyncingState::is_syncing() const { return is_pbft_syncing() || is_dag_syncing(); }

bool SyncingState::is_pbft_syncing() const { return pbft_syncing_; }

bool SyncingState::is_dag_syncing() const { return dag_syncing_; }

}  // namespace taraxa::network::tarcap