#pragma once

#include "dag/dag_block.hpp"
#include "dag/dag_manager.hpp"
#include "packet_handler.hpp"

namespace taraxa {
class PbftChain;
class PbftManager;
class DagManager;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftSyncingState;

/**
 * @brief ExtSyncingPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that need to interact with syncing process in some way
 */
class ExtSyncingPacketHandler : public PacketHandler {
 public:
  ExtSyncingPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                          std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                          std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DbStorage> db, const addr_t &node_addr, const std::string &log_channel_name);

  virtual ~ExtSyncingPacketHandler() = default;
  ExtSyncingPacketHandler(const ExtSyncingPacketHandler &) = default;
  ExtSyncingPacketHandler(ExtSyncingPacketHandler &&) = default;
  ExtSyncingPacketHandler &operator=(const ExtSyncingPacketHandler &) = default;
  ExtSyncingPacketHandler &operator=(ExtSyncingPacketHandler &&) = default;

  /**
   * @brief Restart syncing
   *
   * @param shared_state
   * @param force
   */
  void restartSyncingPbft(bool force = false);

  /**
   * @brief Send sync request to the current syncing peer with specified request_period
   *
   * @param request_period
   * @param ignore_chain_size_check ignore peer's chain size check - it is used when processing sync packet from
   *                                current syncing peer as he specifies in packet if he already send his last block
   *                                or not. This info is more up to date then peer->chain_size that might have been
   * saved in the past and it is not valid anymore
   *
   * @return true if sync request was sent, otherwise false
   */
  bool syncPeerPbft(PbftPeriod request_period, bool ignore_chain_size_check = false);

  void requestDagBlocks(const dev::p2p::NodeID &_nodeID, const std::unordered_set<blk_hash_t> &blocks,
                        PbftPeriod period);
  void requestPendingDagBlocks(std::shared_ptr<TaraxaPeer> peer = nullptr);

  std::shared_ptr<TaraxaPeer> getMaxChainPeer();

 protected:
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_{nullptr};

  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<PbftManager> pbft_mgr_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DbStorage> db_{nullptr};
};

}  // namespace taraxa::network::tarcap
