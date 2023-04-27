#pragma once

#include "common/ext_votes_packet_handler.hpp"

namespace taraxa {
class PbftManager;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap::v1 {

class GetNextVotesBundlePacketHandler final : public ExtVotesPacketHandler {
 public:
  GetNextVotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<tarcap::PeersState> peers_state,
                                  std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                                  std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                                  std::shared_ptr<VoteManager> vote_mgr, const addr_t& node_addr);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetNextVotesSyncPacket;

 private:
  void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  void process(const threadpool::PacketData& packet_data, const std::shared_ptr<tarcap::TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap::v1
