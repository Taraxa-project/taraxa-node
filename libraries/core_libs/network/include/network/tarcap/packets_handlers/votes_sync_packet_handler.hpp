#pragma once

#include "network/tarcap/packets_handlers/common/ext_votes_packet_handler.hpp"

namespace taraxa {
class PbftManager;
class VoteManager;
class NextVotesManager;
class DbStorage;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class VotesSyncPacketHandler final : public ExtVotesPacketHandler {
 public:
  VotesSyncPacketHandler(std::shared_ptr<PeersState> peers_state, std::shared_ptr<PacketsStats> packets_stats,
                         std::shared_ptr<PbftManager> pbft_mgr, std::shared_ptr<PbftChain> pbft_chain,
                         std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<NextVotesManager> next_votes_mgr,
                         std::shared_ptr<DbStorage> db, const uint32_t dpos_delay, const addr_t& node_addr);

  void broadcastPreviousRoundNextVotesBundle();

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::VotesSyncPacket;

 private:
  void validatePacketRlpFormat(const PacketData& packet_data) const override;
  void process(const PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<NextVotesManager> next_votes_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
