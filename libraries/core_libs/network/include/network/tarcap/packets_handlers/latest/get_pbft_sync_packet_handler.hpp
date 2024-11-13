#pragma once

#include "common/packet_handler.hpp"
#include "network/tarcap/packets/latest/get_pbft_sync_packet.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {

class PbftSyncingState;

class GetPbftSyncPacketHandler : public PacketHandler<GetPbftSyncPacket> {
 public:
  GetPbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<PeersState> peers_state,
                           std::shared_ptr<TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                           std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                           const addr_t& node_addr, const std::string& logs_prefix = "GET_PBFT_SYNC_PH");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::kGetPbftSyncPacket;

 private:
  virtual void process(GetPbftSyncPacket&& packet, const std::shared_ptr<TaraxaPeer>& peer) override;

  virtual void sendPbftBlocks(const std::shared_ptr<TaraxaPeer>& peer, PbftPeriod from_period,
                              size_t blocks_to_transfer, bool pbft_chain_synced);

 protected:
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap
