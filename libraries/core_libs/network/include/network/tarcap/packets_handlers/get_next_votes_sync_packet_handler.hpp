#pragma once

#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa {
class PbftManager;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class GetNextVotesBundlePacketHandler final : public ExtVotesPacketHandler {
 public:
  GetNextVotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                  std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                  std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                  std::shared_ptr<VoteManager> vote_mgr, const addr_t& node_addr);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetNextVotesSyncPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
