#pragma once

#include "network/tarcap/packets/v4/vote_packet.hpp"
#include "network/tarcap/packets_handlers/latest/common/ext_votes_packet_handler.hpp"

namespace taraxa::network::tarcap::v4 {

class VotePacketHandler : public ExtVotesPacketHandler<v4::VotePacket> {
 public:
  VotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                    std::shared_ptr<TimePeriodPacketsStats> packets_stats, std::shared_ptr<PbftManager> pbft_mgr,
                    std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                    std::shared_ptr<SlashingManager> slashing_manager, const addr_t& node_addr,
                    const std::string& logs_prefix = "");

  /**
   * @brief Sends pbft vote to connected peers
   *
   * @param vote Votes to send
   * @param block block to send - nullptr means no block
   * @param rebroadcast - send even of vote i known for the peer
   */
  void onNewPbftVote(const std::shared_ptr<PbftVote>& vote, const std::shared_ptr<PbftBlock>& block,
                     bool rebroadcast = false);
  void sendPbftVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<PbftVote>& vote,
                    const std::shared_ptr<PbftBlock>& block);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kVotePacket;

 private:
  virtual void process(VotePacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap::v4
