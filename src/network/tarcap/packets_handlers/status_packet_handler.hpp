#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DagManager;
class NextVotesForPreviousRound;
class PbftManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class SyncingState;
class SyncingHandler;

class StatusPacketHandler : public PacketHandler {
 public:
  StatusPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                      std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<SyncingHandler> syncing_handler,
                      std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<DagManager> dag_mgr,
                      std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr, std::shared_ptr<PbftManager> pbft_mgr,
                      uint64_t conf_network_id, const addr_t& node_addr = {});

  bool sendStatus(const dev::p2p::NodeID& node_id, bool initial);
  void checkLiveness();

 private:
  void process(const dev::RLP& packet_rlp, const PacketData& packet_data, const std::shared_ptr<dev::p2p::Host>& host, const std::shared_ptr<TaraxaPeer>& peer) override;

  static constexpr uint16_t MAX_CHECK_ALIVE_COUNT = 5;
  static constexpr uint16_t INITIAL_STATUS_PACKET_ITEM_COUNT = 10;

  std::shared_ptr<SyncingState> syncing_state_;
  std::shared_ptr<SyncingHandler> syncing_handler_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<DagManager> dag_mgr_;
  std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr_;
  std::shared_ptr<PbftManager> pbft_mgr_;

  uint64_t conf_network_id_;
};

}  // namespace taraxa::network::tarcap
