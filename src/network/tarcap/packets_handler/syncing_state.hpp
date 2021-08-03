#pragma once

#include <atomic>

#include "libp2p/Common.h"
#include "logger/log.hpp"
#include "util/util.hpp"

namespace taraxa {
class PbftChain;
class DagManager;
class DagBlockManager;
class DagBlock;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PeersState;

enum GetBlocksPacketRequestType : ::byte { MissingHashes = 0x0, KnownHashes };

/**
 * @brief SyncingState contains common members and functions related to syncing that are shared among multiple classes
 */
class SyncingState {
 public:
  SyncingState(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PbftChain> pbft_chain,
               std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
               const addr_t &node_addr);

  /**
   * @brief Set pbft syncing
   *
   * @param syncing
   * @param peer_id used in case syncing flag == true to set which peer is the node syncing with
   */
  void set_pbft_syncing(bool syncing, const std::optional<dev::p2p::NodeID> &peer_id = {});

  /**
   * @brief Set dag syncing
   *
   * @param syncing
   * @param peer_id used in case syncing flag == true to set which peer is the node syncing with
   */
  void set_dag_syncing(bool syncing, const std::optional<dev::p2p::NodeID> &peer_id = {});

  /**
   * @brief Set current time as last received sync packet  time
   */
  void set_last_sync_packet_time();

  /**
   * @brief Check if syncing is active
   *
   * @return true if last syncing packet was received within SYNCING_INACTIVITY_THRESHOLD
   */
  bool is_actively_syncing() const;

  bool is_syncing() const;
  bool is_pbft_syncing() const;
  bool is_dag_syncing() const;

  const dev::p2p::NodeID &syncing_peer() const;

  void set_peer_malicious();
  bool is_peer_malicious(const dev::p2p::NodeID &peer_id) const;

  /**
   * @brief Restart syncing
   *
   * @param shared_state
   * @param force
   */
  void restartSyncingPbft(bool force);

  void syncPeerPbft(unsigned long height_to_sync);
  void requestPbftBlocks(dev::p2p::NodeID const &_id, size_t height_to_sync);
  void requestPendingDagBlocks();
  void requestBlocks(const dev::p2p::NodeID &_nodeID, std::vector<blk_hash_t> const &blocks,
                     GetBlocksPacketRequestType mode);

  void syncPbftNextVotes(uint64_t pbft_round, size_t pbft_previous_round_next_votes_size);
  void requestPbftNextVotes(dev::p2p::NodeID const &peerID, uint64_t pbft_round,
                            size_t pbft_previous_round_next_votes_size);

  std::pair<bool, std::vector<blk_hash_t>> checkDagBlockValidation(DagBlock const &block) const;

  /**
   * @brief Restart syncing in case there is ongoing syncing with the peer that just got disconnected
   *
   * @param node_id
   */
  void processDisconnect(const dev::p2p::NodeID &node_id);

 private:
  void set_peer(const dev::p2p::NodeID &peer_id);

 private:
  std::shared_ptr<PeersState> peers_state_{nullptr};

  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DagBlockManager> dag_blk_mgr_{nullptr};

  std::atomic<bool> pbft_syncing_{false};
  std::atomic<bool> dag_syncing_{false};

  ExpirationCache<dev::p2p::NodeID> malicious_peers_;

  // Number of seconds needed for ongoing syncing to be declared as inactive
  static constexpr std::chrono::seconds SYNCING_INACTIVITY_THRESHOLD{60};

  // What time was received last syncing packet
  std::chrono::steady_clock::time_point last_received_sync_packet_time_{std::chrono::steady_clock::now()};
  mutable std::shared_mutex time_mutex_;

  // Peer id that the node is syncing with
  dev::p2p::NodeID peer_id_;
  mutable std::shared_mutex peer_mutex_;

  // Declare logger instances
  LOG_OBJECTS_DEFINE
};

}  // namespace taraxa::network::tarcap
