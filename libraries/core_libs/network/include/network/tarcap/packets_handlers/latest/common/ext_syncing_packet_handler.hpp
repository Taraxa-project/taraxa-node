#pragma once

#include "dag/dag_manager.hpp"
#include "network/tarcap/shared_states/pbft_syncing_state.hpp"
#include "packet_handler.hpp"
#include "pbft/pbft_chain.hpp"
#include "pbft/pbft_manager.hpp"

namespace taraxa {
class PbftManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

/**
 * @brief ExtSyncingPacketHandler is extended abstract PacketHandler with added functions that are used in packet
 *        handlers that need to interact with syncing process in some way
 */
class ExtSyncingPacketHandler : public PacketHandler {
 public:
  ExtSyncingPacketHandler(const FullNodeConfig &conf, std::shared_ptr<PeersState> peers_state,
                          std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                          std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                          std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                          std::shared_ptr<DbStorage> db, const addr_t &node_addr, const std::string &log_channel_name);

  virtual ~ExtSyncingPacketHandler() = default;
  ExtSyncingPacketHandler &operator=(const ExtSyncingPacketHandler &) = delete;
  ExtSyncingPacketHandler &operator=(ExtSyncingPacketHandler &&) = delete;

  void requestDagBlocks(const dev::p2p::NodeID &_nodeID, std::vector<blk_hash_t> &&blocks, PbftPeriod period);
  void requestPendingDagBlocks(std::shared_ptr<TaraxaPeer> peer = nullptr);

 protected:
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_{nullptr};

  std::shared_ptr<PbftChain> pbft_chain_{nullptr};
  std::shared_ptr<PbftManager> pbft_mgr_{nullptr};
  std::shared_ptr<DagManager> dag_mgr_{nullptr};
  std::shared_ptr<DbStorage> db_{nullptr};
};

}  // namespace taraxa::network::tarcap
