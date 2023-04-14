#pragma once

#include "network/v1_tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa {
class PbftManager;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::v1_tarcap {

class GetNextVotesSyncPacketHandler final : public ExtVotesPacketHandler {
 public:
  GetNextVotesSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<tarcap::PeersState> peers_state,
                                std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                                std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                std::shared_ptr<VoteManager> vote_mgr, const addr_t& node_addr);

  // Packet type that is processed by this handler
  static constexpr tarcap::SubprotocolPacketType kPacketType_ = tarcap::SubprotocolPacketType::GetNextVotesSyncPacket;

 private:
  void validatePacketRlpFormat(const tarcap::PacketData& packet_data) const override;
  void process(const tarcap::PacketData& packet_data, const std::shared_ptr<tarcap::TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::v1_tarcap
