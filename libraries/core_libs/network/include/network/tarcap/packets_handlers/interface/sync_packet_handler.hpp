#pragma once

#include "network/tarcap/packets_handlers/latest/common/ext_syncing_packet_handler.hpp"

namespace taraxa::network::tarcap {

class ISyncPacketHandler : public ExtSyncingPacketHandler {
 public:
  ISyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                     std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                     std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                     std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                     std::shared_ptr<DbStorage> db, const addr_t& node_addr, const std::string& logs_prefix);

  /**
   * @brief Start syncing pbft if needed
   *
   */
  void startSyncingPbft();

  /**
   * @brief Send sync request to the current syncing peer with specified request_period
   *
   * @param request_period
   *
   * @return true if sync request was sent, otherwise false
   */
  virtual bool syncPeerPbft(PbftPeriod request_period);

  void sendStatusToPeers();

  virtual bool sendStatus(const dev::p2p::NodeID& node_id, bool initial);

 private:
  const h256 kGenesisHash;
};

}  // namespace taraxa::network::tarcap
