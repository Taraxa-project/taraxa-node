#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

#include "dag/dag.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

ExtSyncingPacketHandler::ExtSyncingPacketHandler(std::shared_ptr<PeersState> peers_state,
                                                 std::shared_ptr<PacketsStats> packets_stats,
                                                 std::shared_ptr<PbftSyncingState> pbft_syncing_state,
                                                 std::shared_ptr<PbftChain> pbft_chain,
                                                 std::shared_ptr<PbftManager> pbft_mgr,
                                                 std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DbStorage> db,
                                                 const addr_t &node_addr, const std::string &log_channel_name)
    : PacketHandler(std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
      pbft_syncing_state_(std::move(pbft_syncing_state)),
      pbft_chain_(std::move(pbft_chain)),
      pbft_mgr_(std::move(pbft_mgr)),
      dag_mgr_(std::move(dag_mgr)),
      db_(std::move(db)) {}

void ExtSyncingPacketHandler::restartSyncingPbft(bool force) {
  if (pbft_syncing_state_->isPbftSyncing() && !force) {
    LOG(log_dg_) << "restartSyncingPbft called but syncing_ already true";
    return;
  }

  std::shared_ptr<TaraxaPeer> peer = getMaxChainPeer();

  if (!peer) {
    pbft_syncing_state_->setPbftSyncing(false);
    LOG(log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
    return;
  }

  auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
  if (peer->pbft_chain_size_ > pbft_sync_period) {
    LOG(log_si_) << "Restarting syncing PBFT from peer " << peer->getId() << ", peer PBFT chain size "
                 << peer->pbft_chain_size_ << ", own PBFT chain synced at period " << pbft_sync_period;

    const auto node_id = peer->getId();

    pbft_syncing_state_->setPbftSyncing(true, pbft_sync_period, std::move(peer));

    // Handle case where syncing peer just disconnected
    if (!syncPeerPbft(pbft_sync_period + 1)) {
      // Only restart syncing if peer is actually disconnected and removed from peers_state, otherwise there is a risk
      // of endless recursion
      if (peers_state_->getPeer(node_id) == nullptr) {
        return restartSyncingPbft(true);
      }
      pbft_syncing_state_->setPbftSyncing(false);
      return;
    }

    // Disable snapshots only if are syncing from scratch
    if (pbft_syncing_state_->isDeepPbftSyncing()) {
      db_->disableSnapshots();
    }
  } else {
    LOG(log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                 << pbft_chain_->getPbftChainSize() << ")"
                 << " is greater or equal than max node pbft chain size:" << peer->pbft_chain_size_;
    pbft_syncing_state_->setPbftSyncing(false);
    db_->enableSnapshots();
  }
}

bool ExtSyncingPacketHandler::syncPeerPbft(unsigned long height_to_sync) {
  const auto node_id = pbft_syncing_state_->syncingPeer();
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
      if (peer.second->peer_light_node &&
          pbft_mgr_->pbftSyncingPeriod() + peer.second->peer_light_node_history < peer.second->pbft_chain_size_) {
        LOG(log_er_) << "Disconnecting from light node peer " << peer.first
                     << " History: " << peer.second->peer_light_node_history
                     << " chain size: " << peer.second->pbft_chain_size_;
        disconnect(peer.first, dev::p2p::UserReason);
        continue;
      }
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
  dev::RLPStream s(2);  // Period + blocks list
  s.append(static_cast<uint64_t>(period));
  s.append(blocks);

  sealAndSend(_nodeID, SubprotocolPacketType::GetDagSyncPacket, std::move(s));
}

}  // namespace taraxa::network::tarcap
