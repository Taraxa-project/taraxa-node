#pragma once

#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa::network::tarcap {

class VotePacketHandler final : public ExtVotesPacketHandler {
 public:
  VotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                    std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                    const addr_t& node_addr);

  /**
   * @brief Sends pbft vote to connected peers
   *
   * @param vote Votes to send
   * @param block block to send - nullptr means no block
   * @param rebroadcast - send even of vote i known for the peer
   */
  void onNewPbftVote(const std::shared_ptr<Vote>& vote, const std::shared_ptr<PbftBlock>& block,
                     bool rebroadcast = false);
  void sendPbftVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<Vote>& vote,
                    const std::shared_ptr<PbftBlock>& block);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::VotePacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 private:
  const size_t kVotePacketSize{1};
  const size_t kExtendedVotePacketSize{3};
};

}  // namespace taraxa::network::tarcap
