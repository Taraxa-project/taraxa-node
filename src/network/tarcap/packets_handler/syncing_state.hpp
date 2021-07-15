#pragma once

#include <atomic>

#include "libp2p/Common.h"
#include "logger/log.hpp"

namespace taraxa {

class PbftChain;
class DagManager;
class DagBlockManager;

}  // namespace taraxa

namespace taraxa::network::tarcap {

class PeersState;

/**
 * @brief SyncingState contains common members and functions related to syncing that are shared among multiple classes
 */
class SyncingState {
 public:
  enum GetBlocksPacketRequestType : ::byte { MissingHashes = 0x0, KnownHashes };

  SyncingState(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PbftChain> pbft_chain,
               std::shared_ptr<DagManager> dag_mgr, std::shared_ptr<DagBlockManager> dag_blk_mgr,
               const addr_t &node_addr);

  /**
   * @brief Restart syncing
   *
   * @param shared_state
   * @param force
   */
  void restartSyncingPbft(bool force);

  /**
   * @brief Restart syncing in case there is ongoing syncing with the peer that just got disconnected
   *
   * @param node_id
   */
  void processDisconnect(const dev::p2p::NodeID &node_id);

 public:
  // TODO: all of this must be thread safe - will be fixed and copy pasted here by Matus's syncing PR
  std::atomic<bool> syncing_{false};
  dev::p2p::NodeID peer_syncing_pbft_{};
  bool requesting_pending_dag_blocks_ = {false};
  dev::p2p::NodeID requesting_pending_dag_blocks_node_id_{};

  // Declare logger instances
  LOG_OBJECTS_DEFINE

 private:
  std::shared_ptr<PeersState> peers_state_{nullptr};
  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DagBlockManager> dag_blk_mgr_{nullptr};

  void syncPeerPbft(dev::p2p::NodeID const &_nodeID, unsigned long height_to_sync);
  void requestPbftBlocks(dev::p2p::NodeID const &_id, size_t height_to_sync);
  void requestPendingDagBlocks(dev::p2p::NodeID const &_id);
  void requestBlocks(const dev::p2p::NodeID &_nodeID, std::vector<blk_hash_t> const &blocks,
                     GetBlocksPacketRequestType mode);
};

}  // namespace taraxa::network::tarcap
