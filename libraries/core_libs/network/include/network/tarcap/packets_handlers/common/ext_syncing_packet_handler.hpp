#pragma once

#include "dag/dag_block.hpp"
#include "packet_handler.hpp"

namespace taraxa {
class PbftChain;
class PbftManager;
class DagManager;
class DagBlockManager;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;

/**
 * @brief ExtSyncingPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that need to interact with syncing process in some way
 */
class ExtSyncingPacketHandler : public PacketHandler {
 public:
  ExtSyncingPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                          std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                          std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DagBlockManager> dag_blk_mgr, std::shared_ptr<DbStorage> db,
                          const addr_t &node_addr, const std::string &log_channel_name);

  virtual ~ExtSyncingPacketHandler() = default;

  /**
   * @brief Restart syncing
   *
   * @param shared_state
   * @param force
   */
  void restartSyncingPbft(bool force = false);

  bool syncPeerPbft(unsigned long height_to_sync);
  void requestDagBlocks(const dev::p2p::NodeID &_nodeID, const std::unordered_set<blk_hash_t> &blocks, uint64_t period);
  void requestPendingDagBlocks(std::shared_ptr<TaraxaPeer> peer = nullptr);

  std::pair<bool, std::unordered_set<blk_hash_t>> checkDagBlockValidation(const DagBlock &block) const;
  std::shared_ptr<TaraxaPeer> getMaxChainPeer();

 protected:
  std::shared_ptr<SyncingState> syncing_state_{nullptr};

  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<PbftManager> pbft_mgr_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DagBlockManager> dag_blk_mgr_{nullptr};
  std::shared_ptr<DbStorage> db_{nullptr};
};

}  // namespace taraxa::network::tarcap
