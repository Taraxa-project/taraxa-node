#pragma once

#include "../latest/common/packet_handler.hpp"

namespace taraxa {
class PbftChain;
class DbStorage;
class VoteManager;
}  // namespace taraxa

namespace taraxa::network::tarcap {
  class PbftSyncingState;
}

namespace taraxa::network::tarcap::v3 {
class GetPbftSyncPacketHandler : public PacketHandler {
 public:
  GetPbftSyncPacketHandler(const FullNodeConfig& conf, std::shared_ptr<taraxa::network::tarcap::PeersState> peers_state,
                           std::shared_ptr<taraxa::network::tarcap::TimePeriodPacketsStats> packets_stats,
                           std::shared_ptr<PbftSyncingState> pbft_syncing_state, std::shared_ptr<PbftChain> pbft_chain,
                           std::shared_ptr<VoteManager> vote_mgr, std::shared_ptr<DbStorage> db,
                           const addr_t& node_addr, const std::string& logs_prefix = "GET_PBFT_SYNC_PH");

  // Packet type that is processed by this handler
  static constexpr SubprotocolPacketType kPacketType_ = SubprotocolPacketType::GetPbftSyncPacket;

 private:
  virtual void validatePacketRlpFormat(const threadpool::PacketData& packet_data) const override;
  virtual void process(const threadpool::PacketData& packet_data, const std::shared_ptr<TaraxaPeer>& peer) override;

  virtual void sendPbftBlocks(const std::shared_ptr<TaraxaPeer>& peer, PbftPeriod from_period,
                              size_t blocks_to_transfer, bool pbft_chain_synced);

 protected:
  std::shared_ptr<PbftSyncingState> pbft_syncing_state_;
  std::shared_ptr<PbftChain> pbft_chain_;
  std::shared_ptr<VoteManager> vote_mgr_;
  std::shared_ptr<DbStorage> db_;
};

}  // namespace taraxa::network::tarcap::v3
