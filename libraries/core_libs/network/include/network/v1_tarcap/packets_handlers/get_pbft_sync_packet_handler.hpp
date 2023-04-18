#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {
class PbftSyncingState;
}

namespace taraxa::network::v1_tarcap {

class GetPbftSyncPacketHandler final : public tarcap::PacketHandler {
 public:
  GetPbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<tarcap::PeersState> peers_state,
                           std::shared_ptr<tarcap::TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<tarcap::PbftSyncingState> pbft_syncing_state,
                           std::shared_ptr<PbftChain> pbft_chain, std::shared_ptr<VoteManager> vote_mgr,
                           std::shared_ptr<DbStorage> db, const addr_t& node_addr);

  void sendPbftBlocks(dev::p2p::NodeID const& peer_id, PbftPeriod from_period, size_t blocks_to_transfer,
                      bool pbft_chain_synced);

  // Packet type that is processed by this handler
  static constexpr tarcap::SubprotocolPacketType kPacketType_ = tarcap::SubprotocolPacketType::GetPbftSyncPacket;

 private:
  void validatePacketRlpFormat(const tarcap::PacketData& packet_data) const override;
  void process(const tarcap::PacketData& packet_data, const std::shared_ptr<tarcap::TaraxaPeer>& peer) override;

  std::shared_ptr<tarcap::PbftSyncingState> pbft_syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::v1_tarcap
