#pragma once

#include "network/tarcap/packets_handlers/common/ext_syncing_packet_handler.hpp"

namespace taraxa {
class NextVotesForPreviousRound;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class StatusPacketHandler : public ExtSyncingPacketHandler {
 public:
  StatusPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                      std::shared_ptr<SyncingState> syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                      std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<DagManager> dag_mgr,
                      std::shared_ptr<DagBlockManager> dag_blk_mgr,
                      std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr, uint64_t conf_network_id,
                      const addr_t& node_addr);

  virtual ~StatusPacketHandler() = default;

  bool sendStatus(const dev::p2p::NodeID& node_id, bool initial);
  void sendStatusToPeers();

 private:
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  void requestPbftNextVotes(dev::p2p::NodeID const& peerID, uint64_t pbft_round,
                            size_t pbft_previous_round_next_votes_size);
  void syncPbftNextVotes(uint64_t pbft_round, size_t pbft_previous_round_next_votes_size);

  static constexpr uint16_t INITIAL_STATUS_PACKET_ITEM_COUNT = 10;
  const uint64_t conf_network_id_;

  std::shared_ptr<NextVotesForPreviousRound> next_votes_mgr_;
};

}  // namespace taraxa::network::tarcap
