#pragma once

#include "network/tarcap/packets_handlers/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftSyncingState;

class GetPbftSyncPacketHandler final : public PacketHandler {
 public:
  GetPbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                           std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                           const addr_t& node_addr);

  void sendPbftBlocks(const std::shared_ptr<TaraxaPeer>& peer, PbftPeriod from_period, size_t blocks_to_transfer,
                      bool pbft_chain_synced);

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetPbftSyncPacket;

 private:
  void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
