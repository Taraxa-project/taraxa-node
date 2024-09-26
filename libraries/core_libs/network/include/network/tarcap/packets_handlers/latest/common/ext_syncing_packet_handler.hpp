#pragma once

#include "dag/dag_manager.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "packet_handler.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa::network::tarcap {

/**
 * @brief ExtSyncingPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that need to interact with syncing process in some way
 */
template <class PacketType>
class ExtSyncingPacketHandler : public PacketHandler<PacketType> {
 public:
  ExtSyncingPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                          std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                          std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                          std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DbStorage> db, const addr_t &node_addr, const std::string &log_channel_name)
      : PacketHandler<PacketType>(conf, std::move(peers_state), std::move(packets_stats), node_addr, log_channel_name),
        pbft_syncing_state_(std::move(pbft_syncing_state)),
        pbft_chain_(std::move(pbft_chain)),
        pbft_mgr_(std::move(pbft_mgr)),
        dag_mgr_(std::move(dag_mgr)),
        db_(std::move(db)) {}

  virtual ~ExtSyncingPacketHandler() = default;
  ExtSyncingPacketHandler &operator=(const ExtSyncingPacketHandler &) = delete;
  ExtSyncingPacketHandler &operator=(ExtSyncingPacketHandler &&) = delete;

  /**
   * @brief Start syncing pbft if needed
   *
   */
  void startSyncingPbft() {
    if (pbft_syncing_state_->isPbftSyncing()) {
      LOG(this->log_dg_) << "startSyncingPbft called but syncing_ already true";
      return;
    }

    std::shared_ptr<TaraxaPeer> peer = getMaxChainPeer();
    if (!peer) {
      LOG(this->log_nf_) << "Restarting syncing PBFT not possible since no connected peers";
      return;
    }

    auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
    if (peer->pbft_chain_size_ > pbft_sync_period) {
      auto peer_id = peer->getId().abridged();
      auto peer_pbft_chain_size = peer->pbft_chain_size_.load();
      if (!pbft_syncing_state_->setPbftSyncing(true, pbft_sync_period, std::move(peer))) {
        LOG(this->log_dg_) << "startSyncingPbft called but syncing_ already true";
        return;
      }
      LOG(this->log_si_) << "Restarting syncing PBFT from peer " << peer_id << ", peer PBFT chain size "
                         << peer_pbft_chain_size << ", own PBFT chain synced at period " << pbft_sync_period;

      if (syncPeerPbft(pbft_sync_period + 1)) {
        // Disable snapshots only if are syncing from scratch
        if (pbft_syncing_state_->isDeepPbftSyncing()) {
          db_->disableSnapshots();
        }
      } else {
        pbft_syncing_state_->setPbftSyncing(false);
      }
    } else {
      LOG(this->log_nf_) << "Restarting syncing PBFT not needed since our pbft chain size: " << pbft_sync_period << "("
                         << pbft_chain_->getPbftChainSize() << ")"
                         << " is greater or equal than max node pbft chain size:" << peer->pbft_chain_size_;
      db_->enableSnapshots();
    }
  }

  /**
   * @brief Send sync request to the current syncing peer with specified request_period
   *
   * @param request_period
   *
   * @return true if sync request was sent, otherwise false
   */
  bool syncPeerPbft(PbftPeriod request_period) {
    const auto syncing_peer = pbft_syncing_state_->syncingPeer();
    if (!syncing_peer) {
      LOG(this->log_er_) << "Unable to send GetPbftSyncPacket. No syncing peer set.";
      return false;
    }

    if (request_period > syncing_peer->pbft_chain_size_) {
      LOG(this->log_wr_) << "Invalid syncPeerPbft argument. Node " << syncing_peer->getId() << " chain size "
                         << syncing_peer->pbft_chain_size_ << ", requested period " << request_period;
      return false;
    }

    LOG(this->log_nf_) << "Send GetPbftSyncPacket with period " << request_period << " to node "
                       << syncing_peer->getId();
    return this->sealAndSend(syncing_peer->getId(), SubprotocolPacketType::kGetPbftSyncPacket,
                             std::move(dev::RLPStream(1) << request_period));
  }

  void requestDagBlocks(const dev::p2p::NodeID &_nodeID, const std::unordered_set<blk_hash_t> &blocks,
                        PbftPeriod period) {
    dev::RLPStream s(2);  // Period + blocks list
    s.append(period);
    s.append(blocks);

    this->sealAndSend(_nodeID, SubprotocolPacketType::kGetDagSyncPacket, std::move(s));
  }

  void requestPendingDagBlocks(std::shared_ptr<TaraxaPeer> peer = nullptr) {
    if (!peer) {
      peer = getMaxChainPeer([](const std::shared_ptr<TaraxaPeer> &peer) {
        if (peer->peer_dag_synced_ || !peer->dagSyncingAllowed()) {
          return false;
        }
        return true;
      });
      if (!peer) {
        LOG(this->log_nf_) << "requestPendingDagBlocks not possible since no peers are matching conditions";
        return;
      }
    }

    if (!peer) {
      LOG(this->log_nf_) << "requestPendingDagBlocks not possible since no connected peers";
      return;
    }

    // This prevents ddos requesting dag blocks. We can only request this one time from one peer.
    if (peer->peer_dag_synced_) {
      LOG(this->log_nf_) << "requestPendingDagBlocks not possible since already requested for peer";
      return;
    }

    // Only request dag blocks if periods are matching
    auto pbft_sync_period = pbft_mgr_->pbftSyncingPeriod();
    if (pbft_sync_period == peer->pbft_chain_size_) {
      // This prevents parallel requests
      if (bool b = false; !peer->peer_dag_syncing_.compare_exchange_strong(b, !b)) {
        LOG(this->log_nf_) << "requestPendingDagBlocks not possible since already requesting for peer";
        return;
      }
      LOG(this->log_nf_) << "Request pending blocks from peer " << peer->getId();
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

  std::shared_ptr<TaraxaPeer> getMaxChainPeer(std::function<bool(const std::shared_ptr<TaraxaPeer> &)> filter_func =
                                                  [](const std::shared_ptr<TaraxaPeer> &) { return true; }) {
    std::shared_ptr<TaraxaPeer> max_pbft_chain_peer;
    PbftPeriod max_pbft_chain_size = 0;
    uint64_t max_node_dag_level = 0;

    // Find peer with max pbft chain and dag level
    for (auto const &peer : this->peers_state_->getAllPeers()) {
      // Apply the filter function
      if (!filter_func(peer.second)) {
        continue;
      }

      if (peer.second->pbft_chain_size_ > max_pbft_chain_size) {
        if (peer.second->peer_light_node &&
            pbft_mgr_->pbftSyncingPeriod() + peer.second->peer_light_node_history < peer.second->pbft_chain_size_) {
          LOG(this->log_er_) << "Disconnecting from light node peer " << peer.first
                             << " History: " << peer.second->peer_light_node_history
                             << " chain size: " << peer.second->pbft_chain_size_;
          this->disconnect(peer.first, dev::p2p::UserReason);
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

 protected:
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_{nullptr};

  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<PbftManager> pbft_mgr_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DbStorage> db_{nullptr};
};

}  // namespace taraxa::network::tarcap
