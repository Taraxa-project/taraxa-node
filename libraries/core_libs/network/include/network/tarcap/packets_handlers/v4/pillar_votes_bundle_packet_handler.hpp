#pragma once

#include "network/tarcap/packets_handlers/v4/common/ext_pillar_vote_packet_handler.hpp"

namespace taraxa::network::tarcap::v4 {

class PillarVotesBundlePacketHandler : public ExtPillarVotePacketHandler {
 public:
  PillarVotesBundlePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                                 std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                                 std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                                 const addr_t& node_addr, const std::string& logs_prefix);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kPillarVotesBundlePacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

 public:
  constexpr static size_t kMaxPillarVotesInBundleRlp{250};
};

}  // namespace taraxa::network::tarcap::v4
