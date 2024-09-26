#pragma once

#include "common/ext_votes_packet_handler.hpp"
#include "network/tarcap/packets/votes_bundle_packet.hpp"

namespace taraxa::network::tarcap {

class VotesBundlePacketHandler : public ExtVotesPacketHandler<VotesBundlePacket> {
 public:
  VotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                           std::shared_ptr<SlashingManager> slashing_manager, const addr_t& node_addr,
                           const std::string& logs_prefix = "");

  /**
   * @brief Sends pbft votes bundle to connected peers
   *
   * @param votes Votes to send
   * @param rebroadcast if rebroadcast is true, all votes are resent to all peers
   * @param exclude_node do not send votes to excluded node
   */
  void onNewPbftVotesBundle(const std::vector<std::shared_ptr<PbftVote>>& votes, bool rebroadcast = false,
                            const std::optional<dev::p2p::NodeID>& exclude_node = {});

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kVotesBundlePacket;

 private:
  virtual void process(VotesBundlePacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
