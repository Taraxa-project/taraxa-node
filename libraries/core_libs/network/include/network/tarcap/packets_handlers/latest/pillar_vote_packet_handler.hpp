#pragma once

#include "network/tarcap/packets_handlers/latest/common/ext_pillar_vote_packet_handler.hpp"

namespace taraxa::network::tarcap {

class PillarVotePacketHandler : public ExtPillarVotePacketHandler {
 public:
  PillarVotePacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                          std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                          std::shared_ptr<pillar_chain::PillarChainManager> pillar_chain_manager,
                          const addr_t& node_addr, const std::string& logs_prefix);

  void onNewPillarVote(const std::shared_ptr<PillarVote>& vote, bool rebroadcast = false);
  void sendPillarVote(const std::shared_ptr<TaraxaPeer>& peer, const std::shared_ptr<PillarVote>& vote);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::PillarVotePacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;
};

}  // namespace taraxa::network::tarcap
