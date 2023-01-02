#pragma once

#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa::network::tarcap {

class VotesSyncPacketHandler final : public ExtVotesPacketHandler {
 public:
  VotesSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                         std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                         std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                         const addr_t& node_addr);

  /**
   * @brief Send all previous round next votes
   *
   * @param rebroadcast if rebroadcast is true, all votes are resent to all peers
   */
  void broadcastPreviousRoundNextVotesBundle(bool rebroadcast = false);

  /**
   * @brief Sends pbft votes bundle to connected peers
   *
   * @param votes Votes to send
   * @param rebroadcast if rebroadcast is true, all votes are resent to all peers
   * @param exclude_node do not send votes to excluded node
   */
  void onNewPbftVotesBundle(std::vector<std::shared_ptr<Vote>>&& votes, bool rebroadcast = false,
                            const std::optional<dev::p2p::NodeID>& exclude_node = {});

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::VotesSyncPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
